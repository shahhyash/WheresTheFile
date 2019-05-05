#ifndef MANIFEST_UTILS_H
#define MANIFEST_UTILS_H

#include <stdio.h>

/* Struct definition for each node in the tree */
typedef struct manifest_entry {
    int version;
    char * file_path;
    char * hash_code;
    struct manifest_entry * next;
} manifest_entry;

/* Struct definition for update entry */
typedef struct update_entry
{
    char code;
    char * file_path;
    struct update_entry * next;
} update_entry;

/* read manifest file contents and generate a tree of manifest entries sorted by path */
manifest_entry * read_manifest_file(char * file_contents);

/* fetch manifest file contents from server from project name */
char * fetch_server_manifest(int sd, char * proj_name);

/* fetch manifest file contents from local copy of project */
char * fetch_client_manifest(char * proj_name);

/* prints out manifest tree with paths */
void print_manifest(manifest_entry * root);

/* frees all allocated memory for manifest tree */
void free_manifest(manifest_entry * root);

/* generate and update hashes for all files in manifest - different from commit bc it does not modify .manifest */
void update_hashes(manifest_entry * root);

/* read .update file and create a linked list of it */
update_entry * fetch_updates (char * proj_name);

/* frees all allocated memory for update list */
void free_updates(update_entry * root);

#endif
