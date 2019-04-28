#include "server_main.h"
#include "server_cmds.h"
#include "fileIO.h"             // Socket + file IO
#include "flags.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>            // Threading
#include <sys/socket.h>         // ?
#include <netinet/in.h>         // Socket
#include <unistd.h>             // FILE IO
//#include <errno.h>              // Read Errors set

#define QUEUE_SIZE 20

/*
 *      Prints usage to stdout
 */
void usage()
{
        fprintf(stderr, "Usage: ./WTFserver <PORT NUMBER>\n");
}

/*
 *      Reads length of project name. Then reads project name from client.
 *      Input sd, a socket file descriptor, and an
 *      Returns NULL on error; otherwise returns pointer to project name.
 */
char * read_project_name(int sd)
{
        char name_len_str[4];
        bzero(name_len_str, 3);
        if (better_read(sd, name_len_str, 3, __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[client_comm] Error returned by better_read. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                return NULL;
        }
        int name_len;
        sscanf(name_len_str, "%d", &name_len);
        // Mutex needed?
        char * project_name = (char *) malloc(sizeof(char) * (name_len+1));
        bzero(project_name, name_len+1);
        if (better_read(sd, project_name, name_len, __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[client_comm] Error returned by better_read. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                return NULL;
        }
        return project_name;
}

/*
 *      Handles client communication through the inputted socket descriptor.
 */
void * client_comm(void * args)
{
        int sd = * ((int *) args);      // socket
        // Read command
        char command[4] = {0};
        if (better_read(sd, command, 3, __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[client_comm] Error returned by better_read. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                close(sd);
                pthread_exit(NULL);
        }
        printf("[client_comm] Command received: %s\n", command);
        char * proj_name = read_project_name(sd);
        if (proj_name == NULL)
        {
                fprintf(stderr, "Error\n");
                close(sd);
                pthread_exit(NULL);
        }
        printf("Project name: %s\n", proj_name);
        if (strcmp(command, "cre") == 0)
        {
                if (create(sd, proj_name))
                {
                        if (better_send(sd, "Error: Project already exists.", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                pthread_exit(NULL);
                        }
                }
                else
                {
                        if (better_send(sd, "Project successfully created!", 29, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                pthread_exit(NULL);
                        }
                }

        }
        else if (strcmp(command, "des") == 0)
        {
                if (destroy(proj_name))
                {
                        if (better_send(sd, "Error: Project does not exist.", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                pthread_exit(NULL);
                        }
                }
                else
                {
                        if (better_send(sd, "Project successfully deleted!", 29, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                pthread_exit(NULL);
                        }
                }
        }
        else if (strcmp(command, "che") == 0)
        {
                if (checkout(sd, proj_name))
                {
                        if (better_send(sd, "Error: Project does not exist.", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                pthread_exit(NULL);
                        }
                }
                else
                {
                        if (better_send(sd, "Project successfully checkedout!", 29, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                pthread_exit(NULL);
                        }
                }
        }
        else
        {
                fprintf(stderr, "[client_comm] Invalid command received.\n");
                if (better_send(sd, "Error: Invalid command received", 31, 0, __FILE__, __LINE__) <= 0)
                {
                        fprintf(stderr, "[client_comm] Error returned by better_read. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                        close(sd);
                        pthread_exit(NULL);
                }
                close(sd);
                pthread_exit(NULL);
        }
        /*
        if (better_send(sd, "", strlen(msg), 0, __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[get_configure] Error returned by better_read. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                close(sd);
                pthread_exit(NULL);
        }
        */
        free(proj_name);
        printf("-->Sent message successfully.\n");
        close(sd);
        pthread_exit(NULL);
}

int main(int argc, char * argv[])
{
        if (argc != 2)
        {
                usage();
                exit(EXIT_FAILURE);
        }

        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);

        char * PORT_STR = argv[1];
        int PORT;
        sscanf(PORT_STR, "%d", &PORT);

        // Create socket file descriptor
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        // Forcefully attaching socket to the port PORT
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                      &opt, sizeof(opt)))
        {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons( PORT );

        // Forcefully attaching socket to the port PORT
        if (bind(server_fd, (struct sockaddr *)&address,
                                     sizeof(address))<0)
        {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, QUEUE_SIZE) < 0)
        {
                perror("listen");
                exit(EXIT_FAILURE);
        }

        int i = 0;
        pthread_t thread_id[QUEUE_SIZE+10];
        while (TRUE)
        {
                if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                   (socklen_t*)&addrlen))<0)
                {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                //for each client request creates a thread and assign the client request to it to process
                //so the main thread can entertain next request
                if( pthread_create(&thread_id[i], NULL, client_comm, &new_socket) != 0 )
                        printf("Failed to create thread\n");
                else
                        i++;
                printf("%d\n", i);
                if( i >= QUEUE_SIZE)
                {
                        i = 0;
                        while(i < QUEUE_SIZE)
                                pthread_join(thread_id[i++], NULL);
                        i = 0;
                }
        }
        pthread_exit(NULL);

        return 0;
}
