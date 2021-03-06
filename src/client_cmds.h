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
 *      Sends project name and command to server. Handles communication.
 *      Returns 0 on success, 1 otherwise.
 */
int send_cmd_proj(int sock, char * proj_name, char * cmd);
/*
 *      Clones repository from server. Returns 1 on success; 0 otherwise.
 */
int _checkout(char * proj_name);
/*
 *      Creates .update file to track any server changes not present in local version
 *      Returns 0 on success, 1 otherwise.
 */
int _update(char * proj_name);
/*
 *      Uses .Update file to perform server operations and update client copies of files
 */
int _upgrade(char * proj_name);
/*
 *      Requests server manifest and outputs a list of all files under the project name along with their version numbers
 *      Returns 0 on success, 1 otherwise.
 */
int current_version(char * proj_name);
/*
 *      Creates a .commit file to keep track of all changes on client's copy of project
 */
int _commit(char * proj_name);
/*
 *      Performs all required operations to fulfill the most recent commit.
 */
int _push(char * proj_name);
/*
 *      The server will revert its current version of the project back to the version number
 *      requested by the client by deleting all more recent versions saved on the server side.
 *      Return 0 on success, 1 otherwise.
 */
int _rollback(char * proj_name, char * version);
/*
 *      The server will send over a file containing the history of all operations
 *      performed on all successful pushes since the project's creation. The output
 *      should be similar to the update output, but with a version number and newline
 *      separating each push's log of changes
 *      Return 0 on success, 1 otherwise.
 */
int _history(char * proj_name);

#endif
