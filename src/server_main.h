#ifndef SERVER_MAIN_H
#define SERVER_MAIN_H

/*
 *      Handles client communication through the inputted socket descriptor.
 */
void * client_comm(void * args);
/*
 *      Reads length of project name. Then reads project name from client.
 *      Input sd, a socket file descriptor, and an
 *      Returns NULL on error; otherwise returns pointer to project name.
 */
char * read_project_name(int sd);
/*
*      Prints usage to stdout
*/
void usage();

#endif
