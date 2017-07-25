#include "util.h"

char *usage = 
    "  -h:\n"
    "    Print this message and exit.\n"
    "  -l [path]:\n"
    "    Specify the file to save the log.\n"
    "  -p [port]:\n"
    "    Specify port number of server.\n"
    "    *: Required\n"
    "  -v:\n"
    "    Print version information and exit.\n"
    "  -V:\n"
    "    Print verbose log.";

static lock_t sigusr1;
static lock_t sigusr2;
static lock_t sigalrm;

static char type = TYPE_FIX;
static int arg = 1024;
static int arg2 = 200;
static char packetBuf[PACKET_LEN];

static int running = 0;

static int port;
static char *path;

void sigusr1Handler(int sig)
{
    sigusr1 = 1;
    logMessage("Trying to terminate running job.");

    if (running == 0)
    {
        int pid;

        logMessage("No test running, bypass.");
        if (rPeekPID(SMEM_CONTROLLERPID, "controller", &pid) == RET_SUCC)
        {
            rKill(pid, "controller", SIGUSR1);
        }
    }

    logMessage("Terminate complete.");
}

void sigusr2Handler(int sig)
{
    static char message[256];
    int targ, ttype, targ2;
    int pid = -1;
    int sigret = 0;
    
    sigusr2 = 1;
    logMessage("Trying to reconfigure server.");
    if (getMessage(SMEM_MESSAGE, message) < 0)
    {
        logError("Can't receive arguments: shared memory not initialized.");
        goto sigusr2Handler_fail_out;
    }

    if (rPeekPID(SMEM_CONTROLLERPID, "controller", &pid) != RET_SUCC)
    {
        goto sigusr2Handler_final;
    }

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
        goto sigusr2Handler_fail_out;
    }

    goto sigusr2Handler_out;

sigusr2Handler_fail_out:
    sigret = SIGUSR2;
    goto sigusr2Handler_final;
sigusr2Handler_out:
    sigret = SIGUSR1;
sigusr2Handler_final:
    rKill(pid, "controller", sigret);
}

void sigalrmHandler(int sig)
{
    sigalrm = 1;
    logVerbose("Alarm!");
}

void doLongTest(int connfd)
{
    struct timeval st, ed;
    long sum = 0;
    double elapsed;

    init_lock(&sigalrm);
    init_lock(&sigusr1);
    alarm(arg);

    gettimeofday(&st, NULL);

    while (!sigalrm)
    {
        int wrote = rio_writenr(connfd, packetBuf, PACKET_LEN);
        if (wrote < PACKET_LEN)
        {
            if (wrote < 0)
            {
                logError("Error occured when sending packets(%s)!", 
                    strerror(errno));
                break;
            }
            else if (sigusr1)
            {
                int pid;
                logMessage("Long test terminated.");

                if (rPeekPID(SMEM_CONTROLLERPID, "controller", &pid) 
                    == RET_SUCC)
                {
                    rKill(pid, "controller", SIGUSR1);
                }
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
    logMessage("  Bytes transferred: %ld", sum);
    logMessage("  Time elapsed : %lfs", elapsed);
    logMessage("  Bandwidth: %lfBytes/sec", sum / elapsed);
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
        
        if (wrote < thislen)
        {
            if (wrote < 0)
            {
                logError("Error occured when sending packets(%s)!", 
                    strerror(errno));
                break;
            }
            else if (sigusr1)
            {
                int pid;
                logMessage("Long test terminated.");

                if (rPeekPID(SMEM_CONTROLLERPID, "controller", &pid) 
                    == RET_SUCC)
                {
                    rKill(pid, "controller", SIGUSR1);
                }
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
    alarm(0);
    elapsed = (ed.tv_sec - st.tv_sec) + (ed.tv_usec - st.tv_usec) / 1000000.0;

    logMessage("Fix test summary:");
    logMessage("  Bytes to transfer: %d", targ);
    logMessage("  Bytes transferred: %d", targ - len);
    logMessage("  Bandwidth: %lfBytes/sec", (targ - len) / elapsed);
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

static void parseArguments(int argc, char **argv)
{
    char c;
    while ((c = getopt(argc, argv, "p:vVl:h")) != EOF)
    {
        switch (c)
        {
        case 'h':
            printUsageAndExit(argv);
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'l':
            path = optarg;
            break;
        case 'v':
            printVersionAndExit("mperf-server");
            break;
        case 'V':
            verboseOn();
            break;
        default:
            logWarning("Unexpected command-line option!");
            printUsageAndExit(argv);
        }
    }

    if (port == 0)
    {
        logFatal("No legal port number specified.\n");
    }
    if (path != NULL)
    {
        redirectLogTo(path);
    }
}

int main(int argc, char **argv)
{
    int listenfd;
    if (argc == 1)
    {
        printUsageAndExit(argv);
    }

    initTimestamp();
    parseArguments(argc, argv);
    printInitTimestamp();

    SignalNoRestart(SIGALRM, sigalrmHandler);
    SignalNoRestart(SIGUSR1, sigusr1Handler);
    SignalNoRestart(SIGUSR2, sigusr2Handler);
    SignalNoRestart(SIGPIPE, SIG_IGN);

    initSharedMem(SMEM_MESSAGE);
    initSharedMem(SMEM_CONTROLLERPID);
    initSharedMem(SMEM_SERVERPID);
    notifyPID(SMEM_SERVERPID, getpid());

    listenfd = Open_listenfd(port);
    logMessage("Listening on port %d.", port);

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
            continue;
        }

        haddrp = inet_ntoa(clientaddr.sin_addr);
        logMessage("Connected to %s", haddrp);

        parse(connfd);
        if (close(connfd) < 0)
        {
            logWarning("Error when closing connection(%s).", 
                strerror(errno));
        }
        logMessage("Connection with %s closed.\n", haddrp);
    }

    return 0;
}