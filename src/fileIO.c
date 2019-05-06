#include "fileIO.h"
#include "flags.h"

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>           // check IS_DIR flags
#include <dirent.h>             // Directory
#include <fcntl.h>              // open flags
#include "compression.h"
#include "commit_utils.h"
/*
 *    Sends nbyte bytes from the filedescriptor fd and stores them in buf.
 *    Returns 1 if successful. Otherwise returns a value according to the error.
 */
int better_send(int fd, char * buf, size_t nbyte, int flags, char * file, int line)
{
        ssize_t ret = 1;
        int bytes_read = 0;
        while (nbyte-bytes_read > 0 && ret != 0)
        {
                ret = send(fd, &buf[bytes_read], nbyte-bytes_read, flags);
                if (ret > 0)
                {
                        bytes_read += ret;
                }
                else if (ret < 0)
                {
                        fprintf(stderr, "[better_send] Error. FILE: %s. Line: %d.\n", file, line);
                        return ret;
                }
                else if (ret == 0 && nbyte-bytes_read != 0)
                {
                        fprintf(stderr, "[better_send] End of file before all bytes sent. FILE: %s. Line: %d.\n", file, line);
                        return ret;
                }
        }
        return 1;
}

/*
 *    Reads nbyte bytes from the filedescriptor filedes and stores them in buf.
 *    Returns 1 if successful. Otherwise returns a value according to the error.
 */
int better_read(int filedes, char * buf, size_t nbyte, char * file, int line)
{
        ssize_t ret = 1;
        int bytes_read = 0;
        while (nbyte-bytes_read > 0 && ret != 0)
        {
                ret = read(filedes, &buf[bytes_read], nbyte-bytes_read);
                if (ret > 0)
                {
                        bytes_read += ret;
                }
                else if (ret < 0)
                {
                        fprintf(stderr, "[better_read] Error. FILE: %s. Line: %d.\n", file, line);
                        return ret;
                }
                else if (ret == 0 && nbyte-bytes_read != 0)
                {
                        fprintf(stderr, "[better_read] End of file before all bytes read. FILE: %s. Line: %d.\n", file, line);
                        return ret;
                }
        }
        return 1;
}

/*
 *    Writes nbyte bytes from the buf and stores them in file with filedescriptor filedes.
 *    Returns 1 if successful. Otherwise returns a value according to the error.
 */
int better_write(int filedes, char * buf, size_t nbyte, char * file, int line)
{
        ssize_t ret = 1;
        int bytes_written = 0;
        while (nbyte-bytes_written > 0 && ret != 0)
        {
                ret = write(filedes, &buf[bytes_written], nbyte-bytes_written);
                if (ret > 0)
                {
                        bytes_written += ret;
                }
                else if (ret < 0)
                {
                        fprintf(stderr, "[better_write] Error. FILE: %s. Line: %d.\n", file, line);
                        return ret;
                }
                else if (ret == 0 && nbyte-bytes_written != 0)
                {
                        fprintf(stderr, "[better_write] End of file before all bytes written. FILE: %s. Line: %d.\n", file, line);
                        return ret;
                }
        }
        return 1;
}

/*
 *      Returns TRUE if directory exists, FALSE otherwise.
 */
int dir_exists(const char * dir)
{
        struct stat stats;
        stat(dir, &stats);
        // Check for file existence
        if (S_ISDIR(stats.st_mode))
                return TRUE;
        return FALSE;
}
/*
 *      Returns TRUE if file exists, FALSE otherwise.
 */
int file_exists(char * file)
{
        if (access(file, F_OK) == -1)
                return FALSE;
        return TRUE;
}
/*
 *      Make directory called dir_name if it does not exist.
 *      Returns 0 on success; 1 if dir exists, and -1 on other error in mkdir
 */
