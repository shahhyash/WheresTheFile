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
            char * new_hash = malloc(sizeof(char) * (SHA256_DIGEST_LENGTH));
            SHA256((const unsigned char * ) file_contents, file_length, (unsigned char *) new_hash);

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
        sscanf(&newline[2], "%d\n", &size);
        // printf("size %d\n", size);
        char * manifest = (char *) malloc(sizeof(char)*(size+1));
        bzero(manifest, size+1);
        sscanf(&newline[2], "%d\n%s", &size, manifest);
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
