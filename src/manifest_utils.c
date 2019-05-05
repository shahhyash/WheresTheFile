#include "manifest_utils.h"
#include "fileIO.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <openssl/sha.h>        // Hash function

manifest_entry * read_manifest_file(char * file_contents)
{        
    /* create node for head of list */
    manifest_entry * root = (manifest_entry*)malloc(sizeof(manifest_entry));
    root->version=atoi(file_contents);
    root->file_path=NULL;
    root->hash_code=NULL;
    root->next=NULL;

    manifest_entry * ptr = root;

    char * line_saveptr;

    char * manifest_line = strtok_r(file_contents, "\n", &line_saveptr);
    while (manifest_line)
    {
        char *item_saveptr, *token, *file_path, *hash_code;
        int mode = 1, version;
        token = strtok_r(manifest_line, " ", &item_saveptr);
        while(token)
        {
            if (mode == 1)
                version = atoi(token);
            
            if (mode == 2)
            {
                file_path = malloc(strlen(token) + 1);
                strcpy(file_path, token);
            }

            if (mode == 3)
            {
                hash_code = malloc(strlen(token) + 1);
                strcpy(hash_code, token);
            }

            ++mode;
            token = strtok_r(NULL, " ", &item_saveptr);
        }

        if (mode == 4)
        {
            /* create new node for current manifest entry */
            ptr->next = (manifest_entry*)malloc(sizeof(manifest_entry));
            ptr = ptr->next;
            ptr->version = version;
            ptr->file_path = file_path;
            ptr->hash_code = hash_code;
            ptr->next = NULL;
        }

        manifest_line = strtok_r(NULL, "\n", &line_saveptr);
    }

    return root;
}

void print_manifest(manifest_entry * root)
{
    while (root)
    {
        printf("version=%d, filepath=%s, hashcode=%s\n", root->version, root->file_path, root->hash_code);
        root = root->next;
    }
}

void free_manifest(manifest_entry * root)
{
    while (root)
    {
        manifest_entry * next = root->next;
        free(root->file_path);
        free(root->hash_code);
        free(root);
        root = next;
    }
}

void update_hashes(manifest_entry * root)
{
    while (root)
    {
        if (root->file_path)
        {
            /* fetch old hash code */
            char * old_hash = root->hash_code;

            /* read file contents */
            int fd = open(root->file_path, O_RDONLY, 00600);
            int file_length = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            char file_contents[file_length+1];
            bzero(file_contents, file_length+1);
            better_read(fd, file_contents, file_length, __FILE__, __LINE__);
            close(fd);

            /* compute new hash code */
            char * new_hash = malloc(sizeof(char) * (SHA256_DIGEST_LENGTH));
            SHA256((const unsigned char * ) file_contents, file_length, (unsigned char *) new_hash);

            if (strcmp(old_hash, new_hash) == 0)
            {
                /* hashes are the same, don't do anything */
                free(new_hash);
            }
            else
            {
                /* hashes are different, update .manifest */
                free(old_hash);
                root->hash_code = new_hash;
            }
        }

        root = root->next;
    }
}

char * fetch_server_manifest(char * proj_name)
{
    char * test_manifest = "1\n1 test/client_cmds.c fae61169ea07ee9866423547fb0933bbb5e993815b77238939fd03d3cfbf0a95\n1 test/client_cmds.h 749cd04330813fc83b382ed43b0e8b2b32e02b87c112ea419253aca1bd36aaec\n1 test/client_main.c 19f3b9cab2bf01ad1ca6f4fef69cd2cd40715561347bb1572b3b9790da775627\n1 test/client_main.h 19d98e0ba7356f5f41a007f72392d887fba9473813a95de4d87c86231df32efd\n1 test/fileIO.c dd0e78c28ada887212b9962029e948cf6c9c24cc8676d83ecccd5f76b263bc64\n1 test/fileIO.h c2e4d92b866763d78f3eb8aed5c38bf83685863177dfa45dab868fdfcbd23daf\n1 test/flags.h a20b6711c9fbb33f183625a4cc5e71aab2a5667daeddaa1d1e14752bed2c33811\n1 test/test_dir/server_cmds.c 74a3652dd0d4de91d9fa10aef37944794ce356c54699abf8c1c5f12451bd843a\n1 test/test_dir/server_cmds.h 9ca0c92b33797e54dfb4eda53744518dc0ea0be53603f27b8166774f9d23b7a0\n1 test/test_dir/server_main.c 96b51064be18c658669a4f56d9b73e4d8410b15953debf30ed0891df62af417b\n1 test/test_dir/server_main.h 97b39e84c4adb32736f562020d2f774723c9be3d0096468d4a666acae29b68ba";
    char * server_manifest = (char*)malloc(strlen(test_manifest)+1);
    strcpy(server_manifest, test_manifest);
    return server_manifest;
}

char * fetch_client_manifest(char * proj_name)
{
    char manifest_path[strlen("/.manifest") + strlen(proj_name) + 1];
    sprintf(manifest_path, "%s/.manifest", proj_name);
    int fd_man = open(manifest_path, O_RDONLY);

    int file_length = lseek(fd_man, 0, SEEK_END);
    lseek(fd_man, 0, SEEK_SET);

    char * manifest_buffer = (char*)malloc(sizeof(char) * file_length);
    if (better_read(fd_man, manifest_buffer, file_length, __FILE__, __LINE__))
        return manifest_buffer;

    close(fd_man);

    return NULL;
}