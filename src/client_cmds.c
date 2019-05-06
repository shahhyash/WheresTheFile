#include "client_cmds.h"
#include "flags.h"
#include "fileIO.h"             // Socket + file IO
#include "manifest_utils.h"     // Functions for .manifest analysis
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>             // FILE IO
#include <fcntl.h>              // open flags
#include <arpa/inet.h>          // socket
#include <netdb.h>              // gethostbyname
#include <openssl/sha.h>        // Hash function
#include "compression.h"
#include <string.h>

/*
 *      Returns file descriptor to the socket as specified by the PORT and IP
 *      in .configure. Returns -1 on error.
 */
int init_socket()
{
        char IP_str[16];
        int PORT;
        if (get_configure(IP_str, &PORT))
                return -1;
        struct hostent * host = gethostbyname(IP_str);
        char * IP = host->h_name;
        //struct sockaddr_in address;
        int sock = 0;

        struct sockaddr_in serv_addr;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            fprintf(stderr, "\n Socket creation error \n");
            return -1;
        }

        memset(&serv_addr, '0', sizeof(serv_addr));

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if(inet_pton(AF_INET, IP, &serv_addr.sin_addr)<=0)
        {
            fprintf(stderr, "\nInvalid address/ Address not supported \n");
            return -1;
        }
        int ret;
        do {
                ret = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0;
                if (ret)
                {
                    fprintf(stderr, "\nConnection Failed. Trying again in 3 seconds \n");
                    sleep(3);
                }
        } while(ret);
        printf("Client has successfully connected to server.\n");


        return sock;
}

/*
 *      Sends project name and command to server. Handles communication.
 *      Returns 0 on success, 1 otherwise.
 */
int send_cmd_proj(int sock, char * proj_name, char * cmd)
{
        int proj_name_length = strlen(proj_name);
        char msg[6 + proj_name_length];
        bzero(msg, 6 + proj_name_length);
        char * leading_zeros = "";
        if (proj_name_length < 10)
                leading_zeros = "00";
        else if (proj_name_length < 100)
                leading_zeros = "0";
        sprintf(msg, "%s%s%d%s", cmd, leading_zeros, proj_name_length, proj_name);
        if (better_send(sock , msg , strlen(msg), 0, __FILE__, __LINE__) <= 0)
                return 1;
        char buf[4];
        bzero(buf, 4);
        if (better_read(sock, buf, 3, __FILE__, __LINE__) != 1)
                return 1;
        if (strcmp(buf, "Err") == 0)
        {
                fprintf(stderr, "Error returned by server.\n");
                return 1;
        }
        printf("Message from server:\t%s\n", buf);
        return 0;
}

/*
 *      Writes IP and port to .configure file. Overwrites .configure if it already
 *      exists. Returns 0 on success; 1 otherwise.
 */