int make_dir(char * dir_name, char * file, int line)
{
        if (!dir_exists(dir_name))
        {
                if (mkdir(dir_name, 00777) == -1)
                {
                        fprintf(stderr, "[make_dir] Error creating new project directory. FILE: %s. LINE: %d.\n", file, line);
                        return -1;
                }
        }
        else
        {
                fprintf(stderr, "[make_dir] Project already exists. FILE: %s. LINE: %d.\n", file, line);
                return 1;
        }
        return 0;
}
/*
 *      Recursively deletes directory dir and the contents inside
 */
void remove_dir(char * dir)
{
        DIR * dirdes = opendir(dir);
        struct dirent * item = readdir(dirdes);
        int no_items = FALSE;
        // loop through directory until no further items are left
        while (item != NULL)
        {
                while (!strcmp(item->d_name, ".") || !strcmp(item->d_name, ".."))
		{
			item = readdir(dirdes);
			if (!item)
			{
				no_items = TRUE;
				break;
			}
		}
		if (no_items)
			break;
                char new_name[strlen(dir) + strlen(item->d_name)+2];
                bzero(new_name, strlen(dir) + strlen(item->d_name)+2);
                sprintf(new_name, "%s/%s", dir, item->d_name);
                // if item is a directory, recursively search all of it's children
                if (item->d_type == DT_DIR)
                {

                        remove_dir(new_name);
                }
                // if item is a regular file delete it
                else if (item->d_type == DT_REG)
                {
                        remove(new_name);
                }
                item = readdir(dirdes);
        }
        remove(dir);
        closedir(dirdes);
}
/*
 *      Compresses name buffer and sends it to client.
 *      Returns 0 on success, 1 otherwise.
 */
int compress_and_send(int sd, char * name, int is_server)
{
        char * zipped = recursive_zip(name, is_server);
        int zipped_size = strlen(zipped);
        int num_digits = 0;
        int i = zipped_size;
        while (i != 0)
        {
                num_digits++;
                i /= 10;
        }
        // Send three digit length of file size
        char file_size_str[4] = {'0','0','0',0};
        if (num_digits < 10)
                sprintf(&file_size_str[2], "%d", num_digits);
        else if (num_digits < 100)
                sprintf(&file_size_str[1], "%d", num_digits);
        else
                sprintf(&file_size_str[0], "%d", num_digits);
        printf("sending %s\n", file_size_str);
        if (better_send(sd, file_size_str, 3, 0, __FILE__, __LINE__) != 1)
        {
                free(zipped);
                return 1;
        }
        char zip_size_str[10] = {0,0,0,0,0,0,0,0,0,0};
        sprintf(zip_size_str, "%d", zipped_size);
        // Send file size
        printf("sending size %s\n", zip_size_str);
        fflush(stdout);
        if (better_send(sd, zip_size_str, strlen(zip_size_str), 0, __FILE__, __LINE__) != 1)
        {
                free(zipped);
                return 1;
        }
        int c_s;
        char * compressed = _compress(zipped, &c_s);
        free(zipped);
        char c_s_str[10] = {0,0,0,0,0,0,0,0,0,0};
        sprintf(c_s_str, "%d", c_s);
        // Send compressed file
        if (send_file(sd, compressed, c_s_str))
        {
                fprintf(stderr, "[checkout] send_file returned error. FILE: %s. LINE: %d.\n", __FILE__, __LINE__);
                free(compressed);
                return 1;
        }
        free(compressed);
        return 0;
}
/*
 *      Sends data stored in file filename through socket descriptor sd.
 *      Returns 0 on success; 1 otherwise.
 *      Protocol: (i) Sends 3 digits that is the number of digits of the file size.
 *                (ii) Send file size.
 *                (iii) Sends file bytes.
 */
