#include "util.h"

static char *smem[16];
static int smem_fd[16];
static char fbuf[SHARED_BLOCK_LEN];

handler_t *SignalNoRestart(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigfillset(&action.sa_mask); // block all signal here
    action.sa_flags = 0;

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

void initSharedMem(int index)
{
    struct stat stat;
    char buf[32];
    sprintf(buf, "temp/%d", index);
    smem_fd[index] = Open(buf, O_CREAT | O_RDWR, 0777);
    logVerbose("Opened %s as shared memory block #%d.", buf, index);
    
    fstat(smem_fd[index], &stat);
    if (stat.st_size < SHARED_BLOCK_LEN)
    {
        logVerbose("File shorter than required, write to fill it.");
        Write(smem_fd[index], fbuf, SHARED_BLOCK_LEN);
    }
    smem[index] = Mmap(NULL, SHARED_BLOCK_LEN, PROT_READ | PROT_WRITE, 
        MAP_SHARED, smem_fd[index], 0);
    logVerbose("Shared memory block #%d(%lx) initialized.", index,
        (unsigned long)smem[index]);
}

void notifyPID(int index, int pid)
{
    *(int*)smem[index] = pid;
    logVerbose("PID set to %d.", *(int*)smem[index]);
}

int peekPID(int index)
{
    int ret = *(int*)smem[index];
    logVerbose("PID fetched, value is %d.", ret);
    return ret;
}

void setMessage(int index, char *message)
{
    int len;

    len = strlen(message) + 1;
    *(int*)smem[index] = len;
    memcpy(smem[index] + sizeof(int), message, len);
}

int getMessage(int index, char *dest)
{
    int len;

    len = *(int*)smem[index];
    memcpy(dest, smem[index] + sizeof(int), len);
    return 0;
}

char *retstr(char ret, char *buf)
{
    switch (ret)
    {
    case RET_SUCC:
        sprintf(buf, "Success(RET_SUCC, %d)", RET_SUCC);
        break;
    case RET_EMSG:
        sprintf(buf, "Unrecognized control message(RET_EMSG, %d)",
            RET_EMSG);
        break;
    case RET_EKILL:
        sprintf(buf, "Controller failed to send signal(RET_EKILL, %d)",
            RET_EKILL);
        break;
    case RET_EREAD:
        sprintf(buf, "Failed to read message(RET_EREAD, %d)", 
            RET_EREAD);
        break;
    case RET_EWRITE:
        sprintf(buf, "Failed to write message(RET_EWRITE, %d)", 
            RET_EWRITE);
        break;
    case RET_EPID:
        sprintf(buf, "Failed to get PID of server(RET_EPID, %d)", 
            RET_EPID);
        break;
    default:
        sprintf(buf, "Value not defined(%d)", (int)ret);
    }
    return buf;
}

/* netdial and netannouce code comes from libtask: http://swtch.com/libtask/
 * Copyright: http://swtch.com/libtask/COPYRIGHT
*/

/* make connection to server */
int
netdial(int domain, int proto, char *local, int local_port, char *server, int port)
{
    struct addrinfo hints, *local_res, *server_res;
    int s;

    if (local) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = domain;
        hints.ai_socktype = proto;
        if (getaddrinfo(local, NULL, &hints, &local_res) != 0)
            return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = domain;
    hints.ai_socktype = proto;
    if (getaddrinfo(server, NULL, &hints, &server_res) != 0)
        return -1;

    s = socket(server_res->ai_family, proto, 0);
    if (s < 0) {
	if (local)
	    freeaddrinfo(local_res);
	freeaddrinfo(server_res);
        return -1;
    }

    if (local) {
        if (local_port) {
            struct sockaddr_in *lcladdr;
            lcladdr = (struct sockaddr_in *)local_res->ai_addr;
            lcladdr->sin_port = htons(local_port);
            local_res->ai_addr = (struct sockaddr *)lcladdr;
        }

        if (bind(s, (struct sockaddr *) local_res->ai_addr, local_res->ai_addrlen) < 0) {
	    close(s);
	    freeaddrinfo(local_res);
	    freeaddrinfo(server_res);
            return -1;
	}
        freeaddrinfo(local_res);
    }

    ((struct sockaddr_in *) server_res->ai_addr)->sin_port = htons(port);
    if (connect(s, (struct sockaddr *) server_res->ai_addr, server_res->ai_addrlen) < 0 && errno != EINPROGRESS) {
	close(s);
	freeaddrinfo(server_res);
        return -1;
    }

    freeaddrinfo(server_res);
    return s;
}