int set_configure(char * IP, char * port)
{
        int fd = open(".configure", O_WRONLY | O_CREAT | O_TRUNC, 00600);
        if (fd == -1)
        {
                fprintf(stderr, "[configure] Error creating .configure. FILE %s. LINE: %d", __FILE__, __LINE__);
                return 1;
        }
        if (better_write(fd, IP, strlen(IP), __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[configure] Error returned by better_write. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                return 1;
        }
        if (better_write(fd, " ", 1, __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[configure] Error returned by better_write. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                return 1;
        }
        if (better_write(fd, port, strlen(port), __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[configure] Error returned by better_write. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                return 1;
        }
        close(fd);
        return 0;
}

/*
 *      Reads IP and PORT from .configure file and stores them in the passed pointers.
 *      Returns 0 on sucess; 1 otherwise.
 */
int get_configure(char * IP, int * PORT)
{
        if (!file_exists(".configure"))
        {
                fprintf(stderr, "[get_configure] No .configure file found.\n");
                return 1;
        }
        int fd = open(".configure", O_RDONLY, 00600);
        if (fd == -1)
        {
                fprintf(stderr, "[get_configure] Error reading .configure. FILE %s. LINE: %d", __FILE__, __LINE__);
                return 1;
        }
        int size = lseek(fd, 0, SEEK_END);
        char buffer[size+1];
        lseek(fd, 0, SEEK_SET);
        if (better_read(fd, buffer, size, __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[get_configure] Error returned by better_read. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                return 1;
        }
        buffer[size] = '\0';
        close(fd);
        char * tokens = strtok(buffer, " ");
        strcpy(IP, tokens);
        //printf("%s\n", IP);
        tokens = strtok(NULL, " ");
        sscanf(tokens, "%d", PORT);
        //printf("%d\n", *PORT);
        return 0;
}

/*
 *      Sends create (if create is TRUE) or destroy (if create is FALSE) commands to the
 *      server with the corresponding proj_name. Returns 0 on sucess; 1 otherwise.
 */
int create_or_destroy(char * proj_name, int create)
{
        int sock = init_socket();

        char * cmd = "cre";
        if (!create)
        cmd = "des";
        if (send_cmd_proj(sock, proj_name, cmd))
                return 1;


        if (create)
        {
                char buf[31];
                bzero(buf, 31);
                if (better_read(sock, buf, 30, __FILE__, __LINE__) != 1)
                        return 1;
                printf("Message received from server: %s\n", buf);
                if (strcmp(buf, "Error: Project does not exist.") == 0)
                {
                        fprintf(stderr, "Server returned error.\n");
                        return 1;
                }
                char * decompressed = receive_file(sock);
                if (decompressed == NULL)
                {
                        fprintf(stderr, "[fetch_server_manifest] Error decompressing.\n");
                        return 1;
                }

                printf("decompressed %s\n", decompressed);
                char * newline = strstr(decompressed, "\n");
                int size;
                sscanf(&newline[2], "%d\n", &size);
                printf("size %d\n", size);
                char * file = (char *) malloc(sizeof(char)*(size+1));
                bzero(file, size+1);
                int i = 3;
                while (newline[i++] != '\n');
                strncpy(file, &newline[i], size);
                printf("file %s\n", file);
                free(decompressed);

                printf("%s\n", proj_name);
                if (make_dir(proj_name, __FILE__, __LINE__) != 0)
                {
                        free(file);
                        return 1;
                }
                // Create .manifest
                char manifest[strlen("/.manifest") + strlen(proj_name)];
                sprintf(manifest, "%s/.manifest", proj_name);
                int fd_man = open(manifest, O_WRONLY | O_CREAT | O_TRUNC, 00600);
                if (fd_man == -1)
                {
                        fprintf(stderr, "[create] Error creating .manifest. FILE %s. LINE: %d", __FILE__, __LINE__);\
                        free(file);
                        return 1;
                }
                if (better_write(fd_man, file, strlen(file), __FILE__, __LINE__) != 1)
                {
                        free(file);
                        return 1;
                }
                close(fd_man);
                free(file);
        }
        char buffer[31] = {0};
        printf("-->Sent message successfully.\n");
        if (better_read( sock , buffer, 30, __FILE__, __LINE__) != 1)
                return 1;
        printf("Message from server:\t%s\n", buffer);
        return 0;
}

/*
 *      Adds filename to the .manifest in the project proj_name.
 *      Returns 1 on error, 0 on success.
 */
int _add(char * proj_name, char * filename)
{
        int pj_len = strlen(filename) + strlen(proj_name) + 2;
        char pj[pj_len];
        bzero(pj, pj_len);
        sprintf(pj, "%s/%s", proj_name, filename);
        if (!file_exists(pj))
        {
                fprintf(stderr, "[_add] Error %s does not exist. FILE: %s. LINE: %d.\n", pj, __FILE__, __LINE__);
                return 1;
        }
        // Remove if already in manifest
        if(!_remove(proj_name, filename))
                printf("[_add] File already in .manifest... replacing...\n");
        int fd_new_file = open(pj, O_RDWR, 00600);
        if (fd_new_file < 0)
        {
                fprintf(stderr, "[_add] Error opening %s. FILE: %s. LINE: %d.\n", filename, __FILE__, __LINE__);
                return 1;
        }

        // Get file length
        int file_length = lseek(fd_new_file, 0, SEEK_END);
        lseek(fd_new_file, 0, SEEK_SET);
        // Hash contents of file
        char data[file_length+1];
        bzero(data, file_length+1);
        if (better_read(fd_new_file, data, file_length, __FILE__, __LINE__) != 1)
                return 1;
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256((const unsigned char *) data, file_length, hash);
        close(fd_new_file);

        // Open manifest
        char manifest[strlen("/.manifest") + strlen(proj_name)];
        sprintf(manifest, "%s/.manifest", proj_name);
        int fd_man = open(manifest, O_RDWR, 00600);
        if (fd_man < 0)
        {
                fprintf(stderr, "[_add] Error opening %s. FILE: %s. LINE: %d.\n", manifest, __FILE__, __LINE__);
                return 1;
        }
        char * ver_str = "1";   // Version 1 since just added
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
        int line_length = strlen(ver_str) + 1 + strlen(pj) + 1 + strlen(hex_hash) + 2;
        char new_line[line_length];
        bzero(new_line, line_length);
        sprintf(new_line, "%s %s %s\n", ver_str, pj, hex_hash);
        lseek(fd_man, 0, SEEK_END);
        if (better_write(fd_man, new_line, line_length-1, __FILE__, __LINE__) != 1)
                return 1;
        close(fd_man);
        printf("[_add] File added successfully.\n");
        return 0;
}

/*
 *      Removes filename to the .manifest in the project proj_name.
 *      Returns 1 on error, 0 on success.
 */
int _remove(char * proj_name, char * filename)
{
        int pj_len = strlen(filename) + strlen(proj_name) + 2;
        char pj[pj_len];
        bzero(pj, pj_len);
        sprintf(pj, "%s/.manifest", proj_name);
        int length = strlen(filename) + strlen(proj_name) + 2;
        char name[length];
        bzero(name, length);
        sprintf(name, "%s/%s", proj_name, filename);
        if (!file_exists(pj))
        {
                fprintf(stderr, "[_add] Error %s does not exist. FILE: %s. LINE: %d.\n", pj, __FILE__, __LINE__);
                return 1;
        }
        int fd = open(pj, O_RDWR, 00600);
        if (fd == -1)
        {
                fprintf(stderr, "[_remove] Error opening %s. FILE: %s. LINE: %d.\n", pj, __FILE__, __LINE__);
                return 1;
        }
        int size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        char buf[size+1];
        if (better_read(fd, buf, size, __FILE__, __LINE__) != 1)
                return 1;
        buf[size]='\0'; /* add string termination so that substring search doesn't look further than end of file */
        // printf("pj: %s\nbuf: %s\n", name, buf);
        char * line = strstr(buf, name);
        if (line == NULL)
        {
                fprintf(stderr, "[_remove] (Ignore if adding...) %s not in manifest. FILE: %s. LINE: %d.\n", filename, __FILE__, __LINE__);
                return 1;
        }
        int num_bytes = line - &buf[0];
        int i = num_bytes;
        int end;
        while (buf[i] != '\n')
                i++;
        end = i+1;
        // printf("%d\n", num_bytes);
        close(fd);
        int fd1 = open(pj, O_RDWR | O_TRUNC, 00600);
        if (fd1 == -1)
        {
                fprintf(stderr, "[_remove] Error opening %s. FILE: %s. LINE: %d.\n", pj, __FILE__, __LINE__);
                return 1;
        }
        if (better_write(fd1, buf, num_bytes-2, __FILE__, __LINE__) != 1)
                return 1;
        if (better_write(fd1, &buf[end], size-end, __FILE__, __LINE__) != 1)
                return 1;

        close(fd1);
        printf("[_remove] File removed successfully.\n");
        return 0;
}

/*
 *      Clones repository from server. Returns 0 on success; 1 otherwise.
 */
int checkout(char * proj_name)
{
        if (dir_exists(proj_name))
        {
                fprintf(stderr, "[checkout] Project already exists locally.\n");
                return 1;
        }
        int sock = init_socket();

        if (send_cmd_proj(sock, proj_name, "che"))
                return 1;

        char buf[31];
        bzero(buf, 31);
        if (better_read(sock, buf, 30, __FILE__, __LINE__) != 1)
                return 1;
        printf("Message received from server: %s\n", buf);
        if (strcmp(buf, "Error: Project does not exist.") == 0)
        {
                fprintf(stderr, "Server returned error.\n");
                return 1;
        }
        char * decompressed = receive_file(sock);
        printf("decompressed %s\n", decompressed);
        recursive_unzip(decompressed);
        free(decompressed);
        return 0;
}

void write_to_update(int fd, char code, char * file_path)
{
        int size = strlen(file_path) + 4;
        char new_update[size];
        bzero(new_update, size);
        sprintf(new_update, "%c %s\n", code, file_path);
        lseek(fd, 0, SEEK_END);
        better_write(fd, new_update, size-1, __FILE__, __LINE__);
}

/*
 *      Creates .update file to track any server changes not present in local version
 *      Returns 0 on success, 1 otherwise.
 */
int _update(char * proj_name)
{
        /* check if local version of project exists */
        if (!dir_exists(proj_name))
        {
                fprintf(stderr, "[update] Project does not exist locally.\n");
                return 1;
        }

        /* check if .Update exists from previous iteration - if it does, direct user to run upgrade first */
        char dot_update_path[strlen(proj_name) + strlen("/.Update")];
        sprintf(dot_update_path, "%s/.Update", proj_name);
        if (file_exists(dot_update_path))
        {
                fprintf(stderr, "[update] An existing .Update file already exists. Please run upgrade before running update again.\n");
                return 1;
        }

        /* begin by initializing a connection to the server */
        int sd = init_socket();
        if (sd == -1)
        {
                fprintf(stderr, "[_update] Error connecting to server.");
                return 1;
        }
        /* fetch manifest file from server and store in a linked list */
        char * manifest_contents = fetch_server_manifest(sd, proj_name);
        if (manifest_contents == NULL)
        {
                fprintf(stderr, "[_update] Error creating manifest. FILE %s. LINE: %d.\n", __FILE__, __LINE__);
                return 1;
        }
        manifest_entry * server_manifest = read_manifest_file(manifest_contents);
        free(manifest_contents); /* free buffer of manifest file bc we don't need it anymore */

        /* fetch manifest file from client and store in a linked list */
        manifest_contents = fetch_client_manifest(proj_name);
        manifest_entry * client_manifest = read_manifest_file(manifest_contents);
        free(manifest_contents);

        /* recompute hashes for all files in client manifest */
        update_hashes(client_manifest);

        /* create and open .Update file */
        int fd_update = open(dot_update_path, O_RDWR | O_CREAT, 00600);
        if (fd_update == -1)
        {
                fprintf(stderr, "[_update] Error opening file %s. FILE: %s. LINE: %d\n", dot_update_path, __FILE__, __LINE__);
                return 1;
        }

        /* count number of updates */
        int num_updates = 0;

        /* Flag if conflicts were found */
        int conflicts_found = 0;

        /* iterate through all items in server manifest and compare against client copy */
        manifest_entry * server_copy = server_manifest;
        manifest_entry * client_copy = NULL;
        while (server_copy != NULL)
        {
                if (server_copy->file_path && server_copy->hash_code) /* check if manifest item is not the root item */
                {
                        client_copy = client_manifest;
                        while (client_copy != NULL)
                        {
                                if (client_copy->file_path)
                                {
                                        int cmp = strcmp(server_copy->file_path, client_copy->file_path);
                                        if (cmp == 0)
                                                break;
                                }
                                client_copy = client_copy->next;
                        }

                        /* successfully found manifest entry for same file path */
                        if (client_copy != NULL)
                        {
                                int version_cmp = server_copy->version - client_copy->version;
                                printf("%s vs %s\n", "server_copy->hash_code", client_copy->hash_code);
                                int hash_cmp = strcmp(server_copy->hash_code, client_copy->hash_code);

                                if (version_cmp == 0 && hash_cmp == 0)
                                {
                                        /* file is the same, do nothing */
                                }
                                else if (version_cmp == 0 && hash_cmp != 0)
                                {
                                        write_to_update(fd_update, 'U', server_copy->file_path);
                                        ++num_updates;
                                }
                                else if (version_cmp != 0 && hash_cmp == 0)
                                {
                                        if (server_manifest->version != client_manifest->version)
                                        {
                                                write_to_update(fd_update, 'M', server_copy->file_path);
                                                ++num_updates;
                                        }
                                        else
                                        {
                                                fprintf(stderr, "ERROR: Local manifest file may be corrupted. %s has different version numbers despite manifest versions being the same. This should not ever occur.\n", client_copy->file_path);
                                                free_manifest(client_manifest);
                                                free_manifest(server_manifest);
                                                return 1;
                                        }

                                }
                                else
                                {
                                        if (server_manifest->version != client_manifest->version)
                                        {
                                                fprintf(stderr, "CONFLICT: %s\n", server_copy->file_path);
                                                conflicts_found = 1;
                                                ++num_updates;
                                        }
                                        else
                                        {
                                                fprintf(stderr, "ERROR: Local manifest file may be corrupted. %s has different version numbers despite manifest versions being the same. This should not ever occur.\n", client_copy->file_path);
                                                free_manifest(client_manifest);
                                                free_manifest(server_manifest);
                                                return 1;
                                        }

                                }
                        }
                        else    /* file exists on server copy, but not in clients, should probably be added */
                        {
                                write_to_update(fd_update, 'A', server_copy->file_path);
                                ++num_updates;
                        }

                }

                /* go to next item in manifest */
                server_copy = server_copy->next;
        }

        /* iterate through client to see if there's any files that need to be deleted */
        client_copy = client_manifest->next;
        while (client_copy != NULL)
        {
                int found = 0;
                server_copy = server_manifest->next;
                while (server_copy)
                {
                        int file_path_cmp = strcmp(client_copy->file_path, server_copy->file_path);
                        if (file_path_cmp == 0)
                        {
                                found = 1;
                                break;
                        }

                        server_copy = server_copy->next;
                }

                if (!found)
                {
                        if (server_manifest->version == client_manifest->version)
                        {
                                /* file needs to be uploaded */
                                write_to_update(fd_update, 'U', server_copy->file_path);
                                ++num_updates;
                        }
                        else
                        {
                                /* client copy probably doesn't exist on server anymore, mark for deletion */
                                write_to_update(fd_update, 'D', client_copy->file_path);
                                ++num_updates;
                        }
                }

                client_copy = client_copy->next;
        }

        if (num_updates == 0)
        {
                printf("Client copy is up to date.\n");
        }
        else
        {
                if(conflicts_found)
                {
                        printf("Please fix the conflicts listed above before running update again.\n");
                        close(fd_update);
                        remove(dot_update_path);
                        free_manifest(client_manifest);
                        free_manifest(server_manifest);
                        return 1;
                }
                else
                {
                        lseek(fd_update, 0, SEEK_SET);
                        int file_length = lseek(fd_update, 0, SEEK_END);
                        lseek(fd_update, 0, SEEK_SET);
                        char update_buf[file_length+1];
                        if(better_read(fd_update, update_buf, file_length, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[update] ERROR: Could not read from .Update file\n");
                                return 1;
                        }
                        close(fd_update);
                        update_buf[file_length]='\0';
                        printf("%s", update_buf);
                }

        }

        free_manifest(client_manifest);
        free_manifest(server_manifest);

        return 0;
}

/* fetch file from server given it's file path: proj_name/sub_dir/file_name */
char * fetch_server_copy(int sd, char * file_name)
{
        if (send_cmd_proj(sd, file_name, "fet"))
                return NULL;

        char buf[31];
        bzero(buf, 31);
        if (better_read(sd, buf, 30, __FILE__, __LINE__) != 1){
                return NULL;}
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
        char * newline = strstr(decompressed, "\n");
        int size;
        // printf("newline %s\n", newline);
        sscanf(&newline[2], "%d\n", &size);
        // printf("size %d\n", size);
        char * file = (char *) malloc(sizeof(char)*(size+1));
        bzero(file, size+1);
        newline = strstr(&newline[2], "\n");
        newline = strstr(&newline[1], "\n");
        // printf("newline %s\n", newline);
        free(decompressed);
        strncpy(file, &newline[1], size);
        return file;
}

int _upgrade(char * proj_name)
{
        /* check if local version of project exists */
        if (!dir_exists(proj_name))
        {
                fprintf(stderr, "[_upgrade] ERROR: Project does not exist locally.\n");
                return 1;
        }

        /* check if .Update exists from previous iteration - if it does, direct user to run upgrade first */
        char dot_update_path[strlen(proj_name) + strlen("/.Update")];
        sprintf(dot_update_path, "%s/.Update", proj_name);
        if (!file_exists(dot_update_path))
        {
                fprintf(stderr, "[_upgrade] ERROR: An existing .Update file does not exist. Please run update before running upgrade.\n");
                return 1;
        }

        /* begin by initializing a connection to the server */
        int sd = init_socket();
        if (sd == -1)
        {
                fprintf(stderr, "[_upgrade] Error connecting to server.");
                return 1;
        }

        /* fetch manifest file from server and store in a linked list - if it is unable to fetch from server it might not be a valid project */
        char * manifest_contents = fetch_server_manifest(sd, proj_name);
        if (manifest_contents == NULL)
        {
                fprintf(stderr, "[_update] Error fetching manifest. FILE %s. LINE: %d.\n", __FILE__, __LINE__);
                return 1;
        }
        char svr_msg[31];
        bzero(svr_msg, 31);
        if (better_read(sd, svr_msg, 30, __FILE__, __LINE__) != 1)
        {
                return 1;
        }
        close(sd);
        sd = init_socket();
        if (sd == -1)
        {
                fprintf(stderr, "[_upgrade] Error connecting to server.");
                return 1;
        }
        manifest_entry * server_manifest = read_manifest_file(manifest_contents);

        free(manifest_contents); /* free buffer of manifest file bc we don't need it anymore */
        if (server_manifest == NULL)
        {
                fprintf(stderr, "[upgrade] Error fetching server_manifest. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                /* free allocated resources */
                free_manifest(server_manifest);
                return 1;
        }

        /* fetch list of required updates for this project */
        update_entry * updates = fetch_updates(proj_name);
        if (server_manifest == NULL)
        {
                fprintf(stderr, "[upgrade] Error fetching updates. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                /* free allocated resources */
                free_manifest(server_manifest);

                free_updates(updates);
                return 1;
        }
        update_entry * update_ptr = updates;

        while (update_ptr)
        {
                if (update_ptr->code == 'M')
                {
                        char * file_contents = fetch_server_copy(sd, update_ptr->file_path);

                        if (file_contents == NULL)
                        {
                                fprintf(stderr, "[upgrade] Error fetching server copy. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                /* free allocated resources */
                                free_manifest(server_manifest);
                                free_updates(updates);
                                return 1;
                        }
                        int fd = open(update_ptr->file_path, O_WRONLY | O_TRUNC, 00600);
                        if (fd == -1)
                        {
                                fprintf(stderr, "[upgrade] Error opening file %s for modification. Exiting process...\n", update_ptr->file_path);
                                /* free allocated resources */
                                free_manifest(server_manifest);
                                free_updates(updates);
                                return 1;
                        }

                        if (better_write(fd, file_contents, strlen(file_contents), __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[upgrade] Unable to write to file %s during modification. Exiting process...\n", update_ptr->file_path);
                                /* free allocated resources */
                                free_manifest(server_manifest);
                                free_updates(updates);
                                return 1;
                        }
                        char * f_name = strstr(update_ptr->file_path, "/");
                        _add(proj_name, &f_name[1]);

                        close(fd);
                        free(file_contents);
                }
                else if (update_ptr->code == 'A')
                {
                        char * file_contents = fetch_server_copy(sd, update_ptr->file_path);
                        if (file_contents == NULL)
                        {
                                fprintf(stderr, "[upgrade] Error fetching server copy. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                /* free allocated resources */
                                free_manifest(server_manifest);
                                free_updates(updates);
                                return 1;
                        }
                        int fd = open(update_ptr->file_path, O_WRONLY | O_CREAT, 00600);
                        if (fd == -1)
                        {
                                fprintf(stderr, "[upgrade] Error opening file %s . Exiting process...\n", update_ptr->file_path);
                                /* free allocated resources */
                                free_manifest(server_manifest);
                                free_updates(updates);
                                return 1;
                        }

                        if (better_write(fd, file_contents, strlen(file_contents), __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[upgrade] Unable to write to file %s during modification. Exiting process...\n", update_ptr->file_path);
                                /* free allocated resources */
                                free_manifest(server_manifest);
                                free_updates(updates);
                                return 1;
                        }
                        char * f_name = strstr(update_ptr->file_path, "/");
                        _add(proj_name, &f_name[1]);
                        close(fd);
                        free(file_contents);
                }
                else if (update_ptr->code == 'D')
                {
                        int filepath_start = strlen(proj_name) + 1;
                        printf("[upgrade] Removing file %s from manifest for project %s.\n", &update_ptr->file_path[filepath_start], proj_name);
                        _remove(proj_name, &update_ptr->file_path[filepath_start]);
                }
                else
                {
                        fprintf(stderr, "[upgrade] ERROR: Invalid update code read: %c.\n", update_ptr->code);
                }

                update_ptr = update_ptr->next;
        }

        /* free allocated resources */
        free_manifest(server_manifest);
        free_updates(updates);

        /* remove update file after performing operations */
        char update_path[strlen("/.Update" + strlen(proj_name) + 1)];
        sprintf(update_path, "%s/.Update", proj_name);
        remove(update_path);

        return 0;
}
/*
 *      Requests server manifest and outputs a list of all files under the project name along with their version numbers
 *      Returns 0 on success, 1 otherwise.
 */
int current_version(char * proj_name)
{
        int sock = init_socket();
        if (sock == -1)
                return 1;
        char *  manifest = fetch_server_manifest(sock, proj_name);
        if (manifest == NULL)
                return 1;
        // printf("man %s\n", manifest);
        printf("Current project version:\n");
        char * token = strtok(manifest, "\n");
        printf("%s %s\n", token, proj_name);
        token = strtok(NULL, "\n");
        while (token != NULL) {
                int i = 0;
                while(token[i++] != ' ');
                while(token[i++] != ' ');
                token[i-1] = '\0';
                printf("%s\n", token);
                token = strtok(NULL, "\n");
        }
        return 0;
}
