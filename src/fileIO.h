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
/*
 *      Returns TRUE if directory exists, FALSE otherwise.
 */
int dir_exists(const char * dir);
/*
 *      Returns TRUE if file exists, FALSE otherwise.
 */
int file_exists(char * file);
/*
 *      Make directory called dir_name if it does not exist.
 *      Returns 0 on success; 1 if dir exists, and -1 on other error in mkdir
 */
int make_dir(char * dir_name, char * file, int line);
/*
 *      Recursively deletes directory dir and the contents inside
 */
void remove_dir(char * dir);

#endif
