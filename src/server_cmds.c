#include "server_cmds.h"
#include "fileIO.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>             // FILE IO
#include <fcntl.h>              // open flags


/*
 *
 */
int checkout(int sd, char * proj_name)
{
        
        return 0;
}

/*
 *      Creates directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int create(int sd, char * proj_name)
{
        // Create projects folder if one does not already exist
        if (make_dir("projects", __FILE__, __LINE__) < 0)
                return 1;
        char new_proj_name[strlen(proj_name)+9];
        sprintf(new_proj_name, "projects/%s", proj_name);

        // create new project in projects folder
        if (make_dir(new_proj_name, __FILE__, __LINE__) != 0)
                return 1;
        printf("%s\n", new_proj_name);

        // Init manifest
        char manifest[strlen(new_proj_name)+11];
        bzero(manifest, strlen(new_proj_name)+11);
        sprintf(manifest, "%s/.manifest", new_proj_name);
        int fd = open(manifest, O_WRONLY | O_CREAT | O_TRUNC, 00600);
        if (better_write(fd, "1\n", 2, __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[create] Error returned by better_write. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                close(fd);
                return 1;
        }
        if (send_file(sd, manifest))
                return 1;
        close(fd);
        return 0;
}

/*
 *      Deletes directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int destroy(char * proj_name)
{
        if (!dir_exists("projects"))
        {
                fprintf(stderr, "[create] Project does not exist.\n");
                return 1;
        }
        char new_proj_name[strlen(proj_name)+9];
        sprintf(new_proj_name, "projects/%s", proj_name);
        if (!dir_exists(new_proj_name))
        {
                fprintf(stderr, "[create] Project does not exist.\n");
                return 1;
        }
        printf("removing directory: %s\n", new_proj_name);
        remove_dir(new_proj_name);
        return 0;
}
