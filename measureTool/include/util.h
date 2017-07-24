#ifndef __UTIL_H__
#define __UTIL_H__

#include "csapp.h"
#include "log.h"

#define PACKET_LEN 131072

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

#define TYPE_LONG 0
#define TYPE_FIX 1

int netdial(int domain, int proto, char *local, int local_port, 
    char *server, int port);

void initSharedMem(int index);
void notifyPID(int index, int pid);
int peekPID(int index);

void setMessage(int index, char *message);
int getMessage(int index, char *dest);

char *retstr(char ret, char *buf);

typedef int lock_t;
static inline void init_lock(lock_t *plock)
{
    *plock = 0;
}
static inline void spin(lock_t *plock)
{
    while (*plock == 0);
}
static inline void spinOR(lock_t *plock1, lock_t *plock2)
{
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

static inline char rKill(int pid, const char *name, int sig)
{
    if (kill(pid, sig) < 0)
    {
        logError("Can't send signal to %s(%d)!", name, pid);
        return RET_EKILL;
    }
    return RET_SUCC;
}

static inline int rSendMessage(int connfd, const char *name ,char *message)
{
    static const int len = sizeof(int);
    if (rio_writen(connfd, message, len + strlen(message + len)) < 0)
    {
        logError("Can't send message to %s!(%s)", name, strerror(errno));
        return RET_EWRITE;
    }
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
    return RET_SUCC;
}

#endif