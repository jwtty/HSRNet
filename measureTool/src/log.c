#include "log.h"
#include "csapp.h"

int verbose = 0;
static struct timeval tst, ted;

void verboseOn()
{
    verbose = 1;
}

void verboseOff()
{
    verbose = 0;
}

void printInitTimestamp()
{
    logMessage("Timestamp initialized at %lf", 
        tst.tv_sec + tst.tv_usec / (double)1000000.0);
}

void initTimestamp()
{
    gettimeofday(&tst, NULL);
}

double getTimestamp()
{
    gettimeofday(&ted, NULL);
    return (ted.tv_sec - tst.tv_sec) + (ted.tv_usec - tst.tv_usec) / 1000000.0;
}