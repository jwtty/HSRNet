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

static int port;
static char *path;

static inline void rSendRetval(int connfd, char *ret)
{
    if (rio_writen(connfd, ret, 1) < 0)
    {
        logError("Can't send return value to client(%s)!", strerror(errno));
    }
}

void sigHandler1(int sig)
{
    sigusr1 = 1;
}

void sigHandler2(int sig)
{
    sigusr2 = 1;
}

static void do_terminate(int connfd)
{
    int pid;
    char ret;

    if ((ret = rPeekPID(SMEM_SERVERPID, "server", &pid)) != RET_SUCC)
    {
        goto do_terminate_out;
    }

    init_lock(&sigusr1);
    if ((ret = rKill(pid, "server", SIGUSR1)) != RET_SUCC)
    {
        goto do_terminate_out;
    }

    spin(&sigusr1);

do_terminate_out:
    rSendRetval(connfd, &ret);
    if (ret == RET_SUCC)
    {
        logMessage("Terminate completed.");
    }
    else
    {
        logWarning("Terminate failed.");
    }
}

static void do_configure(int connfd)
{
    int pid;
    char ret;
    char message[1024];

    if ((ret = rPeekPID(SMEM_SERVERPID, "server", &pid)) != RET_SUCC)
    {
        goto do_configure_out;
    }

    if ((ret = rReceiveMessage(connfd, "client", message)) != RET_SUCC)
    {
        goto do_configure_out;
    }
    setMessage(SMEM_MESSAGE, message);

    init_lock(&sigusr1);
    init_lock(&sigusr2);
    if ((ret = rKill(pid, "server", SIGUSR2)) != RET_SUCC)
    {
        goto do_configure_out;
    }

    spinOR(&sigusr1, &sigusr2);

    if (sigusr2 != 0)
    {
        getMessage(SMEM_MESSAGE, message);
        ret = RET_EMSG;
        goto do_configure_out;
    }

do_configure_out:
    rSendRetval(connfd, &ret);
    if (ret == RET_EMSG)
    {
        logError("%s", message);
        rSendMessage(connfd, "client", message, strlen(message) + 1);
    }
    if (ret == RET_SUCC)
    {
        logMessage("Reconfigure completed.");
    }
    else
    {
        logWarning("Reconfigure failed.");
    }
}

static void parse(int connfd)
{
    char c;
    if (rio_readn_force(connfd, &c, 1) == 1)
    {
        switch (c)
        {
        case SIG_TERM:
            logMessage("Instuction Terminate received.");
            do_terminate(connfd);
            break;
        case SIG_CONF:
            logMessage("Instuction Reconfigure received.");
            do_configure(connfd);
            break;
        default:
            logWarning("Unrecognized instruction %d.", (int)c);
        }
    }
    else
    {
        c = RET_EREAD;
        rSendRetval(connfd, &c);
    }
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
            printVersionAndExit("mperf-controller");
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

    SignalNoRestart(SIGUSR1, sigHandler1);
    SignalNoRestart(SIGUSR2, sigHandler2);
    SignalNoRestart(SIGPIPE, SIG_IGN);

    initSharedMem(SMEM_MESSAGE);
    initSharedMem(SMEM_CONTROLLERPID);
    initSharedMem(SMEM_SERVERPID);
    notifyPID(SMEM_CONTROLLERPID, getpid());

    listenfd = Open_listenfd(port);
    logMessage("Listening on port %d.", port);

    // initialize complete, start main loop
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