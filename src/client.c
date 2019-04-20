#include "client.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "fileIO.h"
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

void usage()
{
        fprintf(stderr, "Usage: ./WTF <PORT NUMBER>\n");
        exit(EXIT_FAILURE);
}

int init_socket()
{
        char IP_str[16];
        int PORT;
        get_configure(IP_str, &PORT);
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

int get_configure(char * IP, int * PORT)
{
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
        {
                fprintf(stderr, "[create] Error returned from better_send.\n");
        }
        char buffer[1024] = {0};
        printf("-->Sent message successfully.\n");
        read( sock , buffer, 1024);
        printf("Message from server:\t%s\n",buffer );
        return 0;
}

int main(int argc, char * argv[])
{


        if (argc < 3)
        {
                usage();
        }
        if (strcmp(argv[1], "configure") == 0)
        {
                if (argc != 4)
                {
                        usage();
                }
                return set_configure(argv[2], argv[3]);
        }
        else if (strcmp(argv[1], "create") == 0)
        {
                return create_or_destroy(argv[2], TRUE);
        }
        else if (strcmp(argv[1], "destroy") == 0)
        {
                return create_or_destroy(argv[2], FALSE);
        }
        else
        {
                int sock = init_socket();

                char *msg = "cre004test";

                char buffer[1024] = {0};
                better_send(sock , msg , strlen(msg) , 0, __FILE__, __LINE__);

                printf("-->Sent message successfully.\n");
                read( sock , buffer, 1024);
                printf("Message from server:\t%s\n",buffer );
        }
        return 0;
}
