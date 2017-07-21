#ifndef __SENDER_H
#define __SENDER_H

#define E_INVAL 1
#define E_OTHER 2

/*
 *  Function: 
 *      Create a process which can send data to /../client/client through given port.
 *      Return normally when size or time is up.
 *  Usage:
 *      sender -p [port name] -s [reciever's socket] -l [flow size (Bytes)]
 *      sender -p [port name] -s [reciever's socket] -t [transmission time (ms)]
 *  Example: 
 *      sender -p 8080 -s 8.8.8.8:8080 -l 1048576
 *      sender -p 8080 -s 8.8.8.8:8080 -t 1000
 *  Return Value:
 *      0 = success
 *      -E_INVAL = invalid parameter
 *      -E_OTHER = other undefined error
 */

#endif
