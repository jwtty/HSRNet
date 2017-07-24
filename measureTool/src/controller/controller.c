#include "util.h"

static inline void rSendRetval(int connfd, char *ret)
{
    if (rio_writen(connfd, ret, 1) < 0)
    {
        logError("Can't send return value to client(%s)!", strerror(errno));
    }
}

static lock_t sigusr1;
static lock_t sigusr2;

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

    pid = peekPID(SMEM_SERVERPID);

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

    pid = peekPID(SMEM_SERVERPID);

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
        rSendMessage(connfd, "client", message);
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

int main(int argc, char **argv)
{
    int listenfd, port;
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(1);
    }

    initSharedMem(SMEM_MESSAGE);
    initSharedMem(SMEM_CONTROLLERPID);
    initSharedMem(SMEM_SERVERPID);
    notifyPID(SMEM_CONTROLLERPID, getpid());

    port = atoi(argv[1]);
    listenfd = Open_listenfd(port);
    logMessage("Listening on port %d.", port);

    Signal(SIGUSR1, sigHandler1);
    Signal(SIGUSR2, sigHandler2);
    Signal(SIGPIPE, SIG_IGN);

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
        logMessage("Connected to %s\n", haddrp);

        parse(connfd);
        if (close(connfd) < 0)
        {
            logWarning("Error when closing connection(%s).", 
                strerror(errno));
        }
        logMessage("Connection with %s closed.", haddrp);
    }

    return 0;
}