int send_file(int sd, char * buf, char * filesize)
{
        int num_digits = strlen(filesize);
        // Send three digit length of file size
        char file_size_str[4] = {'0','0','0',0};
        if (num_digits < 10)
                sprintf(&file_size_str[2], "%d", num_digits);
        else if (num_digits < 100)
                sprintf(&file_size_str[1], "%d", num_digits);
        else
                sprintf(&file_size_str[0], "%d", num_digits);
        printf("sending %s\n", file_size_str);
        if (better_send(sd, file_size_str, 3, 0, __FILE__, __LINE__) != 1)
                return 1;
        // Send file size
        printf("sending size %s\n", filesize);
        fflush(stdout);
        if (better_send(sd, filesize, strlen(filesize), 0, __FILE__, __LINE__) != 1)
                return 1;
        // Send file bytes
        int f_size_num;
        sscanf(filesize, "%d", &f_size_num);
        // printf("sending file %s\n", buf);
        if (better_send(sd, buf, f_size_num, 0, __FILE__, __LINE__) != 1)
                return 1;
        return 0;

}
/*
 *      Using the socket descriptor sd, the function reads the decompressed
 *      file received and returns a pointer to it as a string.
 */
char * receive_file(int sd)
{
        // Read decompressed file size length
        char d_length_size[4] = {0,0,0,0};
        if (better_read(sd , d_length_size , 3, __FILE__, __LINE__) <= 0)
                return NULL;
        int d_file_size_length;
        sscanf(d_length_size, "%d", &d_file_size_length);
        // Read decompressed file size
        char d_file_size_str[1+d_file_size_length];
        bzero(d_file_size_str, 1+d_file_size_length);
        if (better_read(sd , d_file_size_str , d_file_size_length, __FILE__, __LINE__) <= 0)
                return NULL;
        int d_file_size;
        sscanf(d_file_size_str, "%d", &d_file_size);
        // Read size of file length
        char length_size[4] = {0,0,0,0};
        if (better_read(sd , length_size , 3, __FILE__, __LINE__) <= 0)
                return NULL;
        int file_size_length;
        sscanf(length_size, "%d", &file_size_length);
        // Read file size
        char file_size_str[1+file_size_length];
        bzero(file_size_str, 1+file_size_length);
        if (better_read(sd , file_size_str , file_size_length, __FILE__, __LINE__) <= 0)
                return NULL;
        // Read file bytes
        int file_size;
        sscanf(file_size_str, "%d", &file_size);
        char file[file_size+1];
        bzero(file, file_size+1);
        if (better_read(sd , file , file_size, __FILE__, __LINE__) <= 0)
                return NULL;
        // decompress file
        printf("file: %s\n", file);
        char * decompressed = _decompress(file, d_file_size, file_size);
        return decompressed;
}

/*
 *      Generates zipped buffer for an individual file (used for commits because it adds op code for function)
 */
char * build_file_buffer(char * file_buffer, char * file_path, int size, char op_code)
{
        char size_str[10] = {0,0,0,0,0,0,0,0,0,0};
        sprintf(size_str, "%d", size);

        /* Buffer Format:
                <FILEPATH>\n
                <F (for File)>\n
                <OPCODE: M, A, D>\n
                <SIZE>\n
                <FILE>\n
                \0
        */

        /* compute buffer size and file info start */
        int buffer_size = sizeof(char) * ( strlen(file_path) + 1 + 1 + strlen(size_str) + size + 6);
        int file_start = sizeof(char) * ( strlen(file_path) + 1 + 1 + strlen(size_str) + 4);

        /* allocate space */
        char * buf = (char *) malloc(buffer_size);
        bzero(buf, buffer_size);

        /* write file metadata at the beginning of the buffer */
        sprintf(buf, "%s\nF\n%c\n%d\n", file_path, op_code, size);

        /* write file contents right after it */
        strcpy(&buf[file_start], file_buffer);
        buf[buffer_size-2] = '\n';
        buf[buffer_size-1] = '\0';

        printf("buffer:\n%s\n", buf);

        return buf;
}

