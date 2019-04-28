#ifndef COMPRESSION_H
#define COMPRESSION_H

typedef struct _node {
        char * data;
        struct _node * next;
} node;
/*
 *      Compresses inputted buffer buf using zlib.
 *      Returns dynamically allocated pointer to compressed buffer
 *      and stores compressed size in inputted integer pointer.
 */
char * _compress(char * buf, int * compressed_size);
/*
 *      Decompresses inputted buffer using zlib.
 *      Returns dynamically allocated pointer to decompressed buffer of
 *      size orig_size.
 */
char * _decompress(char * buf, int orig_size, int compressed_size);
/*
 *      Recursively reads through a file or directory and returns a buffer
 *      containing all the files.
 */
char * recursive_zip(char * filename, int is_server);
/*
 *      Reads through zipped buffer and creates a file/directory for each item.
 */
void recursive_unzip(char * zip_buf);

#endif
