#include "server_cmds.h"
#include "fileIO.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>             // FILE IO
#include <fcntl.h>              // open flags
#include "compression.h"
#include "flags.h"
#include "threads_and_locks.h"

extern int is_table_lcked;
extern int num_access;
extern pthread_mutex_t access_lock;
extern pthread_mutex_t table_lck;
extern proj_t * proj_list;

/*
 *      Clones project and sends it to client. Returns 0 on success; 1 otherwise.
 */
int checkout(int sd, char * proj_name)
{
        // Return if .server_repo folder does not already exist
        if (dir_exists(".server_repo") == 0)
                return 1;
        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        else
        {
                if (better_send(sd, "Found repository. Sending now.", 30, 0, __FILE__, __LINE__) != 1)
                        return 1;
        }
        while (is_table_lcked)
                printf("table locked.\n");
        pthread_mutex_lock(lock);
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char new_proj_name[strlen(proj_name)+1+strlen(".server_repo/")];
        bzero(new_proj_name, strlen(proj_name)+1+strlen(".server_repo/"));
        sprintf(new_proj_name, ".server_repo/%s", proj_name);

        int ret = compress_and_send(sd, new_proj_name);

        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);
        return ret;
}

/*
 *      Creates directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int create(int sd, char * proj_name)
{
        if (dir_exists(".server_repo") == 0)
                return 1;
        printf("Seeing is table is locked...\n");
        pthread_mutex_lock(&table_lck);
        printf("Table is unlocked.");
        is_table_lcked = TRUE;
        pthread_mutex_t * lock = add_project(proj_name, __FILE__, __LINE__);
        if (lock == NULL)
        {
                pthread_mutex_unlock(&table_lck);
                is_table_lcked = FALSE;
                return 1;
        }
        while (!is_table_lcked)
        {
                printf("Table locked\n");
        }
        pthread_mutex_lock(lock);
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char new_proj_name[strlen(proj_name)+1+strlen(".server_repo/")];
        bzero(new_proj_name, strlen(proj_name)+1+strlen(".server_repo/"));
        sprintf(new_proj_name, ".server_repo/%s", proj_name);

        // create new project in .server_repo folder
        if (make_dir(new_proj_name, __FILE__, __LINE__) != 0)
        {
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                is_table_lcked = FALSE;
                pthread_mutex_unlock(&table_lck);
                return 1;
        }
        printf("%s\n", new_proj_name);

        // Init manifest
        char manifest[strlen(new_proj_name)+11];
        bzero(manifest, strlen(new_proj_name)+11);
        sprintf(manifest, "%s/.manifest", new_proj_name);
        int fd = open(manifest, O_WRONLY | O_CREAT | O_TRUNC, 00600);
        if (better_write(fd, "1\n", 2, __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[create] Error returned by better_write. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                close(fd);
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                is_table_lcked = FALSE;
                pthread_mutex_unlock(&table_lck);
                return 1;
        }
        close(fd);
        // int compressed_size;
        char data[3] = {'1', '\n', 0};
        // char * compressed_manifest = _compress(data, &compressed_size);
        // char size_buf[10] = {0,0,0,0,0,0,0,0,0,0};
        // sprintf(size_buf, "%d", compressed_size);
        if (send_file(sd, data, "3"))
        {
                fprintf(stderr, "[create] send_file returned error.\n");
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                is_table_lcked = FALSE;
                pthread_mutex_unlock(&table_lck);
                return 1;
        }
        // free(compressed_manifest);
        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);
        is_table_lcked = FALSE;
        pthread_mutex_unlock(&table_lck);
        return 0;
}

/*
 *      Deletes directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int destroy(char * proj_name)
{
        if (dir_exists(".server_repo") == 0)
                return 1;
        printf("Seeing is table is locked...\n");
        pthread_mutex_lock(&table_lck);
        printf("Table is unlocked.");
        is_table_lcked = TRUE;
        proj_t * to_be_deleted = delete_project(proj_name);
        if (to_be_deleted == NULL)
        {
                is_table_lcked = FALSE;
                pthread_mutex_unlock(&table_lck);
                fprintf(stderr, "[create] Error deleting project from repository.\n");
                return 1;
        }
        if (!dir_exists(".server_repo"))
        {
                fprintf(stderr, "[create] Project does not exist.\n");
                pthread_mutex_unlock(&table_lck);
                return 1;
        }
        char new_proj_name[strlen(proj_name)+1+strlen(".server_repo/")];
        bzero(new_proj_name, strlen(proj_name)+1+strlen(".server_repo/"));
        sprintf(new_proj_name, ".server_repo/%s", proj_name);
        if (!dir_exists(new_proj_name))
        {
                fprintf(stderr, "[create] Project does not exist.\n");
                pthread_mutex_unlock(&table_lck);
                return 1;
        }
        printf("removing directory: %s\n", new_proj_name);
        remove_dir(new_proj_name);
        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(&to_be_deleted->lock);
        pthread_mutex_destroy(&to_be_deleted->lock);
        free(to_be_deleted->proj_name);
        free(to_be_deleted);
        is_table_lcked = FALSE;
        pthread_mutex_unlock(&table_lck);
        return 0;
}

/*
 *      Sends manifest for project proj_name to client.
 *      Returns 0 on success, 1 otherwise.
 */
int send_manifest(int sd, char * proj_name)
{
        // Return if .server_repo folder does not already exist
        if (dir_exists(".server_repo") == 0)
                return 1;
        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        else
        {
                if (better_send(sd, "Found repository. Sending now.", 30, 0, __FILE__, __LINE__) != 1)
                        return 1;
        }
        while (is_table_lcked)
                printf("table locked.\n");
        pthread_mutex_lock(lock);
        // Mark another thread as accessing
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char file_name[strlen(proj_name)+1+strlen(".server_repo/")+strlen("/.manifest")];
        bzero(file_name, strlen(proj_name)+1+strlen(".server_repo/")+strlen("/.manifest"));
        // Read and compress manifest
        sprintf(file_name, ".server_repo/%s/.manifest", proj_name);

        int ret = compress_and_send(sd, file_name);

        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);

        return ret;
}

/*
 *      Sends specified file for project proj_name to client.
 *      Returns 0 on success, 1 otherwise.
 */
int send_server_copy(int sd, char * file_path)
{
        /* Return if .server_repo folder does not already exist */
        if (dir_exists(".server_repo") == 0)
                return 1;

        /* fetch project name from file path */
        char * sub_path = index(file_path, '/');
        int proj_name_length = sub_path - file_path;
        char proj_name[proj_name_length + 1];
        strncpy(proj_name, file_path, proj_name_length);
        proj_name[proj_name_length] = '\0';

        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        else
        {
                if (better_send(sd, "Found repository. Sending now.", 30, 0, __FILE__, __LINE__) != 1)
                        return 1;
        }
        while (is_table_lcked)
                printf("table locked.\n");
        pthread_mutex_lock(lock);
        
        /* Mark another thread as accessing */
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char file_name[strlen(".server_repo/") + strlen(file_path) + 1];
        bzero(file_name, strlen(".server_repo/") + strlen(file_path) + 1);
        
        /* Read and compress manifest */
        sprintf(file_name, ".server_repo/%s", file_path);

        int ret = compress_and_send(sd, file_name);

        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);

        return ret;
}