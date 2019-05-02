#ifndef THREADS_AND_LOCKS_H
#define THREADS_AND_LOCKS_H
#include <pthread.h>            // Threading

typedef struct _proj_t {
        pthread_mutex_t lock;
        char * proj_name;
        struct _proj_t * next;
} proj_t;

/*
 *      Returns pointer to mutex for project if it exists; NULL otherwise.
 */
pthread_mutex_t * get_project_lock(char * proj_name);
/*
 *      Adds project to server's proj_list.
 *      Returns lock to new project if successful; otherwise NULL;
 */
pthread_mutex_t * add_project(char * proj_name, char * file, int line);
/*
 *      Deletes project proj_name from server's proj_list.
 *      Returns pointer on success; NULL otherwise.
 *      Warning: table_lck must be locked BEFORE calling this function.
 *      Warning: locks project before returning the project to be deleted
 */
proj_t * delete_project(char * proj_name);

#endif