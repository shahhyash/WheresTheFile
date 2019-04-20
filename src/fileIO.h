#ifndef FILEIO_H
#define FILEIO_H

#include <stdio.h>

/*
 *    Sends nbyte bytes from the filedescriptor fd and stores them in buf.
 *    Returns 1 if successful. Otherwise returns a value according to the error.
 */
int better_send(int fd, char * buf, size_t nbyte, int flags, char * file, int line);
/*
 *    Reads nbyte bytes from the filedescriptor filedes and stores them in buf.
 *    Returns 1 if successful. Otherwise returns a value according to the error.
 */
int better_read (int filedes, char * buf, size_t nbyte, char * file, int line);
/*
 *    Writes nbyte bytes from the buf and stores them in file with filedescriptor filedes.
 *    Returns 1 if successful. Otherwise returns a value according to the error.
 */
int better_write(int filedes, char * buf, size_t nbyte, char * file, int line);

#endif
