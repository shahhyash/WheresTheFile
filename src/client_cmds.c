#include "client_cmds.h"
#include "flags.h"
#include "fileIO.h"             // Socket + file IO
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>             // FILE IO
#include <fcntl.h>              // open flags
#include <arpa/inet.h>          // socket
#include <netdb.h>              // gethostbyname
#include <openssl/sha.h>        // Hash function


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
            printf("\n Socket creation error \n");
            return -1;
        }

        memset(&serv_addr, '0', sizeof(serv_addr));

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if(inet_pton(AF_INET, IP, &serv_addr.sin_addr)<=0)
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("\nConnection Failed \n");
            return -1;
        }
        return sock;
}

/*
 *      Writes IP and port to .configure file. Overwrites .configure if it already
 *      exists. Returns 0 on success; 1 otherwise.
 */
int set_configure(char * IP, char * port)
{
        int fd = open(".configure", O_WRONLY | O_CREAT | O_TRUNC, 00600);
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
        int proj_name_length = strlen(proj_name);
        char msg[6 + proj_name_length];
        bzero(msg, 6 + proj_name_length);
        char * leading_zeros = "";
        if (proj_name_length < 10)
                leading_zeros = "00";
        else if (proj_name_length < 100)
                leading_zeros = "0";
        char * cmd = "cre";
        if (!create)
                cmd = "des";
        sprintf(msg, "%s%s%d%s", cmd, leading_zeros, proj_name_length, proj_name);
        if (better_send(sock , msg , strlen(msg), 0, __FILE__, __LINE__) <= 0)
                return 1;
        if (create)
        {
                // Read length of size of file
                char file_size_length_str[4] = {0,0,0,0};
                if (better_read(sock , file_size_length_str , 3, __FILE__, __LINE__) <= 0)
                        return 1;
                int file_size_length = 1;
                sscanf(file_size_length_str, "%d", &file_size_length);
                // Read size of file
                char file_size_str[file_size_length+1];
                bzero(file_size_str, file_size_length+1);
                if (better_read(sock , file_size_str , file_size_length, __FILE__, __LINE__) <= 0)
                        return 1;
                // Read file bytes
                int file_size;
                sscanf(file_size_str, "%d", &file_size);
                char file[file_size+1];
                bzero(file, file_size+1);
                if (better_read(sock , file , file_size, __FILE__, __LINE__) <= 0)
                        return 1;
                /* Create local directory for project */
                if (make_dir(proj_name, 0777) != 0)
                        return 1;
                // Create .manifest
                char manifest[strlen("/.manifest") + strlen(proj_name)];
                sprintf(manifest, "%s/.manifest", proj_name);
                int fd_man = open(manifest, O_WRONLY | O_CREAT | O_TRUNC, 00600);
                if (better_write(fd_man, file, file_size, __FILE__, __LINE__))
                        return 1;
                close(fd_man);
        }
        char buffer[30] = {0};
        printf("-->Sent message successfully.\n");
        read( sock , buffer, 1024);
        printf("Message from server:\t%s\n", buffer);
        return 0;
}

/*
 *      Adds filename to the .manifest in the project proj_name.
 *      Returns 1 on error, 0 on success.
 */
int _add(char * proj_name, char * filename)
{
        // Remove if already in manifest
        if(!_remove(proj_name, filename))
                printf("[_add] File already in .manifest... replacing...\n");
        int pj_len = strlen(filename) + strlen(proj_name) + 2;
        char pj[pj_len];
        bzero(pj, pj_len);
        sprintf(pj, "%s/%s", proj_name, filename);
        int fd_new_file = open(pj, O_RDWR, 00600);
        if (fd_new_file < 0)
        {
                fprintf(stderr, "[add] Error opening %s. FILE: %s. LINE: %d.\n", filename, __FILE__, __LINE__);
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
                fprintf(stderr, "[add] Error opening %s. FILE: %s. LINE: %d.\n", manifest, __FILE__, __LINE__);
                return 1;
        }
        // Read version number
        /*
        int i = 0;
        while (TRUE)
        {
                char temp[2] = {0,0};
                if (better_read(fd_man, temp, 1, __FILE__, __LINE__) != 1)
                        return 1;
                if (*temp == '\n')
                        break;
                i++;

        }
        lseek(fd_man, 0, SEEK_SET);
        char ver_str[i+1];
        bzero(ver_str, i+1);
        i = 0;
        while (TRUE)
        {
                char temp[2] = {0,0};
                if (better_read(fd_man, temp, 1, __FILE__, __LINE__) != 1)
                        return 1;
                if (*temp == '\n')
                        break;
                ver_str[i] = *temp;
        }
        */
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
        int fd = open(pj, O_RDWR, 00600);
        int size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        char buf[size];
        better_read(fd, buf, size, __FILE__, __LINE__);
        printf("pj: %s\nbuf: %s\n", name, buf);
        char * line = strstr(buf, name);
        if (line == NULL)
        {
                fprintf(stderr, "[_remove] %s not in manifest. FILE: %s. LINE: %d.\n", filename, __FILE__, __LINE__);
                return 1;
        }
        int num_bytes = line - &buf[0];
        int i = num_bytes;
        int end;
        while (buf[i] != '\n')
                i++;
        end = i+1;
        printf("%d\n", num_bytes);
        close(fd);
        int fd1 = open(pj, O_RDWR | O_TRUNC, 00600);
        better_write(fd1, buf, num_bytes-2, __FILE__, __LINE__);
        better_write(fd1, &buf[end], strlen(buf)-end, __FILE__, __LINE__);

        close(fd1);
        printf("[_add] File removed successfully.\n");
        return 0;
}

/*
 *
 */
int checkout(char * proj_name)
{
        if (dir_exists(proj_name))
        {
                fprintf(stderr, "[checkout] Project already exists locally.\n");
                return 1;
        }

        return 0;
}
