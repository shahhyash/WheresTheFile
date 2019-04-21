#include "fileIO.h"
#include "flags.h"

#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>           // check IS_DIR flags
#include <dirent.h>             // Directory
#include <fcntl.h>              // open flags
#include <zlib.h>
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
}
/*
 *      Sends data stored in file filename through socket descriptor sd.
 *      Returns 0 on success; 1 otherwise.
 *      Protocol: (i) Sends 3 digits that is the number of digits of the file size.
 *                (ii) Send file size.
 *                (iii) Sends file bytes.
 */
int send_file(int sd, char * filename)
{
        int fd = open(filename, O_RDONLY, 00600);
        int fd_size = lseek(fd, 0, SEEK_END);
        char buf[fd_size+1];
        bzero(buf, fd_size+1);
        lseek(fd, 0, SEEK_SET);
        if (better_read(fd, buf, fd_size, __FILE__, __LINE__) != 1)
                return 1;
        int num_digits = 0, digits = fd_size;
        while (digits != 0)
        {
                num_digits++;
                digits /= 10;
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
                return 1;
        // Send file size
        char digits_str[num_digits+1];
        bzero(digits_str, num_digits+1);
        sprintf(digits_str, "%d", fd_size);
        if (better_send(sd, digits_str, num_digits, 0, __FILE__, __LINE__) != 1)
                return 1;
        // Send file bytes
        if (better_send(sd, buf, fd_size, 0, __FILE__, __LINE__) != 1)
                return 1;
        return 0;

}
/*
 *
 */
char * _compress(char * filename)
{
        return NULL;
}