/*
 *      Compress all commit entries together and send them out to the server to complete push
 */
int push_changes_to_server(int sd, commit_entry * commits, char * commit_file)
{
        /* keep track of total size */
        int zipped_size = 2;

        /* start by adding the commit file to the front of the list */
        node * front = (node *) malloc(sizeof(node));
        front->data = build_file_buffer(commit_file, ".commit", strlen(commit_file), 'M');
        front->next = NULL;

        /* increment total zip size */
        zipped_size += strlen(front->data);

        node * ptr = front;

        commit_entry * commit_ptr = commits;
        while (commit_ptr)
        {
                int fd = open(commit_ptr->file_path, O_RDONLY, 00600);
                if (fd == -1)
                {
                        fprintf(stderr, "[push_changes] Error opening file %s.\n", commit_ptr->file_path);
                        return 1;
                }

                /* fetch file contents and size */
                int size = lseek(fd, 0, SEEK_END);
                lseek(fd, 0, SEEK_SET);
                char buf[size+1];
                if (better_read(fd, buf, size, __FILE__, __LINE__) != 1)
                        return 1;
                close(fd);

                /* allocate space for new node */
                ptr->next = (node *) malloc(sizeof(node));
                ptr = ptr->next;

                /* build zip contents with file metadata */
                ptr->data = build_file_buffer(buf, commit_ptr->file_path, size, commit_ptr->op_code);
                ptr->next = NULL;

                /* increment total zip size */
                zipped_size += strlen(ptr->data);

                /* travel to next commit in linked list */
                commit_ptr = commit_ptr->next;
        }

        /* build zip buffer with size zipped_size */
        char * zipped_buffer = (char *) malloc(sizeof(char) * zipped_size);
        bzero(zipped_buffer, zipped_size);

        /* iterate through linked list of zipped files and write them to one buffer */
        int i = 0;
        while (front != NULL)
        {
                strncpy(&zipped_buffer[i], front->data, strlen(front->data));
                i = strlen(zipped_buffer);
                node * old = front;
                front = front->next;
                free(old->data);
                free(old);
        }
        zipped_buffer[zipped_size-1] = '\0';

        printf("zipped: %s\n", zipped_buffer);

        int num_digits = 0;
        i = zipped_size;
        while (i != 0)
        {
                num_digits++;
                i /= 10;
        }

        // Send three digit length of file size
        char file_size_str[4] = {'0','0','0',0};
        if (num_digits < 10)
                sprintf(&file_size_str[2], "%d", num_digits);
        else if (num_digits < 100)
                sprintf(&file_size_str[1], "%d", num_digits);
        else
                sprintf(&file_size_str[0], "%d", num_digits);
        printf("sending %s\n", file_size_str);
        if (better_send(sd, file_size_str, 3, 0, __FILE__, __LINE__) != 1)
        {
                free(zipped_buffer);
                return 1;
        }

        /* now that we have a singular zipped buffer, we should compress and send it to server */

        char zip_size_str[10] = {0,0,0,0,0,0,0,0,0,0};
        sprintf(zip_size_str, "%d", zipped_size);
        // Send file size
        printf("sending size %s\n", zip_size_str);
        fflush(stdout);
        if (better_send(sd, zip_size_str, strlen(zip_size_str), 0, __FILE__, __LINE__) != 1)
        {
                free(zipped_buffer);
                return 1;
        }
        int c_s;
        char * compressed = _compress(zipped_buffer, &c_s);
        free(zipped_buffer);
        char c_s_str[10] = {0,0,0,0,0,0,0,0,0,0};
        sprintf(c_s_str, "%d", c_s);
        // Send compressed file
        if (send_file(sd, compressed, c_s_str))
        {
                fprintf(stderr, "[checkout] send_file returned error. FILE: %s. LINE: %d.\n", __FILE__, __LINE__);
                free(compressed);
                return 1;
        }
        free(compressed);
        return 0;
}
