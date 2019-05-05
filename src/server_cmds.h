#ifndef SERVER_CMDS_H
#define SERVER_CMDS_H
/*
 *      Clones project and sends it to client. Returns 0 on success; 1 otherwise.
 */
int checkout(int sd, char * proj_name);
/*
 *      Creates directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int create(int s, char * proj_name);
/*
 *      Deletes directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int destroy(char * proj_name);
/*
 *      Sends manifest for project proj_name to client.
 *      Returns 0 on success, 1 otherwise.
 */
int send_manifest(int sd, char * proj_name);

#endif
