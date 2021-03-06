#include "util.h"

char *usage = 
    "  -B [local ip]:\n"
    "    Specify the target ip address.\n"
    "    *: Required\n"
    "  -c [server ip]:\n"
    "    Specify the source ip address.\n"
    "    *: Required\n"
    "  -h:\n"
    "    Print this message and exit.\n"
    "  -l [path]:\n"
    "    Specify the file to save the log.\n"
    "  -n [size]:\n"
    "    Tell the program to do fix test with size=[size]Bytes.\n"
    "  -p [port]:\n"
    "    Specify port number of server.\n"
    "    *: Required\n"
    "  -P [cport]:\n"
    "    Specify port number of controller(default value is [port] + 2).\n"
    "  -t [time]:\n"
    "    Tell the program to do long test with timeLength=[time]seconds.\n"
    "  -T [Time]:\n"
    "    End test and exit after [Time] seconds.\n"
    "      for long tests, default value is [time] + 10.\n"
    "      for fix tests, default value is 200.\n"
    "  -v:\n"
    "    Print version information and exit.\n"
    "  -V:\n"
    "    Print verbose log.";

static char *localIP = NULL;
static char *serverIP = NULL;
static unsigned short port = 0;
static unsigned short cport = 0;
static int timelen = -1;
static int localTime = -1;
static int size = -1;
static char *path = NULL;
static int connfd = -1;
static long byteReceived = 0;
static double timeElapsed = 0;
static int mss = 0;
static char recvBuf[67108864];
static lock_t sigint = 0;
static lock_t sigalrm = 0;
static struct timeval st, ed;

static void parseArguments(int argc, char **argv)
{
    char c;
    while ((c = getopt(argc, argv, "B:c:p:t:n:l:P:vVT:h")) != EOF)
    {
        switch (c)
        {
        case 'B':
            localIP = optarg;
            break;
        case 'c':
            serverIP = optarg;
            break;
        case 'h':
            printUsageAndExit(argv);
            break;
        case 'l':
            path = optarg;
            break;
        case 'n':
            size = atoi(optarg);
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
        case 'T':
            localTime = atoi(optarg);
            break;
        case 'v':
            printVersionAndExit("mperf-client");
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
    if (localTime < 0)
    {
        if (size > 0)
        {
            localTime = 200;
        }
        else
        {
            localTime = timelen + 10;
        }
    }

    if (localIP == NULL)
    {
        logFatal("No local IP specified.");
    }
    if (serverIP == NULL)
    {
        logFatal("No server IP specified.");
    }
    if (port == 0)
    {
        logFatal("No legal port number specified.");
    }
    if (timelen < 0 && size < 0)
    {
        logFatal("No legal -t or -n argument specified.");
    }
    if (path != NULL)
    {
        redirectLogTo(path);
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
    int arg = size > 0 ? localTime + 10 : timelen;
    int arg2 = size;
    char ret;

    logVerbose("Trying to terminate early jobs...");
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

    logVerbose("Trying to reconfigure the server...");
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

static void dumpResult()
{
    timeElapsed = 
        (ed.tv_sec - st.tv_sec) + (ed.tv_usec - st.tv_usec) / 1000000.0;
    logMessage("Test summary:");
    logMessage("  Total time: %lfs", timeElapsed);
    logMessage("  Bytes received: %ld", byteReceived);
    logMessage("  Bandwidth: %lfBytes/sec", byteReceived / timeElapsed);
}

static void doReceive()
{
    int ret;

    logVerbose("Timeout threshold is %d", localTime);
    alarm(localTime);
    gettimeofday(&st, NULL);
    do
    {
        if ((ret = rio_readnr(connfd, recvBuf, PACKET_LEN)) < PACKET_LEN)
        {
            if (ret < 0)
            {
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    logWarning("Connection broken(%s).", strerror(errno));
                }
                else 
                {
                    logError("Unexpected read error(%s)!", strerror(errno));
                }
            }
            else
            {
                if (errno == EINTR)
                {
                    if (sigint)
                    {
                        logMessage("Ctrl+C received, terminate.");
                    }
                    else if (sigalrm)
                    {
                        logMessage("Test timeout after %d seconds, terminate.",
                            localTime);
                    }
                    else
                    {
                        logError("Interrupted by unexpected signal!");
                    }
                }
                byteReceived += ret;
            }
            break;
        }
        byteReceived += ret;
    }
    while (!sigint && !sigalrm);

    gettimeofday(&ed, NULL);
    alarm(0);
    
    dumpResult();
    logVerbose("SIGINT: %d, SIGALRM: %d", sigint, sigalrm);
    logMessage("Transfer complete.");
}

void sigintHandler(int sig)
{
    sigint = 1;
}

void sigalrmHandler(int sig)
{
    sigalrm = 1;
    logVerbose("Alarm!");
    alarm(1);
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printUsageAndExit(argv);
    }

    initTimestamp();

    SignalNoRestart(SIGINT, sigintHandler);
    SignalNoRestart(SIGALRM, sigalrmHandler);
    SignalNoRestart(SIGPIPE, SIG_IGN);

    parseArguments(argc, argv);
    printInitTimestamp();

    reconfigureServer();

    connectToServer();
    doReceive();

    return 0;
}