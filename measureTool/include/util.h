#ifndef __UTIL_H__
#define __UTIL_H__

#include "csapp.h"
#include "log.h"

#define PACKET_LEN 131072

#define SHARED_BLOCK_LEN 4096
#define SMEM_SERVERPID 0
#define SMEM_CONTROLLERPID 1
#define SMEM_MESSAGE 2

#define SIG_TERM 0
#define SIG_CONF 1

#define RET_SUCC 0
#define RET_EMSG 1
#define RET_EKILL 2
#define RET_EREAD 3
#define RET_EWRITE 4
#define RET_EPID 5

#define TYPE_LONG 0
#define TYPE_FIX 1

#define VERSION "1.0.0038"

extern char *usage;
static inline void printUsageAndExit(char **argv)
{
    fprintf(stderr, "Usage: %s [options]\n%s\n", argv[0], usage);
    exit(0);
}

static inline void printVersionAndExit()
{
    fprintf(stderr, "Current version: %s\n", VERSION);
    exit(0);
}

int netdial(int domain, int proto, char *local, int local_port, 
    char *server, int port);

void initSharedMem(int index);
void notifyPID(int index, int pid);
int peekPID(int index);

void setMessage(int index, char *message);
int getMessage(int index, char *dest);

char *retstr(char ret, char *buf);

handler_t *SignalNoRestart(int signum, handler_t handler);

typedef volatile int lock_t;
static inline void init_lock(lock_t *plock)
{
    *plock = 0;
}
static inline void spin(lock_t *plock)
{
    logVerbose("Waiting for spin lock...");
    while (*plock == 0);
}
static inline void spinOR(lock_t *plock1, lock_t *plock2)
{
    logVerbose("Waiting for spin lock...");
    while (*plock1 == 0 && *plock2 == 0);
}

static inline int rio_readn_force(int connfd, char *buf, int len)
{
    int ret = rio_readn(connfd, buf, len);
    if (ret < 0)
    {
        logError("Read error(%s).", strerror(errno));
    }
    else if (ret < len)
    {
        logWarning("EOF reached.");
    }
    return ret;
}

static inline char rPeekPID(int index, const char *name, int *dest)
{
    int pid = peekPID(index);
    if (pid == 0)
    {
        logError("Can't get PID of %s!", name);
        return RET_EPID;
    }
    *dest = pid;
    logVerbose("PID of %s is %d.", name, pid);
    return RET_SUCC;
}

static inline char rKill(int pid, const char *name, int sig)
{
    if (kill(pid, sig) < 0)
    {
        logError("Can't send signal to %s(%d)!", name, pid);
        return RET_EKILL;
    }
    logVerbose("Signal %d sent to %s(%d).", sig, name, pid);
    return RET_SUCC;
}

static inline int rSendMessage(int connfd, const char *name,
    char *message, int len)
{
    static char buf[2048];
    // use this to make gcc happy...
    static int *ibuf = (int*)buf;
    *ibuf = len;
    memcpy(buf + sizeof(int), message, len);
    if (rio_writen(connfd, buf, len + sizeof(int)) < 0)
    {
        logError("Can't send message to %s!(%s)", name, strerror(errno));
        return RET_EWRITE;
    }
    logVerbose("Sent message with length=%d to %s", len, name);
    return RET_SUCC;
}

static inline int rReceiveMessage(int connfd, const char *name, char *buf)
{
    static const int len = sizeof(int);
    static int msglen;
    if (rio_readn_force(connfd, (char*)&msglen, len) < len)
    {
        logError("Can't receive length from %s!", name);
        return RET_EREAD;
    }
    if (rio_readn_force(connfd, buf, msglen) < msglen)
    {
        logError("Can't receive message from %s!", name);
        return RET_EREAD;
    }
    logVerbose("Received message with length=%d from %s", msglen, name);
    return RET_SUCC;
}

#endif