#include "compression.h"
#include "fileIO.h"
#include "flags.h"
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>           // check IS_DIR flags
#include <dirent.h>             // Directory
#include <fcntl.h>              // open flags
#include <zlib.h>               // zlib compression
/*
 *      Compresses inputted buffer buf using zlib.
 *      Returns dynamically allocated pointer to compressed buffer
 *      and stores compressed size in inputted integer pointer.
 */
char * _compress(char * buf, int * compressed_size)
{
        // int fd = open(filename, O_RDONLY, 00600);
        // int size = lseek(fd, 0, SEEK_END);
        // lseek(fd, 0, SEEK_SET);
        // char buf[size+1];
        // bzero(buf, size+1);
        // if (better_read(fd, buf, size, __FILE__, __LINE__) != 1)
        //         return NULL;
        // close(fd);
        int size = strlen(buf);
        char * buf_compress= (char *) malloc(sizeof(char)*(size+1));
        if (buf_compress == NULL)
                return NULL;
        bzero(buf_compress, size+1);
        // zlib struct
        z_stream defstream;
        defstream.zalloc = Z_NULL;
        defstream.zfree = Z_NULL;
        defstream.opaque = Z_NULL;
        defstream.avail_in = (uInt)size+1; // size of input, string + terminator
        defstream.next_in = (Bytef *)buf; // input char array
        defstream.avail_out = (uInt)size+1; // size of output
        defstream.next_out = (Bytef *)buf_compress; // output char array
        // the actual compression work.
        deflateInit(&defstream, Z_BEST_COMPRESSION);
        deflate(&defstream, Z_FINISH);
        deflateEnd(&defstream);
        *compressed_size = defstream.total_out;

        // printf("Compressed size is: %lu vs. original %d\n", defstream.total_out, size);
        // printf("Compressed string is: %s\n", buf_compress);
        /*
        char * compressed = (char *) malloc((strlen(filename)+1size+1)*sizeof(char));
        bzero(compressed, strlen(compressed)+1);
        sprintf(compressed, "")
        return compressed;
        */
        return buf_compress;
}
/*
 *      Decompresses inputted buffer using zlib.
 *      Returns dynamically allocated pointer to decompressed buffer of
 *      size orig_size.
 */
char * _decompress(char * buf, int orig_size, int compressed_size)
{
        char * decompressed = (char *) malloc(sizeof(char) * orig_size+1);
        bzero(decompressed, orig_size+1);
        // zlib struct
        z_stream infstream;
        infstream.zalloc = Z_NULL;
        infstream.zfree = Z_NULL;
        infstream.opaque = Z_NULL;
        // setup "b" as the input and "c" as the compressed output
        infstream.avail_in = (uInt)compressed_size+1; // size of input
        infstream.next_in = (Bytef *)buf; // input char array
        infstream.avail_out = (uInt)orig_size+1; // size of output
        infstream.next_out = (Bytef *)decompressed; // output char array

        // the actual DE-compression work.
        inflateInit(&infstream);
        inflate(&infstream, Z_NO_FLUSH);
        inflateEnd(&infstream);

        // printf("Uncompressed size is: %lu vs. %d\n", strlen(decompressed), orig_size);
        // printf("Uncompressed string is: %s\n", decompressed);
        return decompressed;

}
/*
 *      Recursively reads through a file or directory and returns a buffer
 *      containing all the files.
 */
