#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

extern int verbose;
void verboseOn();
void verboseOff();

void initTimestamp();
double getTimestamp();
void printInitTimestamp();

#define logVerbose(...)\
do\
{\
    if (verbose)\
    {\
        fprintf(stderr, "[ Verbose ](%12.6lf): ", getTimestamp());\
        fprintf(stderr, __VA_ARGS__);\
        fprintf(stderr, "\n");\
    }\
}\
while (0)

#define logMessage(...)\
do\
{\
    fprintf(stderr, "[   Log   ](%12.6lf): ", getTimestamp());\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
}\
while (0)

#define logWarning(...)\
do\
{\
    fprintf(stderr, "[ Warning ](%12.6lf): ", getTimestamp());\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
}\
while (0)

#define logError(...)\
do\
{\
    fprintf(stderr, "[  Error  ](%12.6lf): ", getTimestamp());\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
}\
while (0)

#define logFatal(...)\
do\
{\
    fprintf(stderr, "[ FATAL ](%12.6lf): ", getTimestamp());\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
    exit(1);\
}\
while (0)

static inline void redirectLogTo(char *path)
{
    int os = open(path, O_APPEND | O_WRONLY | O_CREAT, 0666);
    if (os < 0)
    {
        logFatal("Can't open log file %s.", path);
    }
    else
    {
        dup2(os, 2);
    }
}

#endif