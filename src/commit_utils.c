#include "commit_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fileIO.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

char * fetch_commit_file(char * proj_name, int is_server, int commit_id)
{
    char * commit_path;

    if (is_server && commit_id > 0)
    {
        int digits = 1;
        if (commit_id > 9)  ++digits;
        if (commit_id > 99) ++digits;
        if (commit_id > 999) ++digits;

        /* if it's fetching server copy, it's a different path */
        int path_len = strlen(".server_repo/") + strlen(proj_name) + strlen("/.commit") + digits + 1;
        commit_path = (char *) malloc(sizeof(char)*path_len);
        bzero(commit_path, path_len);
        sprintf(commit_path, ".server_repo/%s/.commit%d", proj_name, commit_id);
    }
    else
    {
        /* build commit path for client side */
        int path_len =strlen(proj_name) + strlen("/.commit") + 1;
        commit_path =(char *) malloc(sizeof(char)*path_len);
        bzero(commit_path, path_len);
        sprintf(commit_path, "%s/.commit", proj_name);
    }

    int fd = open(commit_path, O_RDONLY, 00600);
    if (fd == -1)
    {
        fprintf(stderr, "[fetch_commit_file] ERROR: Unable to open manifest file %s for project %s.\n", commit_path, proj_name);
        return NULL;
    }

    int file_length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char * file = (char*) malloc(sizeof(char) * file_length);
    if (better_read(fd, file, file_length, __FILE__, __LINE__) <= 0)
    {
        fprintf(stderr, "[fetch_commit_file] ERROR: Unable to read .commit file.\n");
        return NULL;
    }

    /* close the file */
    close(fd);
    free(commit_path);

    return file;
}

commit_entry * read_commit_file(char * file_contents)
{
    /* start by creating list pointers */
    commit_entry *root = NULL, *ptr = NULL;

    char * line_saveptr;
    char * commit_line = strtok_r(file_contents, "\n", &line_saveptr);
    while (commit_line)
    {
        char *item_saveptr, *token, *file_path, *hash_code;
        int mode = 1;
        token = strtok_r(commit_line, " ", &item_saveptr);
        while(token)
        {
            if (mode == 1)
            {
                if (!ptr)
                {
                    root = (commit_entry*) malloc(sizeof(commit_entry));
                    ptr = root;
                }
                else
                {
                    ptr->next = (commit_entry*) malloc(sizeof(commit_entry));
                    ptr = ptr->next;
                }

                ptr->op_code = token[0];
            }

            if (mode == 2)
                    ptr->version = atoi(token);

            if (mode == 3)
            {
                    file_path = malloc(strlen(token) + 1);
                    strcpy(file_path, token);
                    ptr->file_path = file_path;
            }

            if (mode == 4)
            {
                    hash_code = malloc(strlen(token) + 1);
                    strcpy(hash_code, token);
                    ptr->hash_code = hash_code;
            }

            ++mode;
            token = strtok_r(NULL, " ", &item_saveptr);
        }

        commit_line = strtok_r(NULL, "\n", &line_saveptr);
    }

    return root;
}

void free_commit_list(commit_entry * root)
{
    while(root)
    {
        commit_entry * next = root->next;
        free(root->file_path);
        free(root->hash_code);
        free(root);
        root = next;
    }
}
