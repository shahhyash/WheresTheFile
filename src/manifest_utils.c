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
#include "flags.h"

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
                    // printf("new hash: %s\n", hash_code);
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
            char * new_hash = hash(file_contents);
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
/*
 *      Returns the version number of the filename in the manifest.
 *      If the filename==proj_name, returns manifest version.
 *      Returns -1 on error.
 */
int get_version(char * proj_name, char * filename, int * is_dif_ver)
{
        *is_dif_ver = FALSE;
        char man_name[strlen(proj_name)+strlen(filename)+2];
        bzero(man_name, strlen(proj_name)+strlen(filename)+2);
        sprintf(man_name, "%s/%s", proj_name, filename);
        // Open manifest
        char manifest_path[strlen("/.manifest") + strlen(proj_name)];
        sprintf(manifest_path, "%s/.manifest", proj_name);
        int fd = open(manifest_path, O_RDONLY, 00600);
        if (fd == -1)
                return -1;
        int size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        if (size < 2)
        {
                fprintf(stderr, "[get_version] Invalid .manifest.");
                return -1;
        }
        char buf[size+1];
        bzero(buf, size+1);
        if (better_read(fd, buf, size, __FILE__, __LINE__) != 1)
                return -1;
        close(fd);
        // Get manifest version
        char * token = strtok(buf, "\n");
        int version;
        sscanf(token, "%d", &version);
        if (strcmp(proj_name, filename) == 0)
        // Return manifest version
        {
                return version;
        }
        token = strtok(NULL, "\n");
        while (token != NULL)
        // Return file version in manifest
        {
                char * subtoken = strtok_r(token, " ", &token);
                sscanf(subtoken, "%d", &version);
                subtoken = strtok_r(token, " ", &token);
                if (strcmp(subtoken, man_name) == 0)
                {
                        // Compare hash codes
                        int f = open(man_name, O_RDONLY, 00600);
                        if (f == -1)
                                return -1;
                        int sze = lseek(fd, 0, SEEK_END);
                        lseek(fd, 0, SEEK_SET);
                        char file[sze+1];
                        bzero(file, sze+1);
                        if (better_read(f, file, sze, __FILE__, __LINE__) != 1)
                                return -1;
                        char * hashcode = hash(file);
                        subtoken = strtok_r(token, " ", &token);
                        if (strcmp(subtoken, hashcode) != 0)
                                *is_dif_ver = TRUE;
                        free(hashcode);
                        return version;
                }
                token = strtok(NULL, "\n");
        }
        // Not found
        return 0;

}
/*
 *      Returns hash of data.
 */
char * hash(char * data)
{
        if (data == NULL)
                return NULL;
        if (strlen(data) == 0)
        {
                char * hex_hash = (char *) malloc(sizeof(char));
                *hex_hash = '\0';
                return hex_hash;
        }
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256((const unsigned char *) data, strlen(data), hash);
        char * hex_hash = (char *) malloc(sizeof(char)*(SHA256_DIGEST_LENGTH*2+1));
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
        return hex_hash;
}
/*
 *      Increments manifest version by one.
 *      Returns 0 on success, 1 otherwise.
 */
int update_manifest_version(char * proj_name, int version)
{
        char man_name[strlen(proj_name)+strlen("/.manifest")+1];
        bzero(man_name, strlen(proj_name)+strlen("/.manifest")+1);
        sprintf(man_name, "%s/.manifest", proj_name);
        int fd = open(man_name, O_RDONLY, 00600);
        if (fd == -1)
        {
                fprintf(stderr, "[update_manifest_version] Error opening %s FILE: %s LINE: %d\n", man_name, __FILE__, __LINE__);
                return 1;
        }
        int size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        char buf[size+1];
        bzero(buf, size+1);
        if (better_read(fd, buf, size, __FILE__, __LINE__) != 1)
                return 1;
        close(fd);
        int i = 0;
        while (buf[i++] != '\n');
        char num_buf[128];
        bzero(num_buf, 128);
        sprintf(num_buf, "%d\n", version);
        int fd1 = open(man_name, O_RDWR | O_TRUNC, 00600);
        if (better_write(fd1, num_buf, strlen(num_buf), __FILE__, __LINE__) != 1)
                return 1;
        if (better_write(fd1, &buf[i], strlen(&buf[i]), __FILE__, __LINE__) != 1)
                return 1;
        close(fd1);
        return 0;
}
