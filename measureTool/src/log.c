#include "log.h"

int verbose = 0;

void verboseOn()
{
    verbose = 1;
}

void verboseOff()
{
    verbose = 0;
}