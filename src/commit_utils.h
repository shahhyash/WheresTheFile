#ifndef COMMIT_UTILS_H
#define COMMIT_UTILS_H

typedef struct commit_entry
{
    char op_code;
    int version;
    char * file_path;
    char * hash_code;
    struct commit_entry * next;
} commit_entry;

/*
 *  Fetch contents of commit file. If server flag is set to TRUE, then commit_id must be provided to identify
 *  which commit to fetch and return. 
 */
char * fetch_commit_file(char * proj_name, int is_server, int commit_id);

/* Builds commit linked list from file */
commit_entry * read_commit_file(char * file_contents);

/*
 *  Frees all allocated memory when linked list was being built
 */
void free_commit_list(commit_entry * root);

#endif