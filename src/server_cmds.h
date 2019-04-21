#ifndef SERVER_CMDS_H
#define SERVER_CMDS_H

/*
 *      Creates directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int create(char * proj_name);
/*
 *      Deletes directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int destroy(char * proj_name);

#endif
