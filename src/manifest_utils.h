#ifndef MANIFEST_UTILS_H
#define MANIFEST_UTILS_H

#include <stdio.h>

/* Enum definitions for type of node */
enum entry_type{ROOT, FILE, DIRECTORY};

/* Struct definition for each node in the tree */
typedef struct manifest_entry {
    int version;
    enum entry_type type;
    char * file_path;
    char * hash_code;
    int num_children;
    struct manifest_entry * child;
    struct manifest_entry * sibling;
} manifest_entry;

/* read manifest file contents and generate a tree of manifest entries sorted by path */
manifest_entry * build_manifest_tree(char * file_contents, char * proj_name);

/* fetch manifest file contents from server from project name */
char * fetch_server_manifest(char * proj_name);

/* fetch manifest file contents from local copy of project */
char * fetch_client_manifest(char * proj_name);

/* store node into appropriate spot in the directory tree */
int add_manifest_entry(manifest_entry * root, manifest_entry * new_entry, char * path);

#endif