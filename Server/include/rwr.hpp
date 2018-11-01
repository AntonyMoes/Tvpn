#ifndef INCLUDE_RWR_HPP_
#define INCLUDE_RWR_HPP_
#include <cerrno>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
int cread(int fd, char *buf, int n);

/**************************************************************************
* cwrite: write routine that checks for errors and exits if an error is  *
*         returned.                                                      *
**************************************************************************/
int cwrite(int fd, char *buf, int n);

/**************************************************************************
* read_n: ensures we read exactly n bytes, and puts them into "buf".     *
*         (unless EOF, of course)                                        *
**************************************************************************/
int read_n(int fd, char *buf, int n);

/**************************************************************************
* do_debug: prints debugging stuff (doh!)                                *
**************************************************************************/
void do_debug(int debug, char *msg, ...);

/**************************************************************************
* my_err: prints custom error messages on stderr.                        *
**************************************************************************/
void my_err(char *msg, ...);




#endif //  _INCLUDE_RWR_HPP_
