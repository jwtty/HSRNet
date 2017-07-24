#include "util.h"

static char *smem[16];
static int smem_fd[16];

void initSharedMem(int index)
{
    char buf[32];
    sprintf(buf, "temp/%d", index);
    smem_fd[index] = Open(buf, O_CREAT | O_RDWR, 0777);
    smem[index] = Mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
        MAP_SHARED, smem_fd[index], 0);
}

void notifyPID(int index, int pid)
{
    *(int*)smem[index] = pid;
}

int peekPID(int index)
{
    return *(int*)smem[index];
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