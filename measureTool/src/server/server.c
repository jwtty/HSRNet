#include "util.h"

static lock_t sigusr1;
static lock_t sigusr2;
static lock_t sigalrm;
static sigset_t mask, backup;

static char type = TYPE_FIX;
static int arg = 1024;
static int arg2 = 200;
static char packetBuf[PACKET_LEN];

static int running = 0;

void sigHandler1(int sig)
{
    sigusr1 = 1;
    sigprocmask(SIG_BLOCK, &mask, &backup);

    if (running == 0)
    {
        int pid;

        logMessage("No test running, bypass.");
        pid = peekPID(SMEM_CONTROLLERPID);
        rKill(pid, "controller", SIGUSR1);
    }

    sigprocmask(SIG_SETMASK, &backup, NULL);
}

void sigHandler2(int sig)
{
    static char message[256];
    int targ, ttype, targ2;
    int pid;
    sigprocmask(SIG_BLOCK, &mask, &backup);
    
    sigusr2 = 1;
    logMessage("Trying to reconfigure server.");
    if (getMessage(SMEM_MESSAGE, message) < 0)
    {
        logError("Can't receive arguments: shared memory not initialized.");
        goto sighandler2_fail_out;
    }

    pid = peekPID(SMEM_CONTROLLERPID);

    sscanf(message, "%d%d%d", &ttype, &targ, &targ2);
    switch (ttype)
    {
    case TYPE_LONG:
        arg = targ;
        type = (char)ttype;
        logMessage("Reconfigured with type = long, arg = %d", targ);
        break;
    case TYPE_FIX:
        arg = targ;
        arg2 = targ2;
        type = (char)ttype;
        logMessage("Reconfigured with type = fix, arg = %d", targ);
        break;
    default:
        sprintf(message, "Unrecognized type %d", ttype);
        logWarning("%s", message);
        setMessage(SMEM_MESSAGE, message);
        goto sighandler2_fail_out;
    }

    goto sighandler2_out;

sighandler2_fail_out:
    rKill(pid, "controller", SIGUSR2);
    goto sighandler2_final;
sighandler2_out:
    rKill(pid, "controller", SIGUSR1);
sighandler2_final:
    sigprocmask(SIG_SETMASK, &backup, NULL);
}

void sigHandler3(int sig)
{
    sigalrm = 1;
}

void doLongTest(int connfd)
{
    struct timeval st, ed;
    int sum = 0;
    double elapsed;

    init_lock(&sigalrm);
    init_lock(&sigusr1);
    alarm(arg);

    gettimeofday(&st, NULL);

    while (!sigalrm)
    {
        int wrote = rio_writenr(connfd, packetBuf, PACKET_LEN);
        if (wrote < 0)
        {
            logError("Error uccured when sending packets(%s)!", 
                strerror(errno));
            break;
        }
        else if (wrote < PACKET_LEN)
        {
            if (sigusr1)
            {
                int pid;
                logMessage("Long test terminated.");

                pid = peekPID(SMEM_CONTROLLERPID);
                rKill(pid, "controller", SIGUSR1);
            }
            else if (!sigalrm)
            {
                logWarning("Send unexpectedly interrupted by signal.");
            }

            sum += wrote;
            break;
        }
        sum += wrote;
    }

    gettimeofday(&ed, NULL);
    elapsed = (ed.tv_sec - st.tv_sec) + (ed.tv_usec - st.tv_usec) / 1000000.0;

    alarm(0);
    logMessage("Long test summary:");
    logMessage("  Bytes transferred: %d", sum);
    logMessage("  Time elapsed : %lfs", elapsed);
}

void doFixTest(int connfd)
{
    int targ = arg2;
    int len = arg2;
    struct timeval st, ed;
    double elapsed;

    init_lock(&sigalrm);
    init_lock(&sigusr1);
    alarm(arg);
    
    // it's a viutual syscall on x64, so we assume it costs 
    // less than 1us.
    gettimeofday(&st, NULL);

    while (len > 0)
    {
        int thislen = len > PACKET_LEN ? PACKET_LEN : len;
        int wrote = rio_writenr(connfd, packetBuf, thislen);
        if (wrote < 0)
        {
            logError("Error uccured when sending packets(%s)!", 
                strerror(errno));
            break;
        }
        else if (wrote < thislen)
        {
            if (sigusr1)
            {
                int pid;
                logMessage("Long test terminated.");

                pid = peekPID(SMEM_CONTROLLERPID);
                rKill(pid, "controller", SIGUSR1);
            }
            else if (!sigalrm)
            {
                logWarning("Send unexpectedly interrupted by signal.");
            }
            else
            {
                logWarning("Fix test timeout.");
            }
            len -= wrote;
            break;
        }
        len -= wrote;
    }

    gettimeofday(&ed, NULL);
    elapsed = (ed.tv_sec - st.tv_sec) + (ed.tv_usec - st.tv_usec) / 1000000.0;

    logMessage("Fix test summary:");
    logMessage("  Bytes to transfer: %d", targ);
    logMessage("  Bytes transferred: %d", targ - len);
    logMessage("  Time elapsed : %lfs", elapsed);
}

void parse(int connfd)
{
    running = 1;

    switch (type)
    {
    case TYPE_LONG:
        doLongTest(connfd);
        break;
    case TYPE_FIX:
        doFixTest(connfd);
        break;
    default:
        logWarning("Unrecognized type %d.", (int)type);
    }
    running = 0;
}

int main(int argc, char **argv)
{
    int listenfd, port;
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(1);
    }
    
    sigfillset(&mask);
    notifyPID(SMEM_SERVERPID, getpid());

    port = atoi(argv[1]);
    listenfd = Open_listenfd(port);
    logMessage("Listening on port %d.", port);

    Signal(SIGUSR1, sigHandler1);
    Signal(SIGUSR2, sigHandler2);
    Signal(SIGALRM, sigHandler3);
    Signal(SIGPIPE, SIG_IGN);

    while (1)
    {
        unsigned int clientlen;
        int connfd;
        struct sockaddr_in clientaddr;
        char *haddrp;

        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (connfd < 0)
        {
            logError("Unable to accept connection(%s)!", 
                strerror(errno));
        }

        haddrp = inet_ntoa(clientaddr.sin_addr);
        logMessage("Connected to %s\n", haddrp);

        parse(connfd);
        if (close(connfd) < 0)
        {
            logWarning("Error when closing connection(%s).", 
                strerror(errno));
        }
        logMessage("Connection with %s closed.", haddrp);
    }
}