#ifndef CLIENT_CMDS_H
#define CLIENT_CMDS_H

/*
 *      Adds filename to the .manifest in the project proj_name.
 *      Returns 1 on error, 0 on success.
 */
int _add(char * proj_name, char * filename);
/*
 *      Sends create (if create is TRUE) or destroy (if create is FALSE) commands to the
 *      server with the corresponding proj_name. Returns 0 on sucess; 1 otherwise.
 */
int create_or_destroy(char * proj_name, int create);
/*
 *      Reads IP and PORT from .configure file and stores them in the passed pointers.
 *      Returns 0 on sucess; 1 otherwise.
 */
int get_configure(char * IP, int * PORT);
/*
 *      Returns file descriptor to the socket as specified by the PORT and IP
 *      in .configure. Returns -1 on error.
 */
int init_socket();
/*
 *      Writes IP and port to .configure file. Overwrites .configure if it already
 *      exists. Returns 0 on success; 1 otherwise.
 */
int set_configure(char * IP, char * port);
/*
 *      Removes filename to the .manifest in the project proj_name.
 *      Returns 1 on error, 0 on success.
 */
int _remove(char * proj_name, char * filename);
/*
 *
 */
int checkout(char * proj_name);
/*
 *      Creates .update file to track any server changes not present in local version
 */ 
int _update(char * proj_name);

#endif
