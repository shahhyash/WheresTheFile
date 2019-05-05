#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include "fileIO.h"

/*
 *      Runs argv_list command in a seperate fork and execs on it.
 */
int fork_and_exec(char * argv_list[], int dynamic)
{
        pid_t  pid;
        // int ret = 1;
        int status;
        pid = fork();
        if (pid == -1)
        {

                // pid == -1 means error occured
                printf("can't fork, error occured\n");
                return 1;
        }
        else if (pid == 0)
        {

                // the execv() only return if error occured.
                // The return value is -1
                execv(argv_list[0],argv_list);
                exit(0);
        }
        else
        {
                // a positive number is returned for the pid of
                // parent process
                // getppid() returns process id of parent of
                // calling process
                printf("parent process, pid = %u\n",getppid());

                // the parent process calls waitpid() on the child
                // waitpid() system call suspends execution of
                // calling process until a child specified by pid
                // argument has changed state
                // see wait() man page for all the flags or options
                // used here
                if (waitpid(pid, &status, 0) > 0)
                {

                        if (WIFEXITED(status) && !WEXITSTATUS(status))
                                printf("program execution successful\n");

                        else if (WIFEXITED(status) && WEXITSTATUS(status))
                        {
                                if (WEXITSTATUS(status) == 127)
                                {

                                        // execv failed
                                        printf("execv failed\n");
                                        return 1;
                                }
                                else
                                {
                                        printf("program terminated normally,"
                                        " but returned a non-zero status\n");
                                        return 1;
                                }
                        }
                        else
                        {
                                printf("program didn't terminate normally\n");
                                return 1;
                        }
                }
                else
                {
                        // waitpid() failed
                        printf("waitpid() failed\n");
                        return 1;
                }
                if (dynamic)
                {
                        int argc = 0;
                        while (argv_list[argc] != NULL)
                        {
                                free(argv_list[argc++]);
                        }
                }
        }
        return 0;
}
/*
 *      Finds an open port and starts the server there, stores in PORT
 *      Returns server_PID on success, -1 otherwise.
 */
pid_t init_server(int * port)
{
        *port = 9000;
        char port_str[10];
        bzero(port_str, 10);
        sprintf(port_str, "%d", *port);
        char * argv_list[] = {"bin/WTFserver", port_str, NULL};
        pid_t  pid;
        // int ret = 1;
        pid = fork();
        if (pid == -1)
        {

                // pid == -1 means error occured
                printf("can't fork, error occured\n");
                exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {

                // the execv() only return if error occured.
                // The return value is -1
                execv(argv_list[0],argv_list);
                exit(0);
        }
        else
        {

                return pid;
        }

}
/*
 *      Run configure plan for client with IP ADDRESS localhost and PORT input_port.
 *      Returns 0 on success, 1 otherwise.
 */
int init_client(int port)
{
        char port_str[10];
        bzero(port_str, 10);
        sprintf(port_str, "%d", port);
        char * argv_list[] = {"bin/WTF", "configure", "127.0.0.1", port_str, NULL};
        return fork_and_exec(argv_list, 0);
}
int main()
{
        char * file_ex = "words words words words words";
        int PORT;
        pid_t server_pid = init_server(&PORT);
        if (server_pid == -1)
                return 1;
        if (init_client(PORT) != 0)
        {
                fprintf(stderr, "Error initializing client.\n");
                if (kill(server_pid, SIGINT) != 0)
                        fprintf(stderr, "Kill failed.\n");
                return 1;
        }
        FILE * cmds = fopen(".test_commands", "r");
        if (cmds == NULL)
                return 1;
        // First line is number of test cases
        int num_testcases;
        fscanf(cmds, "%d\n", &num_testcases);
        int i;
        for (i = 0; i < num_testcases; i++)
        {
                int num_cmds;
                // Read number of commands
                fscanf(cmds, "%d\n", &num_cmds);
                int j;
                for (j = 0; j < num_cmds; j++)
                {
                        // Read command
                        char cmd[100];
                        bzero(cmd, 100);
                        int cur = 0;
                        do {
                                cmd[cur++] = fgetc(cmds);
                        } while (cmd[cur-1] != '\n');
                        cmd[cur-1] = '\0';
                        printf("%s\n", cmd);

                        // Process cmd into list of cmds
                        // count spaces
                        int num_spaces = 0;
                        int k;
                        for (k = 0; k < strlen(cmd); k++)
                        {
                                if (cmd[k] == ' ')
                                        num_spaces++;
                        }
                        char * argv_list[num_spaces+2];
                        int start = 0;
                        int cur_arg = 0;
                        for (k = 0; k < strlen(cmd); k++)
                        {
                                if (cmd[k] == ' ')
                                {
                                        char * arg = (char *) malloc(sizeof(char)*(k-start+1));
                                        strncpy(arg, &cmd[start], k-start);
                                        arg[k-start] = '\0';
                                        argv_list[cur_arg++] = arg;
                                        start = k + 1;
                                }
                        }
                        char * arg = (char *) malloc(sizeof(char)*(strlen(cmd)-start+1));
                        strncpy(arg, &cmd[start], strlen(cmd)-start);
                        arg[strlen(cmd)-start] = '\0';
                        argv_list[cur_arg++] = arg;
                        start = k + 1;
                        argv_list[num_spaces+1] = NULL;

                        if (strncmp(cmd, "rm -rf", 6)==0)
                        {
                                remove_dir(argv_list[2]);
                                int argc = 0;
                                while (argv_list[argc] != NULL)
                                {
                                        free(argv_list[argc++]);
                                }
                        }
                        else if (strncmp(cmd, "make", 4) == 0)
                        {
                                FILE * new = fopen(argv_list[1], "w+");
                                fwrite(file_ex, 1, strlen(file_ex), new);
                                fclose(new);
                                int argc = 0;
                                while (argv_list[argc] != NULL)
                                {
                                        free(argv_list[argc++]);
                                }
                        }
                        else
                        {
                                if (fork_and_exec(argv_list, 1) != 0)
                                {
                                        fclose(cmds);
                                        if (kill(server_pid, SIGINT) != 0)
                                                fprintf(stderr, "Kill failed.\n");
                                        exit(EXIT_FAILURE);
                                }

                        }
                }
        }
        fclose(cmds);
        if (kill(server_pid, SIGINT) != 0)
                fprintf(stderr, "Kill failed.\n");

        return 0;
}
