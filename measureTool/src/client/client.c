#include "util.h"

static char usage[] = 
    "  -B [local ip]:\n"
    "    Specify the target ip address.\n"
    "    *: Required\n"
    "  -c [server ip]:\n"
    "    Specify the source ip address.\n"
    "    *: Required\n"
    "  -p [port]:\n"
    "    Specify port number of server.\n"
    "    *: Required\n"
    "  -P [cport]:\n"
    "    Specify port number of controller(default port + 2).\n"
    "  -t [time]:\n"
    "    Tell the program to do long test whose length is [time] seconds,\n"
    "    or set the timeout threshold of fix test if -n is specified.\n"
    "    *: Required\n"
    "  -n [size]:\n"
    "    Tell the program to do fix test whose size is [size] bytes.\n"
    "    (if the -n flag is specified, the program will perform a fix test)\n"
    "  -l [path]:\n"
    "    Specify the file to save the log.";

static char *localIP = NULL;
static char *serverIP = NULL;
static unsigned short port = 0;
static unsigned short cport = 0;
static int timelen = -1;
static int size = -1;
static char *path = NULL;
static int connfd = -1;
static int byteReceived = 0;
static double timeElapsed = 0;
static int mss = 0;
static char recvBuf[65536];

void parseArguments(int argc, char **argv)
{
    int os;
    char c;
    while ((c = getopt(argc, argv, "B:c:p:t:n:l:")) != EOF)
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
        default:
            logWarning("Unexpected command-line option!");
            printf("Usage: %s [options]\n%s", argv[0], usage);
            exit(1);
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

static inline void rRecvRetval(int connfd, char *ret)
{
    static char text[256];
    if (rio_readn_force(connfd, ret, 1) < 1)
    {
        logFatal("Can't receive reconfigure return value!");
    }
    if (*ret != RET_SUCC)
    {
        logFatal("Reconfigure failed(%s)!", retstr(*ret, text));
    }
    
    logMessage("Reconfigure complete.");
}

static void reconfigureServer()
{
    static char message[1024];
    int type = size > 0 ? TYPE_FIX : TYPE_LONG;
    int arg = timelen;
    int arg2 = size;
    char ret;
    if ((connfd = netdial(
        AF_INET, SOCK_STREAM, localIP, 0, serverIP, cport)) < 0)
    {
        logFatal("Can't connect to controller!");
    }

    sprintf(message, "%d %d %d", (int)type, arg, arg2);
    rSendMessage(connfd, "controller", message);

    rRecvRetval(connfd, &ret);
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
    logMessage("Test result:");
    logMessage("  Total time: %lf", timeElapsed);
    logMessage("  Bytes received: %d", byteReceived);
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printf("Usage: %s [options]\n%s", argv[0], usage);
        exit(1);
    }

    Signal(SIGPIPE, SIG_IGN);

    parseArguments(argc, argv);

    reconfigureServer();

    connectToServer();
    doReceive();
    
    dumpResult();

    return 0;
}