#include "manifest_utils.h"
#include <string.h>
#include <ctype.h>

manifest_entry * build_manifest_tree(char * file_contents, char * proj_name)
{        
        /* create root node for tree */
        manifest_entry * root = (manifest_entry*)malloc(sizeof(manifest_entry));
        root->type=ROOT;
        root->version=atoi(file_contents[0]);
        root->num_children=0;
        root->child=NULL;
        root->sibling=NULL;

        manifest_entry * current_node = NULL;

        /* iterate through file contents */
        int size = strlen(file_contents);
        int i;
        int mode = 0; /*0->root, 1->version number, 2->file_path, 3->hash_code*/
        int start = 0;
        for (i=0; i<size; i++)
        {          
            if (mode==1 && isdigit(file_contents[i]))
            {
                start = i;

                /* change mode so that it looks for end of version no then file_path now */
                mode = 2;
            }

            if (mode==2 && isspace(file_contents[i]))
            {
                char version_str[i-start+1];
                strncpy(&version_str, file_contents[start], i-start);
                
                /* create new node for current manifest entry */
                current_node = (manifest_entry*)malloc(sizeof(manifest_entry));
                current_node->type = FILE;
                current_node->version = atoi(version_str);
                current_node->num_children=0;
                current_node->sibling = NULL;
                current_node->child = NULL;
            }

            if (mode == 2 && isalnum(file_contents[i]))
            {
                /* mark start index of file path */
                start = i;

                /* set mode to 3 to look for end of file path and then the hash code */
                mode = 3;
            }

            if (mode == 3 && isspace(file_contents[i]))
            {
                /* reached end of file path. store it */
                char * file_path = (char*) malloc(sizeof(char) * (i-start+1));
                strncpy(file_path, &file_contents[start], i-start);
                current_node->file_path = file_path;
            }

            if (mode == 3 && isalnum(file_contents[i]))
            {
                /* reached beginning of hash - store index to beginning */
                start = i;

                /* set mode to 4 to look for end of hash code */
                mode = 4;
            }

            if (file_contents[i]=='\n')
            {
                /* reached end of line. if previous mode is 4, then we have a current node ready to store into tree */
                if (mode == 4)
                {
                    /* begin by storing hash code */
                    char * hash_code = (char*) malloc(sizeof(char) * (i-start+1));
                    strncpy(hash_code, &file_contents[start], i-start);

                    add_manifest_entry(root, current_node, current_node->file_path);
                }
                
                /* reset mode counter */
                mode = 1;
            }
        }
        
}

char * fetch_server_manifest(char * proj_name)
{
        char * test_manifest = "1\n1 test/client_cmds.c fae61169ea07ee9866423547fb0933bbb5e993815b77238939fd03d3cfbf0a95\n1 test/client_cmds.h 749cd04330813fc83b382ed43b0e8b2b32e02b87c112ea419253aca1bd36aaec\n1 test/client_main.c 19f3b9cab2bf01ad1ca6f4fef69cd2cd40715561347bb1572b3b9790da775627\n1 test/client_main.h 19d98e0ba7356f5f41a007f72392d887fba9473813a95de4d87c86231df32efd\n1 test/fileIO.c dd0e78c28ada887212b9962029e948cf6c9c24cc8676d83ecccd5f76b263bc64\n1 test/fileIO.h c2e4d92b866763d78f3eb8aed5c38bf83685863177dfa45dab868fdfcbd23daf\n1 test/flags.h a20b6711c9fbb33f183625a4cc5e71aab2a5667daeddaa1d1e14752bed2c3381\n1 test/server_cmds.c 74a3652dd0d4de91d9fa10aef37944794ce356c54699abf8c1c5f12451bd843a\n1 test/server_cmds.h 9ca0c92b33797e54dfb4eda53744518dc0ea0be53603f27b8166774f9d23b7a0\n1 test/server_main.c 96b51064be18c658669a4f56d9b73e4d8410b15953debf30ed0891df62af417b\n1 test/server_main.h 97b39e84c4adb32736f562020d2f774723c9be3d0096468d4a666acae29b68ba\n";
        char * server_manifest = (char*)malloc(strlen(test_manifest));
        strcpy(server_manifest, test_manifest);
        return server_manifest;
}

char * fetch_client_manifest(char * proj_name)
{
    /* TODO: Implement this */
}