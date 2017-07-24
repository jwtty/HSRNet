#include "util.h"

static char usage[] = 
    "  -B [local ip]:\n"
    "    Specify the target ip address.\n"
    "    *: Required\n"
    "  -c [server ip]:\n"
    "    Specify the source ip address.\n"
    "    *: Required\n"
    "  -l [path]:\n"
    "    Specify the file to save the log.\n"
    "  -n [size]:\n"
    "    Tell the program to do fix test whose size is [size] bytes.\n"
    "    (if the -n flag is specified, the program will perform a fix test)\n"
    "  -p [port]:\n"
    "    Specify port number of server.\n"
    "    *: Required\n"
    "  -P [cport]:\n"
    "    Specify port number of controller(default port + 2).\n"
    "  -t [time]:\n"
    "    Tell the program to do long test whose length is [time] seconds,\n"
    "    or set the timeout threshold of fix test if -n is specified.\n"
    "    *: Required\n"
    "  -v:\n"
    "    Print version information and exit.\n"
    "  -V:\n"
    "    Print verbose log.";

static char *localIP = NULL;
static char *serverIP = NULL;
static unsigned short port = 0;
static unsigned short cport = 0;
static int timelen = -1;
static int size = -1;
static char *path = NULL;
static int connfd = -1;
static long byteReceived = 0;
static double timeElapsed = 0;
static int mss = 0;
static char recvBuf[67108864];

static void printUsageAndExit(char **argv)
{
    printf("Usage: %s [options]\n%s\n", argv[0], usage);
    exit(0);
}

static void printVersionAndExit()
{
    printf("Current version: %s\n", VERSION);
    exit(0);
}

static void parseArguments(int argc, char **argv)
{
    int os;
    char c;
    while ((c = getopt(argc, argv, "B:c:p:t:n:l:P:vV")) != EOF)
    {
        switch (c)
        {
        case 'B':
            localIP = optarg;
            break;
        case 'c':
            serverIP = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'P':
            cport = atoi(optarg);
            break;
        case 't':
            timelen = atoi(optarg);
            break;
        case 'n':
            size = atoi(optarg);
            break;
        case 'l':
            path = optarg;
            break;
        case 'v':
            printVersionAndExit();
            break;
        case 'V':
            verboseOn();
            break;
        default:
            logWarning("Unexpected command-line option!");
            printUsageAndExit(argv);
        }
    }

    if (cport == 0)
    {
        cport = port + 2;
    }

    if (localIP == NULL)
    {
        logFatal("No local IP specified.\n");
    }
    if (serverIP == NULL)
    {
        logFatal("No server IP specified.\n");
    }
    if (port == 0)
    {
        logFatal("No legal port number specified.\n");
    }
    if (timelen < 0)
    {
        logFatal("No legal -t argument specified.");
    }
    if (path != NULL)
    {
        if ((os = open(path, O_APPEND | O_WRONLY | O_CREAT, 0666) < 0))
        {
            logFatal("Can't open log file %s.", path);
        }
        else
        {
            dup2(os, 2);
        }
    }
}

static inline void rRecvRetval(int connfd, const char *ope, char *ret)
{
    static char text[256];
    if (rio_readn_force(connfd, ret, 1) < 1)
    {
        logFatal("%s: Can't receive return value!", ope);
    }
    if (*ret != RET_SUCC)
    {
        logFatal("%s failed(%s)!", ope, retstr(*ret, text));
    }
    
    logMessage("%s complete.", ope);
}

static void reconfigureServer()
{
    static char message[1024];
    int type = size > 0 ? TYPE_FIX : TYPE_LONG;
    int arg = timelen;
    int arg2 = size;
    char ret;

    // terminate any running job firstly.
    if ((connfd = netdial(
        AF_INET, SOCK_STREAM, localIP, 0, serverIP, cport)) < 0)
    {
        logFatal("Can't connect to controller!");
    }
    *message = SIG_TERM;
    if (rio_writen(connfd, message, 1) < 0)
    {
        logFatal("Can't send instruction to controller!");
    }
    
    rRecvRetval(connfd, "Terminate", &ret);
    close(connfd);

    // reconfigure now.
    if ((connfd = netdial(
        AF_INET, SOCK_STREAM, localIP, 0, serverIP, cport)) < 0)
    {
        logFatal("Can't connect to controller!");
    }

    *message = SIG_CONF;
    if (rio_writen(connfd, message, 1) < 0)
    {
        logFatal("Can't send instruction to controller!");
    }
    sprintf(message, "%d %d %d", (int)type, arg, arg2);
    logVerbose("Control message: %s", message);
    rSendMessage(connfd, "controller", message, 1 + strlen(message));

    rRecvRetval(connfd, "Reconfigure", &ret);
    close(connfd);
}

static void connectToServer()
{
    socklen_t socklen = sizeof(mss);
    if ((connfd = netdial(
        AF_INET, SOCK_STREAM, localIP, 0, serverIP, port)) < 0)
    {
        logFatal("Can't connect to server!");
    }
    if (getsockopt(connfd, IPPROTO_TCP, 2, &mss, &socklen) < 0)
    {
        logFatal("Failed to get MSS.");
    }

    logMessage("Connection established, mss is %d.", mss);
}

static void doReceive()
{
    int ret;
    struct timeval st, ed;
    gettimeofday(&st, NULL);
    do
    {
        ret = rio_readn(connfd, recvBuf, PACKET_LEN);

        if (ret < 0)
        {
            gettimeofday(&ed, NULL);
            if (errno == EPIPE || errno == ECONNRESET)
            {
                logWarning("Connection broken(%s).", strerror(errno));
            }
            else 
            {
                logError("Unexpected read error(%s)!", strerror(errno));
            }
            break;
        }
        byteReceived += ret;
    }
    while (ret == PACKET_LEN);

    if (ret >= 0)
    {
        gettimeofday(&ed, NULL);
    }
    timeElapsed = 
        (ed.tv_sec - st.tv_sec) + (ed.tv_usec - st.tv_usec) / 1000000.0;
    logMessage("Transfer complete.");
}

static void dumpResult()
{
    logMessage("Test summary:");
    logMessage("  Total time: %lfs", timeElapsed);
    logMessage("  Bytes received: %ld", byteReceived);
    logMessage("  Bandwidth: %lfBytes/sec", byteReceived / timeElapsed);
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printUsageAndExit(argv);
    }

    Signal(SIGPIPE, SIG_IGN);

    parseArguments(argc, argv);

    reconfigureServer();

    connectToServer();
    doReceive();
    
    dumpResult();

    return 0;
}