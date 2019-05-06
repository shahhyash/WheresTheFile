#include "manifest_utils.h"
#include "client_cmds.h"
#include "fileIO.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <openssl/sha.h>        // Hash function

manifest_entry * read_manifest_file(char * file_contents)
{
        /* create node for head of list */
        manifest_entry * root = (manifest_entry*)malloc(sizeof(manifest_entry));
        root->version=atoi(file_contents);
        root->file_path=NULL;
        root->hash_code=NULL;
        root->next=NULL;

        manifest_entry * ptr = root;

        char * line_saveptr;

        char * manifest_line = strtok_r(file_contents, "\n", &line_saveptr);
        while (manifest_line)
        {
            char *item_saveptr, *token, *file_path, *hash_code;
            int mode = 1, version;
            token = strtok_r(manifest_line, " ", &item_saveptr);
            while(token)
            {
                    if (mode == 1)
                            version = atoi(token);

                    if (mode == 2)
                    {
                            file_path = malloc(strlen(token) + 1);
                            strcpy(file_path, token);
                    }

                    if (mode == 3)
                    {
                            hash_code = malloc(strlen(token) + 1);
                            strcpy(hash_code, token);
                    }

                    ++mode;
                    token = strtok_r(NULL, " ", &item_saveptr);
            }

            if (mode == 4)
            {
                    /* create new node for current manifest entry */
                    ptr->next = (manifest_entry*)malloc(sizeof(manifest_entry));
                    ptr = ptr->next;
                    ptr->version = version;
                    ptr->file_path = file_path;
                    ptr->hash_code = hash_code;
                    printf("new hash: %s\n", hash_code);
                    ptr->next = NULL;
            }

            manifest_line = strtok_r(NULL, "\n", &line_saveptr);
        }

        return root;
}

void print_manifest(manifest_entry * root)
{
        while (root != NULL)
        {
                printf("version=%d, filepath=%s, hashcode=%s\n", root->version, root->file_path, root->hash_code);
                root = root->next;
        }
}

void free_manifest(manifest_entry * root)
{
        while (root != NULL)
        {
                manifest_entry * next = root->next;
                free(root->file_path);
                free(root->hash_code);
                free(root);
                root = next;
        }
}

void update_hashes(manifest_entry * root)
{
    while (root)
    {
        if (root->file_path)
        {
            /* fetch old hash code */
            char * old_hash = root->hash_code;

            /* read file contents */
            int fd = open(root->file_path, O_RDONLY, 00600);
            int file_length = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            char file_contents[file_length+1];
            bzero(file_contents, file_length+1);
            better_read(fd, file_contents, file_length, __FILE__, __LINE__);
            close(fd);

            /* compute new hash code */
            unsigned char hash[SHA256_DIGEST_LENGTH];
            bzero(hash, SHA256_DIGEST_LENGTH);
            SHA256((const unsigned char * ) file_contents, file_length, (unsigned char *) hash);
            char hex_hash[SHA256_DIGEST_LENGTH*2+1];
            bzero(hex_hash, SHA256_DIGEST_LENGTH*2+1);
            int j;
            for (j = 0; j < SHA256_DIGEST_LENGTH; j++)
            {
                    char hex[3] = {0,0,0};
                    sprintf(hex, "%x", (int)hash[j]);
                    if (strlen(hex) == 1)
                    {
                            hex_hash[2*j] = '0';
                            hex_hash[2*j+1] = hex[0];
                    }
                    else
                    {
                            hex_hash[2*j] = hex[0];
                            hex_hash[2*j+1] = hex[1];
                    }
            }
            char * new_hash = malloc(sizeof(char) * (strlen(hex_hash)+1));
            strcpy(new_hash, hex_hash);
            if (strcmp(old_hash, new_hash) == 0)
            {
                /* hashes are the same, don't do anything */
                free(new_hash);
            }
            else
            {
                /* hashes are different, update .manifest */
                free(old_hash);
                root->hash_code = new_hash;
            }
        }

        root = root->next;
    }
}

