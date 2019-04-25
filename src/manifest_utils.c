#include "manifest_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

manifest_entry * build_manifest_tree(char * file_contents, char * proj_name)
{        
    /* create root node for tree */
    manifest_entry * root = (manifest_entry*)malloc(sizeof(manifest_entry));
    root->type=MF_ROOT;
    root->version=atoi(file_contents);
    root->name=(char*)malloc(strlen(proj_name)+1);
    strcpy(root->name, proj_name);
    root->file_path = root->name;
    root->num_children=0;
    root->child=NULL;
    root->sibling=NULL;

    manifest_entry * current_node = NULL;

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
            current_node = (manifest_entry*)malloc(sizeof(manifest_entry));
            current_node->type = MF_FILE;
            current_node->version = version;
            current_node->file_path = file_path;
            current_node->hash_code = hash_code;
            current_node->num_children = 0;
            current_node->sibling = NULL;
            current_node->child = NULL;

            /* add entry to appropriate location in tree */
            add_manifest_entry(root, current_node);
        }

        manifest_line = strtok_r(NULL, "\n", &line_saveptr);
    }

    return root;
}

int add_manifest_entry(manifest_entry * root, manifest_entry * new_entry)
{
    manifest_entry * ptr = root;

    char file_path[strlen(new_entry->file_path) + 1];
    strcpy(file_path, new_entry->file_path);

    strtok(file_path, "/");                     /* skip project name part of path */
    char * dir_name = strtok(NULL, "/");

    while (dir_name)
    {
        char * next = strtok(NULL, "/");
        if (next)
        {
            /* dir_name is a directory - search ptr's children and if you reach the end w/out finding it, make a directory node */
            manifest_entry * prev_child = NULL;
            manifest_entry * child = ptr->child;
            while (child)
            {
                if (child->type == MF_DIR)
                {
                    if (strcmp(child->name, dir_name) == 0)
                    {
                        ptr = child;
                        break;
                    }
                }

                prev_child = child;
                child = child->sibling;
            }

            /* if child is NULL, then search failed. Add new sibling directory */
            if (child == NULL)
            {
                /* increment children count for parent directory */
                ++ptr->num_children;

                /* set values for new directory node */
                manifest_entry * new_dir = (manifest_entry*) malloc(sizeof(manifest_entry));
                new_dir->type=MF_DIR;
                new_dir->name = dir_name;
                new_dir->sibling=NULL;
                new_dir->child=NULL;
                new_dir->num_children=0;

                /* if prev_child is null, then parent had no children to begin with */
                if (prev_child)
                {
                    prev_child->sibling = new_dir;
                    ptr = prev_child->sibling;
                }
                else
                {
                    ptr->child = new_dir;
                    ptr = ptr->child;
                }
            }
        }
        else
        {
            /* dir_name is the file -> append new_entry to child nodes */
            
            new_entry->name = (char*)malloc(strlen(dir_name)+1);
            strcpy(new_entry->name, dir_name);

            ++ptr->num_children;

            manifest_entry * prev_child = NULL;
            manifest_entry * child = ptr->child;
            while(child)
            {
                prev_child = child;
                child = child->sibling;
            }

            if (prev_child)
            {
                prev_child->sibling = new_entry;
            }
            else
            {
                ptr->child = new_entry;
            }
        }
        
        dir_name = next;
    }

    return 0;
}

void print_manifest_tree(manifest_entry * root, char * path)
{
    if(root->type == MF_ROOT)
    {
        printf("Found root directory %s. Current Path: %s/\n", root->name, root->name);
        manifest_entry * child = root->child;
        while (child)
        {
            print_manifest_tree(child, root->name);
            child = child->sibling;
        }
    }
    else if (root->type == MF_DIR)
    {
        printf("Found sub directory %s. Current Path: %s/%s/\n", root->name, path, root->name);
        char new_path[strlen(path) + strlen(root->name) + 1];
        sprintf(new_path, "%s/%s", path, root->name);
        manifest_entry * child = root->child;
        while (child)
        {
            print_manifest_tree(child, new_path);
            child = child->sibling;
        }
    }
    else
    {
        printf("Found manifest item %s. File Path: %s\n", root->name, root->file_path);
    }
}

void free_manifest_tree(manifest_entry * root)
{
    if (root->type == MF_ROOT || root->type == MF_DIR)
    {
        free(root->name);
        manifest_entry * child = root->child;
        while (child)
        {
            free_manifest_tree(child);
            manifest_entry * sibling = child->sibling;
            free(child);
            child = sibling;
        }
        free(root);
    }
    else
    {
        free(root->name);
        free(root->file_path);
        free(root->hash_code);
    }
}


char * fetch_server_manifest(char * proj_name)
{
    char * test_manifest = "1\n1 test/client_cmds.c fae61169ea07ee9866423547fb0933bbb5e993815b77238939fd03d3cfbf0a95\n1 test/client_cmds.h 749cd04330813fc83b382ed43b0e8b2b32e02b87c112ea419253aca1bd36aaec\n1 test/client_main.c 19f3b9cab2bf01ad1ca6f4fef69cd2cd40715561347bb1572b3b9790da775627\n1 test/client_main.h 19d98e0ba7356f5f41a007f72392d887fba9473813a95de4d87c86231df32efd\n1 test/fileIO.c dd0e78c28ada887212b9962029e948cf6c9c24cc8676d83ecccd5f76b263bc64\n1 test/fileIO.h c2e4d92b866763d78f3eb8aed5c38bf83685863177dfa45dab868fdfcbd23daf\n1 test/flags.h a20b6711c9fbb33f183625a4cc5e71aab2a5667daeddaa1d1e14752bed2c3381\n1 test/server_cmds.c 74a3652dd0d4de91d9fa10aef37944794ce356c54699abf8c1c5f12451bd843a\n1 test/server_cmds.h 9ca0c92b33797e54dfb4eda53744518dc0ea0be53603f27b8166774f9d23b7a0\n1 test/server_main.c 96b51064be18c658669a4f56d9b73e4d8410b15953debf30ed0891df62af417b\n1 test/server_main.h 97b39e84c4adb32736f562020d2f774723c9be3d0096468d4a666acae29b68ba\n";
    char * server_manifest = (char*)malloc(strlen(test_manifest)+1);
    strcpy(server_manifest, test_manifest);
    return server_manifest;
}

char * fetch_client_manifest(char * proj_name)
{
    /* TODO: Implement this */
    return NULL;
}