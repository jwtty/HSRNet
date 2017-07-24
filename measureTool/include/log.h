#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

extern int verbose;
void verboseOn();
void verboseOff();

#define logVerbose(...)\
do\
{\
    if (verbose)\
    {\
        fprintf(stderr, "[Verbose]:");\
        fprintf(stderr, __VA_ARGS__);\
        fprintf(stderr, "\n");\
    }\
}\
while (0)

#define logMessage(...)\
do\
{\
    fprintf(stderr, "[  Log  ]:");\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
}\
while (0)

#define logWarning(...)\
do\
{\
    fprintf(stderr, "[Warning]:");\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
}\
while (0)

#define logError(...)\
do\
{\
    fprintf(stderr, "[ Error ]:");\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
}\
while (0)

#define logFatal(...)\
do\
{\
    fprintf(stderr, "[ FATAL ]:");\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
    exit(1);\
}\
while (0)

#endif