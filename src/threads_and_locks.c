#include "threads_and_locks.h"
#include "flags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_table_lcked = FALSE;
int num_access = 0;
pthread_mutex_t access_lock;
pthread_mutex_t table_lck;
proj_t * proj_list = NULL;
/*
 *      Returns pointer to mutex for project if it exists; NULL otherwise.
 */
pthread_mutex_t * get_project_lock(char * proj_name)
{
        pthread_mutex_lock(&table_lck);
        is_table_lcked = TRUE;
        proj_t * ptr = proj_list;
        while (ptr != NULL)
        {
                if (strcmp(ptr->proj_name, proj_name)==0)
                {
                        is_table_lcked = FALSE;
                        pthread_mutex_unlock(&table_lck);
                        return &ptr->lock;
                }
                ptr = ptr->next;
        }
        is_table_lcked = FALSE;
        pthread_mutex_unlock(&table_lck);
        return NULL;
}
/*
 *      Adds project to server's proj_list.
 *      Returns lock to new project if successful; otherwise NULL.
 *      Warning: table_lck must be locked BEFORE calling this function.
 */
pthread_mutex_t * add_project(char * proj_name, char * file, int line)
{
        if (!is_table_lcked)
                return NULL;
        while(num_access != 0);
        // Check to make sure project is not already in list
        proj_t * ptr = proj_list;
        while (ptr != NULL)
        {
                if (strcmp(ptr->proj_name, proj_name)==0)
                {
                        fprintf(stderr, "[add_project] Error adding project to server. FILE: %s. LINE: %d.\n", file, line);
                        return NULL;
                }
                ptr = ptr->next;
        }
        // Add project to list
        proj_t * new_front = (proj_t *) malloc(sizeof(proj_t));
        if (new_front == NULL)
        {
                fprintf(stderr, "[add_project] Error adding project to server. FILE: %s. LINE: %d.\n", file, line);
                return NULL;
        }
        new_front->proj_name = (char *) malloc(sizeof(char)*(1+strlen(proj_name)));
        if (new_front->proj_name == NULL)
        {
                fprintf(stderr, "[add_project] Error adding project to server. FILE: %s. LINE: %d.\n", file, line);
                return NULL;
        }
        strcpy(new_front->proj_name, proj_name);
        if (pthread_mutex_init(&new_front->lock, NULL) != 0)
        {
                fprintf(stderr, "[add_project] Mutex init failed. FILE: %s. LINE: %d.\n", file, line);
                return NULL;
        }
        // Add to front
        new_front->next = proj_list;
        proj_list = new_front;
        return &new_front->lock;
}
/*
 *      Deletes project proj_name from server's proj_list.
 *      Returns pointer on success; NULL otherwise.
 *      Warning: table_lck must be locked BEFORE calling this function.
 *      Warning: locks project before returning the project to be deleted
 */
proj_t * delete_project(char * proj_name)
{
        if (!is_table_lcked)
                return NULL;
        while (num_access != 0);

        // First get proj from proj list
        proj_t * ptr = proj_list;
        if (ptr == NULL)
                return NULL;
        if (strcmp(ptr->proj_name, proj_name)==0)
        {
                // lock before removing ptr
                pthread_mutex_lock(&access_lock);
                num_access++;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_lock(&ptr->lock);
                proj_list = proj_list->next;
                return ptr;
        }
        else
        {
                while (ptr->next != NULL)
                {
                        if (strcmp(ptr->next->proj_name, proj_name)==0)
                                break;
                        ptr = ptr->next;
                }
                if (ptr->next == NULL)
                        return NULL;
                proj_t * to_be_deleted = ptr->next;
                pthread_mutex_lock(&access_lock);
                num_access++;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_lock(&to_be_deleted->lock);
                ptr->next = ptr->next->next;
                return to_be_deleted;
        }
        return NULL;
}
