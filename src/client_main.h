#ifndef CLIENT_H
#define CLIENT_H

int init_socket();
int get_configure(char * IP, int * PORT);
int set_configure(char * IP, char * port);
int create_or_destroy(char * proj_name, int create);

#endif