char * fetch_server_manifest(int sd, char * proj_name)
{
        if (send_cmd_proj(sd, proj_name, "man"))
                return NULL;

        char buf[31];
        bzero(buf, 31);
        if (better_read(sd, buf, 30, __FILE__, __LINE__) != 1)
                return NULL;
        printf("Message received from server: %s\n", buf);
        if (strcmp(buf, "Error: Project does not exist.") == 0)
        {
                fprintf(stderr, "Server returned error.\n");
                return NULL;
        }
        char * decompressed = receive_file(sd);
        if (decompressed == NULL)
        {
                fprintf(stderr, "[fetch_server_manifest] Error decompressing.\n");
                return NULL;
        }
        // printf("decompressed %s\n", decompressed);
        char * newline = strstr(decompressed, "\n");
        int size;
        // printf("newline %s\n", newline);
        sscanf(&newline[2], "%d\n", &size);
        // printf("size %d\n", size);
        char * manifest = (char *) malloc(sizeof(char)*(size+1));
        bzero(manifest, size+1);
        newline = strstr(&newline[2], "\n");
        newline = strstr(&newline[1], "\n");
        // printf("newline %s\n", newline);

        strncpy(manifest, &newline[1], size);
        // printf("file %s\n", manifest);
        free(decompressed);
        return manifest;
}

char * fetch_client_manifest(char * proj_name)
{
        char manifest_path[strlen("/.manifest") + strlen(proj_name) + 1];
        sprintf(manifest_path, "%s/.manifest", proj_name);
        int fd_man = open(manifest_path, O_RDONLY);
        if (fd_man == -1)
        {
                fprintf(stderr, "[fetch_client_manifest] Error opening file %s. FILE: %s. LINE: %d\n", manifest_path, __FILE__, __LINE__);
                return NULL;
        }

        int file_length = lseek(fd_man, 0, SEEK_END);
        lseek(fd_man, 0, SEEK_SET);

        char * manifest_buffer = (char *) malloc(sizeof(char) * file_length);
        if (better_read(fd_man, manifest_buffer, file_length, __FILE__, __LINE__) != 1)
                return NULL;

        close(fd_man);
        return manifest_buffer;
}

update_entry * fetch_updates(char * proj_name)
{
    /* fetch contents of update */
    char update_path[strlen("/.Update" + strlen(proj_name) + 1)];
    sprintf(update_path, "%s/.Update", proj_name);
    int fd_update = open(update_path, O_RDONLY);
    if (fd_update == -1)
    {
        fprintf(stderr, "[fetch_updates] Error opening file %s. FILE: %s. LINE: %d.\n", update_path, __FILE__, __LINE__);
        return NULL;
    }

    int file_length = lseek(fd_update, 0, SEEK_END);
    lseek(fd_update, 0, SEEK_SET);

    char * update_buf = (char*) malloc(sizeof(char) * file_length);
    if (better_read(fd_update, update_buf, file_length, __FILE__, __LINE__) != 1)
        return NULL;

    close(fd_update);

    /* create root node and ptr node references */
    update_entry * root = NULL;
    update_entry * ptr = NULL;

    /* iterate through files and build linked list */
    char *line_saveptr;
    char * update_line = strtok_r(update_buf, "\n", &line_saveptr);
    while (update_line)
    {
        char *token, *token_saveptr, *file_path;
        int mode = 1;
        token = strtok_r(update_line, " ", &token_saveptr);
        while(token)
        {
                if (mode == 1)
                {
                    update_entry * next = (update_entry*)malloc(sizeof(update_entry));
                    next->code = token[0];
                    next->next = NULL;

                    if(root)
                    {
                        ptr->next = next;
                        ptr = ptr->next;
                    }
                    else
                    {
                        root = next;
                        ptr = root;
                    }

                }

                if (mode == 2)
                {
                        file_path = malloc(strlen(token) + 1);
                        strcpy(file_path, token);
                        ptr->file_path = file_path;
                }

                ++mode;
                token = strtok_r(NULL, " ", &token_saveptr);
        }

        update_line = strtok_r(NULL, "\n", &line_saveptr);
    }

    free(update_buf);
    return root;
}

void free_updates(update_entry * root)
{
    while (root)
    {
        free(root->file_path);
        update_entry * next = root->next;
        free(root);
        root = next;
    }
}
