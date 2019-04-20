#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "fileIO.h"
#include "server.h"

#define QUEUE_SIZE 3

void usage()
{
        fprintf(stderr, "Usage: ./WTFserver <PORT NUMBER>\n");
}

void * client_comm(void * args)
{
        int new_socket = * ((int *) args);
        char buffer[1024] = {0};
        char * msg = "No, winter is here.";
        read(new_socket , buffer, 1024);

        printf("Message from client:\t%s\n",buffer );
        send(new_socket , msg , strlen(msg) , 0 );
        printf("-->Sent message successfully.\n");
        close(new_socket);
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

                if( i >= QUEUE_SIZE)
                {
                  i = 0;
                  while(i < QUEUE_SIZE)
                  {
                    pthread_join(thread_id[i++], NULL);
                  }
                  i = 0;
                }
        }
        pthread_exit(NULL);

        return 0;
}