char * recursive_zip(char * _filename, int is_server)
{
        char filename[strlen(_filename)];
        bzero(filename, strlen(_filename));
        if (is_server)
                strcpy(filename, &_filename[strlen(".server_repo/")]);
        else
                strcpy(filename, &_filename[0]);
        printf("%s %s\n", filename, _filename);
        if (dir_exists(_filename))
        // Open directory and concat all files
        {
                node * front = NULL;
                DIR * dir = opendir(_filename);
                struct dirent * item = readdir(dir);
                int no_items = FALSE;

                int size = strlen(filename) + 4;        // filename D\n
                while (item != NULL)
                {
                        while (!strcmp(item->d_name, ".") || !strcmp(item->d_name, ".."))
                        {
                                item = readdir(dir);
                                if (item == NULL)
                                {
                                        no_items = TRUE;
                                        break;
                                }
                        }
                        if (no_items)
                                break;
                        char child_file[strlen(_filename)+strlen(item->d_name)+2];
                        bzero(child_file, strlen(_filename)+strlen(item->d_name)+2);
                        sprintf(child_file, "%s/%s", _filename, item->d_name);
                        node * new = (node *) malloc(sizeof(node));
                        new->next = front;
                        new->data = recursive_zip(child_file, is_server);
                        size += strlen(new->data);
                        front = new;
                        item = readdir(dir);
                }
                closedir(dir);
                char * buf = (char *) malloc(sizeof(char) * size);
                bzero(buf, size);
                sprintf(buf, "%s D\n", filename);
                int i = strlen(buf);
                while (front != NULL)
                {
                        strncpy(&buf[i], front->data, strlen(front->data));
                        // sprintf(&buf[i], "%s\n", front->data);
                        i = strlen(buf);
                        node * old = front;
                        front = front->next;
                        free(old->data);
                        free(old);
                }
                return buf;
        }
        else
        // Return compressed file
        {
                int fd = open(_filename, O_RDONLY, 00600);
                if (fd == -1)
                {
                        fprintf(stderr, "[decompress] Error opening file %s. FILE: %s. LINE: %d.\n", _filename, __FILE__, __LINE__);
                        return NULL;
                }
                int size = lseek(fd, 0, SEEK_END);
                lseek(fd, 0, SEEK_SET);
                char size_str[10] = {0,0,0,0,0,0,0,0,0,0};
                sprintf(size_str, "%d", size);
                char * buf = (char *) malloc(sizeof(char)*(size+strlen(filename)+3+strlen(size_str)+3));        // filename D file_size\n<contents>\n
                bzero(buf, size+strlen(filename)+3+strlen(size_str)+3);
                sprintf(buf, "%s\nF\n%d\n", filename, size);
                if (better_read(fd, &buf[strlen(filename)+3+strlen(size_str)+1], size, __FILE__, __LINE__) != 1)
                        return NULL;
                buf[size+strlen(filename)+3+strlen(size_str)+2] = '\n';
                close(fd);
                return buf;
        }
        return NULL;
}
/*
 *      Reads through zipped buffer and creates a file/directory for each item.
 */
void recursive_unzip(char * zip_buf)
{
        int size = strlen(zip_buf);
        int i = 0;
        while (i < size)
        {
                // Read file name
                char nameBuf[1024];
                bzero(nameBuf, 1024);
                sscanf(&zip_buf[i], "%s\n", nameBuf);
                printf("%s\n", nameBuf);
                i += strlen(nameBuf)+1;
                // Read type of file: F file OR D directory
                char type[2] = {0,0};
                sscanf(&zip_buf[i], "%s\n", type);
                printf("%s\n", type);
                i += strlen(type)+1;
                if (*type == 'D')
                // Directory
                {
                        printf("making dir %s", nameBuf);
                        make_dir(nameBuf, __FILE__, __LINE__);
                        printf("%c\n", zip_buf[i]);
                }
                else
                // File
                {
                        // Read file size
                        char size_str[10] = {0,0,0,0,0,0,0,0,0,0};
                        sscanf(&zip_buf[i], "%s\n", size_str);
                        printf("size_str: %s\n",size_str);
                        int file_size;
                        sscanf(size_str, "%d", &file_size);
                        i += strlen(size_str)+1;
                        // Read file
                        char fileBuf[file_size+1];
                        bzero(fileBuf, file_size+1);
                        strncpy(fileBuf, &zip_buf[i], file_size);
                        i += file_size;
                        printf("contents: %s\n", fileBuf);
                        int fd = open(nameBuf, O_WRONLY | O_CREAT, 00600);
                        if (better_write(fd, fileBuf, file_size, __FILE__, __LINE__) != 1)
                                return;
                        close(fd);
                }
        }
        return;
}
