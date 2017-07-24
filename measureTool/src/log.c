#include "log.h"

int verbose = 1;

void verboseOn()
{
    verbose = 1;
}

void verboseOff()
{
    verbose = 0;
}