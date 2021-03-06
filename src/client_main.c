#include "client_main.h"
#include "client_cmds.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fileIO.h"             // Socket + file IO
#include <unistd.h>             // FILE IO
#include "flags.h"
#include "compression.h"
#include "manifest_utils.h"

void usage()
{
        fprintf(stderr, "Usage: ./WTF <PORT NUMBER>\n");
        exit(EXIT_FAILURE);
}

int main(int argc, char * argv[])
{
        int ret;
        if (argc < 3)
        {
                usage();
        }
        if (strcmp(argv[1], "configure") == 0)
        {
                if (argc != 4)
                {
                        fprintf(stderr, "Usage: ./WTF configure <IP ADDRESS> <PORT NUMBER>\n");
                        exit(EXIT_FAILURE);
                }
                ret = set_configure(argv[2], argv[3]);
        }
        else if (strcmp(argv[1], "create") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF create <project_name>\n");
                        exit(EXIT_FAILURE);
                }
                ret = create_or_destroy(argv[2], TRUE);
        }
        else if (strcmp(argv[1], "destroy") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF destroy <project_name>\n");
                        exit(EXIT_FAILURE);
                }
                ret = create_or_destroy(argv[2], FALSE);
        }
        else if (strcmp(argv[1], "add") == 0)
        {
                if (argc != 4)
                {
                        fprintf(stderr, "Usage: ./WTF add <project_name> <filename>\n");
                        exit(EXIT_FAILURE);
                }
                ret = _add(argv[2], argv[3]);
        }
        else if (strcmp(argv[1], "remove") == 0)
        {
                if (argc != 4)
                {
                        fprintf(stderr, "Usage: ./WTF remove <project_name> <filename>\n");
                        exit(EXIT_FAILURE);
                }
                ret =  _remove(argv[2], argv[3]);
        }
        else if (strcmp(argv[1], "update") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF update <project_name>\n");
                        exit(EXIT_FAILURE);
                }
                ret =  _update(argv[2]);
        }
        else if (strcmp(argv[1], "upgrade") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF upgrade <project_name\n");
                        exit(EXIT_FAILURE);
                }

                ret = _upgrade(argv[2]);
        }
        else if (strcmp(argv[1], "checkout") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF checkout <project_name>\n");
                        exit(EXIT_FAILURE);
                }
                ret =  _checkout(argv[2]);
        }
        else if (strcmp(argv[1], "currentversion") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF currentversion <project_name>\n");
                        exit(EXIT_FAILURE);
                }
                ret =  current_version(argv[2]);
        }
        else if (strcmp(argv[1], "commit") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF commit <project_name>\n");
                        exit(EXIT_FAILURE);
                }
                ret = _commit(argv[2]);
        }
        else if (strcmp(argv[1], "push") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF push <project_name>\n");
                        exit(EXIT_FAILURE);
                }
                ret = _push(argv[2]);
        }
        else if (strcmp(argv[1], "history") == 0)
        {
                if (argc != 3)
                {
                        fprintf(stderr, "Usage: ./WTF history <project_name>\n");
                        exit(EXIT_FAILURE);
                }
                ret = _history(argv[2]);
        }
        else if (strcmp(argv[1], "rollback") == 0)
        {
                if (argc != 4)
                {
                        fprintf(stderr, "Usage: ./WTF rollback <project_name> <version>\n");
                        exit(EXIT_FAILURE);
                }
                ret = _rollback(argv[2], argv[3]);
        }
        else
        {
                fprintf(stderr, "Error: please use a valid command.\n");
                return 1;
        }
        if (ret == 0)
                printf("Command executed successfully.\n");
        else
                fprintf(stderr, "Error executing command.\n");
        return ret;
}
