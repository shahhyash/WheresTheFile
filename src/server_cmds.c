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
        // Return if projects folder does not already exist
        if (dir_exists("projects") == 0)
                return 1;
        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        while (is_table_lcked);
        pthread_mutex_lock(lock);
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char new_proj_name[strlen(proj_name)+9];
        sprintf(new_proj_name, "projects/%s", proj_name);
        char * zipped = recursive_zip(new_proj_name, TRUE);

        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);
        printf("zipped: %s\n", zipped);
        int zipped_size = strlen(zipped);

        int num_digits = 0;
        int i = zipped_size;
        while (i != 0)
        {
                num_digits++;
                i /= 10;
        }

        // Send three digit length of file size
        char file_size_str[4] = {'0','0','0',0};
        if (num_digits < 10)
                sprintf(&file_size_str[2], "%d", num_digits);
        else if (num_digits < 100)
                sprintf(&file_size_str[1], "%d", num_digits);
        else
                sprintf(&file_size_str[0], "%d", num_digits);
        printf("sending %s\n", file_size_str);
        if (better_send(sd, file_size_str, 3, 0, __FILE__, __LINE__) != 1)
        {
                return 1;
        }
        char zip_size_str[10] = {0,0,0,0,0,0,0,0,0,0};
        sprintf(zip_size_str, "%d", zipped_size);
        // Send file size
        printf("sending size %s\n", zip_size_str);
        fflush(stdout);
        if (better_send(sd, zip_size_str, strlen(zip_size_str), 0, __FILE__, __LINE__) != 1)
        {
                return 1;
        }
        int c_s;
        char * compressed = _compress(zipped, &c_s);
        char c_s_str[10] = {0,0,0,0,0,0,0,0,0,0};
        sprintf(c_s_str, "%d", c_s);
        // Send compressed file
        send_file(sd, compressed, c_s_str);
        return 0;
}

/*
 *      Creates directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int create(int sd, char * proj_name)
{
        // Create projects folder if one does not already exist
        if(!dir_exists("projects"))
        {
                if (make_dir("projects", __FILE__, __LINE__) < 0)
                        return 1;
        }
        pthread_mutex_lock(&table_lck);
        is_table_lcked = TRUE;
        pthread_mutex_t * lock = add_project(proj_name, __FILE__, __LINE__);
        if (lock == NULL)
                return 1;
        while (!is_table_lcked);
        pthread_mutex_lock(lock);
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char new_proj_name[strlen(proj_name)+9];
        sprintf(new_proj_name, "projects/%s", proj_name);

        // create new project in projects folder
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
        pthread_mutex_lock(&table_lck);
        is_table_lcked = TRUE;
        proj_t * to_be_deleted = delete_project(proj_name);
        if (to_be_deleted == NULL)
        {
                is_table_lcked = FALSE;
                pthread_mutex_unlock(&table_lck);
                fprintf(stderr, "[create] Error deleting project from repository.\n");
                return 1;
        }
        if (!dir_exists("projects"))
        {
                fprintf(stderr, "[create] Project does not exist.\n");
                return 1;
        }
        char new_proj_name[strlen(proj_name)+9];
        sprintf(new_proj_name, "projects/%s", proj_name);
        if (!dir_exists(new_proj_name))
        {
                fprintf(stderr, "[create] Project does not exist.\n");
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
