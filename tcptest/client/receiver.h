#ifndef __RECEIVER_H
#define __RECEIVER_H

#define E_INVAL 1
#define E_OTHER 2

/*
 *  Function: 
 *      Create a process which can recieve data from /../server/sender through given port.
 *      Return normally when get some data and TCP FINs.
 *  Usage: 
 *      receiver -p [port name]
 *  Example: 
 *      receiver -p 8080
 *  Return Value:
 *      0 = success 
 *      -E_INVAL = invalid parameter
 *      -E_OTHER = other undefined error
 */

#endif
