#include <iostream>
#include "../include/rwr.hpp"

int cread(int fd, char *buf, int n){

    int nread;

    // -----------------------------------------------------------


    while ((nread=read(fd, buf, n)) < 0) {
        // Let's wait for the socket to become free to read :^)
    }

    // -----------------------------------------------------------

    return nread;
}

/**************************************************************************
* cwrite: write routine that checks for errors and exits if an error is  *
*         returned.                                                      *
**************************************************************************/
int cwrite(int fd, char *buf, int n){

    int nwrite;

    if((nwrite=write(fd, buf, n)) < 0){
        perror("Writing data");
        exit(1);
    }
    return nwrite;
}

/**************************************************************************
* read_n: ensures we read exactly n bytes, and puts them into "buf".     *
*         (unless EOF, of course)                                        *
**************************************************************************/
int read_n(int fd, char *buf, int n) {
    int nread = 0, left = n;

    while(left > 0) {
        if ((nread = cread(fd, buf, left)) == 0){
            return 0 ;
        }else {
            left -= nread;
            buf += nread;
        }
    }
    return n;
}

/**************************************************************************
* do_debug: prints debugging stuff (doh!)                                *
**************************************************************************/
void do_debug(int debug, char *msg, ...){

    va_list argp;

    if(debug) {
        va_start(argp, msg);
        vfprintf(stderr, msg, argp);
        va_end(argp);
    }
}

/**************************************************************************
* my_err: prints custom error messages on stderr.                        *
**************************************************************************/
void my_err(char *msg, ...) {

    va_list argp;

    va_start(argp, msg);
    vfprintf(stderr, msg, argp);
    va_end(argp);
}
