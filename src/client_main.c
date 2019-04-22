#include "client_main.h"
#include "client_cmds.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fileIO.h"             // Socket + file IO
#include <unistd.h>             // FILE IO
#include "flags.h"

void usage()
{
        fprintf(stderr, "Usage: ./WTF <PORT NUMBER>\n");
        exit(EXIT_FAILURE);
}

int main(int argc, char * argv[])
{

        char * s = _compress("test.txt");
        free(_decompress(s, 223));
        free(s);
        exit(0);
        if (argc < 3)
        {
                usage();
        }
        if (strcmp(argv[1], "configure") == 0)
        {
                if (argc != 4)
                {
                        fprintf(stderr, "Usage: ./WTF <PORT NUMBER>\n");
                        exit(EXIT_FAILURE);
                }
                return set_configure(argv[2], argv[3]);
        }
        else if (strcmp(argv[1], "create") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF <project_name>\n");
                        exit(EXIT_FAILURE);
                }
                return create_or_destroy(argv[2], TRUE);
        }
        else if (strcmp(argv[1], "destroy") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF <project_name>\n");
                        exit(EXIT_FAILURE);
                }
                return create_or_destroy(argv[2], FALSE);
        }
        else if (strcmp(argv[1], "add") == 0)
        {
                if (argc != 4)
                {
                        fprintf(stderr, "Usage: ./WTF <project_name> <filename>\n");
                        exit(EXIT_FAILURE);
                }
                return _add(argv[2], argv[3]);
        }
        else if (strcmp(argv[1], "remove") == 0)
        {
                if (argc != 4)
                {
                        fprintf(stderr, "Usage: ./WTF <project_name> <filename>\n");
                        exit(EXIT_FAILURE);
                }
                return _remove(argv[2], argv[3]);
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
