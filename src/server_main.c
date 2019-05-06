#include "server_main.h"
#include "server_cmds.h"
#include "fileIO.h"             // Socket + file IO
#include "flags.h"
#include "threads_and_locks.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>            // Threading
#include <sys/socket.h>         // ?
#include <netinet/in.h>         // Socket
#include <unistd.h>             // FILE IO
#include <signal.h>             // signals
//#include <errno.h>              // Read Errors set

#define QUEUE_SIZE 20
extern pthread_mutex_t table_lck;
extern pthread_mutex_t access_lock;

typedef struct _thread_node {
        pthread_t thread_id;
        struct _thread_node * next;
} thread_node;
thread_node * front = NULL;

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
                printf("Disconnected client.\n");
                pthread_exit(NULL);
        }
        printf("[client_comm] Command received: %s\n", command);
        char * proj_name = read_project_name(sd);
        if (proj_name == NULL)
        {
                fprintf(stderr, "Error\n");
                close(sd);
                printf("Disconnected client.\n");
                better_send(sd, "Err", 3, 0, __FILE__, __LINE__);
                pthread_exit(NULL);
        }
        printf("Project name: %s\n", proj_name);
        if (better_send(sd, "Ok!", 3, 0, __FILE__, __LINE__) != 1)
        {
                fprintf(stderr, "Error\n");
                close(sd);
                pthread_exit(NULL);
        }
        if (strcmp(command, "cre") == 0)
        {
                if (create(sd, proj_name))
                {
                        if (better_send(sd, "Error: Project already exists.", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                printf("Disconnected client.\n");
                                pthread_exit(NULL);
                        }
                }
                else
                {
                        if (better_send(sd, "Project successfully created! ", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                printf("Disconnected client.\n");
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
                                printf("Disconnected client.\n");
                                pthread_exit(NULL);
                        }
                }
                else
                {
                        if (better_send(sd, "Project successfully deleted! ", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                printf("Disconnected client.\n");
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
                                printf("Disconnected client.\n");
                                pthread_exit(NULL);
                        }
                }
                else
                {
                        if (better_send(sd, "Project checkout successful!  ", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                printf("Disconnected client.\n");
                                pthread_exit(NULL);
                        }
                }
        }
        else if (strcmp(command, "man") == 0)
        {
                if (send_manifest(sd, proj_name))
                {
                        if (better_send(sd, "Error: Project does not exist.", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                printf("Disconnected client.\n");
                                pthread_exit(NULL);
                        }
                }
                else
                {
                        if (better_send(sd, "Sent manifest successfully!   ", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                printf("Disconnected client.\n");
                                pthread_exit(NULL);
                        }
                }
        }
        else if (strcmp(command, "fet") == 0) /* fet for "fetch file" */
        {
                printf("Fetching files mode.\n");
                /* kind of hacking it where client actualy sent file path to server not project name */
                if (send_server_copy(sd, proj_name))
                {
                        if (better_send(sd, "Error: Project does not exist.", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                printf("Disconnected client.\n");
                                pthread_exit(NULL);
                        }
                }
                else
                {
                        if (better_send(sd, "Sent file successfully!       ", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                printf("Disconnected client.\n");
                                pthread_exit(NULL);
                        }
                }
        }
        else if (strcmp(command, "com") == 0) /* com for commit */
        {
                if (receive_commit(sd, proj_name))
                {
                        if (better_send(sd, "Error: Project does not exist.", 30, 0, __FILE__, __LINE__) <= 0)
                        {
                                fprintf(stderr, "[client_comm] Error returned by better_send. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                                close(sd);
                                printf("Disconnected client.\n");
                                pthread_exit(NULL);
                        }
                }
        }
        else
        {
                fprintf(stderr, "[client_comm] Invalid command received.\n");
                if (better_send(sd, "Error:Invalid command received", 30, 0, __FILE__, __LINE__) <= 0)
                {
                        fprintf(stderr, "[client_comm] Error returned by better_read. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                        close(sd);
                        printf("Disconnected client.\n");
                        pthread_exit(NULL);
                }
                close(sd);
                printf("Disconnected client.\n");
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
        printf("Disconnected client.\n");
        pthread_exit(NULL);
}

void termination_handler (int signum)
{
        thread_node * ptr = front;
        int j = 1;
        while (ptr != NULL)
        {
                pthread_join(ptr->thread_id, NULL);
                 printf("Removed thread %d.\n", j++);
                thread_node * old = ptr;
                ptr = ptr->next;
                free(old);
        }
        printf("Server closed.\n");
        remove_dir(".server_repo");
}

int main(int argc, char * argv[])
{
        if (argc != 2)
        {
                usage();
                exit(EXIT_FAILURE);
        }

        if (make_dir(".server_repo", __FILE__, __LINE__) != 0)
        {
                fprintf(stderr, "Error making .server_repo\n");
                exit(EXIT_FAILURE);
        }
        // Setup signals
        struct sigaction new_action, old_action;
        /* Set up the structure to specify the new action. */
        new_action.sa_handler = termination_handler;
        sigemptyset (&new_action.sa_mask);
        new_action.sa_flags = 0;

        sigaction (SIGINT, NULL, &old_action);
        if (old_action.sa_handler != SIG_IGN)
                sigaction (SIGINT, &new_action, NULL);
        sigaction (SIGHUP, NULL, &old_action);
        if (old_action.sa_handler != SIG_IGN)
                sigaction (SIGHUP, &new_action, NULL);
        sigaction (SIGTERM, NULL, &old_action);
        if (old_action.sa_handler != SIG_IGN)
                sigaction (SIGTERM, &new_action, NULL);

        // setup server
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
        if (pthread_mutex_init(&table_lck, NULL) != 0)
        {
                fprintf(stderr, "[main] Mutex init failed. FILE: %s. LINE: %d.\n", __FILE__, __LINE__);
                return 1;
        }
        if (pthread_mutex_init(&access_lock, NULL) != 0)
        {
                fprintf(stderr, "[main] Mutex init failed. FILE: %s. LINE: %d.\n", __FILE__, __LINE__);
                return 1;
        }

        int queued = 0;
        front = NULL;
        while (TRUE)
        {
                if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                   (socklen_t*)&addrlen))<0)
                {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                printf("test\n");
                thread_node * new = (thread_node *) malloc(sizeof(thread_node));
                new->next = front;
                front = new;
                //for each client request creates a thread and assign the client request to it to process
                //so the main thread can entertain next request
                if( pthread_create(&new->thread_id, NULL, client_comm, &new_socket) != 0 )
                        printf("Failed to create thread\n");
                else
                        printf("Succesfully accepted new client.\n");

                queued++;
                printf("%d\n", queued);

                while (queued >= QUEUE_SIZE)
                {
                        thread_node * ptr = front;
                        while (ptr->next != NULL)
                        {
                                if (ptr->next->next == NULL)
                                {
                                        thread_node * prev = ptr;
                                        ptr = ptr->next;
                                        prev->next = NULL;
                                }
                                else
                                        ptr = ptr->next;

                        }
                        pthread_join(ptr->thread_id, NULL);
                         printf("Removed thread %d.\n", queued);
                        free(ptr);
                        queued--;

                }
        }
        pthread_exit(NULL);

        return 0;
}
