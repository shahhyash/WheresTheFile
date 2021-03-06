#include "server_cmds.h"
#include "fileIO.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>             // FILE IO
#include <fcntl.h>              // open flags
#include "compression.h"
#include "flags.h"
#include "threads_and_locks.h"
#include "manifest_utils.h"
#include "commit_utils.h"

extern int is_table_lcked;
extern int num_access;
extern pthread_mutex_t access_lock;
extern pthread_mutex_t table_lck;
extern proj_t * proj_list;

/*
 *      Clones project and sends it to client. Returns 0 on success; 1 otherwise.
 */
int checkout(int sd, char * proj_name)
{
        // Return if .server_repo folder does not already exist
        if (dir_exists(".server_repo") == 0)
                return 1;
        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        else
        {
                if (better_send(sd, "Found repository. Sending now.", 30, 0, __FILE__, __LINE__) != 1)
                        return 1;
        }
        while (is_table_lcked)
                printf("table locked.\n");
        pthread_mutex_lock(lock);
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char new_proj_name[strlen(proj_name)+1+strlen(".server_repo/")];
        bzero(new_proj_name, strlen(proj_name)+1+strlen(".server_repo/"));
        sprintf(new_proj_name, ".server_repo/%s", proj_name);

        int ret = compress_and_send(sd, new_proj_name, TRUE);

        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);
        return ret;
}

/*
 *      Creates directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int create(int sd, char * proj_name)
{
        if (dir_exists(".server_repo") == 0)
                return 1;
        printf("Seeing is table is locked...\n");
        pthread_mutex_lock(&table_lck);
        printf("Table is unlocked.\n");
        is_table_lcked = TRUE;
        pthread_mutex_t * lock = add_project(proj_name, __FILE__, __LINE__);
        if (lock == NULL)
        {
                pthread_mutex_unlock(&table_lck);
                is_table_lcked = FALSE;
                return 1;
        }
        while (!is_table_lcked)
        {
                printf("Table locked\n");
        }
        pthread_mutex_lock(lock);
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char new_proj_name[strlen(proj_name)+1+strlen(".server_repo/")];
        bzero(new_proj_name, strlen(proj_name)+1+strlen(".server_repo/"));
        sprintf(new_proj_name, ".server_repo/%s", proj_name);

        // create new project in .server_repo folder
        if (make_dir(new_proj_name, __FILE__, __LINE__) != 0)
        {
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                is_table_lcked = FALSE;
                pthread_mutex_unlock(&table_lck);
                return 1;
        }
        // printf("%s\n", new_proj_name);

        // Init manifest
        char manifest[strlen(new_proj_name)+11];
        bzero(manifest, strlen(new_proj_name)+11);
        sprintf(manifest, "%s/.manifest", new_proj_name);
        int fd = open(manifest, O_WRONLY | O_CREAT | O_TRUNC, 00600);
        char * data = "2\n";
        if (better_write(fd, data, strlen(data), __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[create] Error returned by better_write. FILE: %s. LINE: %d\n", __FILE__, __LINE__);
                close(fd);
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                is_table_lcked = FALSE;
                pthread_mutex_unlock(&table_lck);
                return 1;
        }
        close(fd);
        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);
        is_table_lcked = FALSE;
        pthread_mutex_unlock(&table_lck);
        return send_manifest(sd, proj_name);
}

/*
 *      Deletes directory of name proj_name.
 *      Returns 1 on error; 0 otherwise.
 */
int destroy(char * proj_name)
{
        if (dir_exists(".server_repo") == 0)
                return 1;
        printf("Seeing is table is locked...\n");
        pthread_mutex_lock(&table_lck);
        printf("Table is unlocked.");
        is_table_lcked = TRUE;
        proj_t * to_be_deleted = delete_project(proj_name);
        if (to_be_deleted == NULL)
        {
                is_table_lcked = FALSE;
                pthread_mutex_unlock(&table_lck);
                fprintf(stderr, "[create] Error deleting project from repository.\n");
                return 1;
        }
        if (!dir_exists(".server_repo"))
        {
                fprintf(stderr, "[create] Project does not exist.\n");
                pthread_mutex_unlock(&table_lck);
                return 1;
        }
        char new_proj_name[strlen(proj_name)+1+strlen(".server_repo/")];
        bzero(new_proj_name, strlen(proj_name)+1+strlen(".server_repo/"));
        sprintf(new_proj_name, ".server_repo/%s", proj_name);
        if (!dir_exists(new_proj_name))
        {
                fprintf(stderr, "[create] Project does not exist.\n");
                pthread_mutex_unlock(&table_lck);
                return 1;
        }
        printf("removing directory: %s\n", new_proj_name);
        remove_dir(new_proj_name);
        char bck_dir[strlen(proj_name)+1+strlen(".server_bck/")];
        bzero(bck_dir, strlen(proj_name)+1+strlen(".server_bck/"));
        sprintf(bck_dir, ".server_bck/%s", proj_name);
        if (dir_exists(bck_dir))
        {
                remove_dir(bck_dir);
        }

        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(&to_be_deleted->lock);
        pthread_mutex_destroy(&to_be_deleted->lock);
        free(to_be_deleted->proj_name);
        free(to_be_deleted);
        is_table_lcked = FALSE;
        pthread_mutex_unlock(&table_lck);
        return 0;
}

/*
 *      Sends manifest for project proj_name to client.
 *      Returns 0 on success, 1 otherwise.
 */
int send_manifest(int sd, char * proj_name)
{
        // Return if .server_repo folder does not already exist
        if (dir_exists(".server_repo") == 0)
                return 1;
        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        else
        {
                if (better_send(sd, "Found repository. Sending now.", 30, 0, __FILE__, __LINE__) != 1)
                        return 1;
        }
        while (is_table_lcked)
                printf("table locked.\n");
        pthread_mutex_lock(lock);
        // Mark another thread as accessing
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char file_name[strlen(proj_name)+1+strlen(".server_repo/")+strlen("/.manifest")];
        bzero(file_name, strlen(proj_name)+1+strlen(".server_repo/")+strlen("/.manifest"));
        // Read and compress manifest
        sprintf(file_name, ".server_repo/%s/.manifest", proj_name);
        printf("Sending %s\n", file_name);
        int ret = compress_and_send(sd, file_name, TRUE);

        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);

        return ret;
}

/*
 *      Sends specified file for project proj_name to client.
 *      Returns 0 on success, 1 otherwise.
 */
int send_server_copy(int sd, char * file_path)
{
        /* Return if .server_repo folder does not already exist */
        if (dir_exists(".server_repo") == 0)
                return 1;

        /* fetch project name from file path */
        char * sub_path = index(file_path, '/');
        printf("Fetching %s from %s\n", sub_path, file_path);
        int proj_name_length = sub_path - file_path;
        char proj_name[proj_name_length + 1];
        strncpy(proj_name, file_path, proj_name_length);
        proj_name[proj_name_length] = '\0';

        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        else
        {
                if (better_send(sd, "Found repository. Sending now.", 30, 0, __FILE__, __LINE__) != 1)
                        return 1;
        }
        while (is_table_lcked)
                printf("table locked.\n");
        pthread_mutex_lock(lock);

        /* Mark another thread as accessing */
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char file_name[strlen(".server_repo/") + strlen(file_path) + 1];
        bzero(file_name, strlen(".server_repo/") + strlen(file_path) + 1);

        /* Read and compress manifest */
        sprintf(file_name, ".server_repo/%s", file_path);

        int ret = compress_and_send(sd, file_name, TRUE);

        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);

        return ret;
}
/*
 *      Sends history file to client.
 *      Return 0 on success, 1 otherwise.
 */
int history(int sd, char * proj_name)
{
        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        else
        {
                if (better_send(sd, "Found repository. Sending now.", 30, 0, __FILE__, __LINE__) != 1)
                        return 1;
        }
        while (is_table_lcked)
                printf("table locked.\n");
        pthread_mutex_lock(lock);

        /* Mark another thread as accessing */
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);
        char hist_file[strlen(proj_name)+strlen(".server_bck/")+strlen("/.history")+1];
        bzero(hist_file, strlen(proj_name)+strlen(".server_bck/")+strlen("/.history")+1);
        sprintf(hist_file, ".server_bck/%s/.history", proj_name);
        if (!file_exists(hist_file))
        {
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                fprintf(stderr, "[history] No history for this file.\n");
                return 1;
        }
        if (better_send(sd, "Ok!                          !", 30, 0, __FILE__, __LINE__) != 1)
        {
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }
        if (compress_and_send(sd, hist_file, TRUE))
        {
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }
        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);
        return 0;
}
/*
 *      Rollsback server project version.
 *      Return 0 on success, 1 otherwise.
 */
int rollback(int sd, char * proj_name)
{
        if (dir_exists(".server_repo") == 0)
                return 1;
        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        else
        {
                if (better_send(sd, "Found repository. Version Num?", 30, 0, __FILE__, __LINE__) != 1)
                        return 1;
        }
        while (is_table_lcked)
                printf("table locked.\n");
        pthread_mutex_lock(lock);

        /* Mark another thread as accessing */
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);

        int ret = 0;
        int rollback_version;
        if(better_read(sd, (char*) &rollback_version, sizeof(int), __FILE__, __LINE__) != 1)
        {
                fprintf(stderr, "[rollback] ERROR: Unable to read rollback version from client.\n");
                ret = 1;
        }

        int is_diff_version;
        int current_version = get_version(proj_name, proj_name, &is_diff_version);

        if (ret == 0 && !dir_exists(".server_bck"))
        {
                fprintf(stderr, "[rollback] ERROR: Backups directory does not exist on the repository.\n");
                ret = 1;
        }

        /* rollback version should be less than current version to be valid */
        /* commit version should be greater than 1 otherwise there can't be a backup version */
        if (ret == 0 && current_version > 2 && rollback_version < current_version)
        {
                char rollback_version_str[128];
                bzero(rollback_version_str, 128);
                sprintf(rollback_version_str, "%d", rollback_version);
                char backup[strlen(".server_bck/") +strlen(proj_name)+1+ strlen(proj_name)+strlen(rollback_version_str)+2];
                bzero(backup, strlen(".server_bck/") +strlen(proj_name)+1+ strlen(proj_name)+strlen(rollback_version_str)+2);
                sprintf(backup, ".server_bck/%s/%s_%s", proj_name, proj_name, rollback_version_str);

                if (file_exists(backup))
                {
                        /* delete current version */
                        char project_dir[strlen(".server_repo/") + strlen(proj_name) + 1];
                        sprintf(project_dir, ".server_repo/%s", proj_name);
                        remove_dir(project_dir);

                        /* check if there are versions in between */
                        int difference = current_version - rollback_version;
                        if (difference > 1)
                        {
                                int i;
                                for(i=0; i<difference; i++)
                                {
                                        int version = rollback_version + 1 + i;
                                        char version_str[128];
                                        bzero(version_str, 128);
                                        sprintf(version_str, "%d", version);
                                        char to_delete[strlen(".server_bck/") +strlen(proj_name)+1+ strlen(proj_name)+strlen(version_str)+2];
                                        bzero(backup, strlen(".server_bck/") +strlen(proj_name)+1+ strlen(proj_name)+strlen(version_str)+2);
                                        sprintf(backup, ".server_bck/%s/%s_%s", proj_name, proj_name, version_str);

                                        remove(to_delete);
                                }
                        }

                        /* backupexists! let's revert it now */

                        // int length = fseek(fd, 0, SEEK_END);
                        // fseek(fd, 0, SEEK_SET);

                        // char compressed[length+1];
                        // better_read(fd, compressed, length, __FILE__, __LINE__);
                        // compressed[length] = '\0';
                        // write metadata:m decompressed size \t compressed size
                        FILE * fp = fopen(backup, "r");
                        int decompressed_size, compressed_size;
                        fscanf(fp, "%d\t", &decompressed_size);
                        fscanf(fp, "%d\n", &compressed_size);
                        fclose(fp);
                        char buf[compressed_size+1];
                        int fd = open(backup, O_RDONLY, 00600);
                        int size = lseek(fd, 0, SEEK_END);
                        lseek(fd, 0, SEEK_SET);
                        char file[size+1];
                        bzero(file, size+1);
                        better_read(fd, file, size, __FILE__, __LINE__);
                        close(fd);
                        int i = 0;
                        while (file[i++] != '\n');
                        printf("%d\n", i);

                        // printf("%d %d\n", decompressed_size, compressed_size);
                        char * decompressed = _decompress(&file[i], decompressed_size, compressed_size);
                        printf("decompressed %s\n", decompressed);
                        recursive_unzip(decompressed, TRUE);

                        free(decompressed);
                        remove(backup);
                }
                else
                {
                        fprintf(stderr, "[rollback] Backup file %s does not exist on the repo.\n", backup);
                        ret = 1;
                }
        }
        else
                ret = 1;

        /* Unmark as another thread as accessing */
        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);

        return ret;
}
int receive_commit(int sd, char * proj_name)
{
        if (dir_exists(".server_repo") == 0)
                return 1;
        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        else
        {
                if (better_send(sd, "Found repository. Send .commit", 30, 0, __FILE__, __LINE__) != 1)
                        return 1;
        }
        while (is_table_lcked)
                printf("table locked.\n");
        pthread_mutex_lock(lock);

        /* Mark another thread as accessing */
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);

        /* Fetch commit file from input socket stream */
        char * commit_contents = receive_file(sd);
        if (commit_contents == NULL)
        {
                fprintf(stderr, "[fetch_server_manifest] Error decompressing.\n");
                /* free allocated memory for received file */
                free(commit_contents);
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }

        /* parse the message information out of the file */
        char * newline = strstr(commit_contents, "\n");

        int size;
        sscanf(&newline[2], "%d\n", &size);

        char * file = (char *) malloc(sizeof(char)*(size+1));
        bzero(file, size+1);
        newline = strstr(&newline[2], "\n");
        newline = strstr(&newline[1], "\n");
        strncpy(file, &newline[1], size);

        /* increment commit count for this project */
        int commit_id = increment_commit_count(proj_name);

        int digits = 1;
        if (commit_id > 9)
                ++digits;
        if (commit_id > 99)
                ++digits;
        if (commit_id > 999)
                ++digits;

        /* generate file path based on the id fetched from incrementing commit count */
        int commit_path_size = strlen(".server_repo/") + strlen(proj_name) + 1 + strlen(".commit") + digits + 1;
        char commit_path[commit_path_size];
        sprintf(commit_path, ".server_repo/%s/.commit%d", proj_name, commit_id);

        /* open commit file and store contents into it */
        int fd_commit = open(commit_path, O_RDWR | O_CREAT, 00600);
        if (fd_commit == -1)
        {
                fprintf(stderr, "[receive_commit] ERROR: Unable to open file %s to store commit.\n", commit_path);
                close(fd_commit);

                /* free allocated memory for received file */
                free(commit_contents);
                free(file);
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }

        if (better_write(fd_commit, file, size, __FILE__, __LINE__) <= 0)
        {
                fprintf(stderr, "[receive_commit] ERROR: Unable to write to commit file.\n");
                close(fd_commit);

                /* free allocated memory for received file */
                free(commit_contents);
                free(file);
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }

        close(fd_commit);

        /* free allocated memory for received file */
        free(commit_contents);
        free(file);

        /* Unmark as another thread as accessing */
        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);

        return 0;
}

int push_handler(int sd, char * proj_name)
{
        if (dir_exists(".server_repo") == FALSE)
                return 1;
        pthread_mutex_t * lock = get_project_lock(proj_name);
        if (lock == NULL)
                return 1;
        else
        {
                if (better_send(sd, "Found repo, waiting on files..", 30, 0, __FILE__, __LINE__) != 1)
                        return 1;
        }
        while (is_table_lcked)
                printf("table locked.\n");
        pthread_mutex_lock(lock);

        /* Mark another thread as accessing */
        pthread_mutex_lock(&access_lock);
        num_access++;
        pthread_mutex_unlock(&access_lock);

        // Receive client's compress file
        char * _client_commit = receive_file(sd);

        char * client_commit = strstr(_client_commit, "\n");
        printf("client %s\n", client_commit);
        client_commit = strstr(&client_commit[1], "\n");
        client_commit = strstr(&client_commit[1], "\n");
        client_commit = &client_commit[1];
        // Read into linked list struct
        char * hash_cli = hash(client_commit);
        int num_commits = get_commit_count(proj_name);
        int cur_commit = 1;
        int found_match = FALSE;
        char * server_commit;
        /* when we get the commit file, we make a linked list out of it and then compare all other commits to see if one exists */
        for (cur_commit = 1; cur_commit <= num_commits; cur_commit++)
        {
                server_commit = fetch_commit_file(proj_name, TRUE, cur_commit);
                char * hash_serv = hash(server_commit);
                free(server_commit);
                if (strcmp(hash_cli, hash_serv) == 0)
                {
                        found_match = TRUE;
                        free(hash_serv);
                        break;
                }
                free(hash_serv);
        }
        free(hash_cli);
        if (!found_match)
        {

                free(_client_commit);
                fprintf(stderr, "[push] No matching commit found.\n");
                /* Unmark as another thread as accessing */
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }
        else
        {
                // delete all commits
                for (cur_commit = 1; cur_commit <= num_commits; cur_commit++)
                {
                        char num[128];
                        bzero(num, 128);
                        sprintf(num, "%d", cur_commit);
                        char old[strlen(proj_name)+strlen(".server_bck")+2+strlen(num)+1];
                        sprintf(old, ".server_bck/%s/%s", proj_name, num);
                        remove(old);
                }
        }

        /* read decompressed file which will tell us the entire log of files that are changed with the files themselves */

        char proj_bck[strlen(".server_bck/") + strlen(proj_name)+ 1];
        sprintf(proj_bck, ".server_bck/%s", proj_name);
        if (!dir_exists(proj_bck))
        {
                make_dir(proj_bck, __FILE__, __LINE__);
        }
        /* compress and take a backup of the current project_manifest/directory and store in server_backups/ */
        int project_dir_path_size = strlen(".server_repo/") + strlen(proj_name) + 1;
        char project_dir_path[project_dir_path_size];
        sprintf(project_dir_path, ".server_repo/%s", proj_name);

        char * current_version_zip = recursive_zip(project_dir_path, TRUE);
        int compressed_size;
        char * compressed = _compress(current_version_zip, &compressed_size);

        /* write zip to .server_backups */
        int diff;
        int cur_version =  get_version(project_dir_path, project_dir_path, &diff);
        char cur_version_str[128];
        bzero(cur_version_str, 128);
        sprintf(cur_version_str, "%d", cur_version);
        char backup[strlen(".server_bck/") + strlen(proj_name)+ 1+ strlen(proj_name)+strlen(cur_version_str)+2];
        bzero(backup, strlen(".server_bck/") +strlen(proj_name)+ 1+ strlen(proj_name)+strlen(cur_version_str)+2);
        sprintf(backup, ".server_bck/%s/%s_%s", proj_name, proj_name, cur_version_str);
        int fd = open(backup, O_WRONLY | O_CREAT | O_TRUNC, 00600);
        char metadata[128];
        bzero(metadata, 128);
        // write metadata:m decompressed size \t compressed size
        sprintf(metadata, "%d\t%d\n", strlen(current_version_zip), compressed_size);
        free(current_version_zip);
        if (better_write(fd, metadata, strlen(metadata), __FILE__, __LINE__) != 1)
        {

                free(_client_commit);
                free(compressed);
                /* Unmark as another thread as accessing */
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }
        if (better_write(fd, compressed, compressed_size, __FILE__, __LINE__) != 1)
        {

                free(_client_commit);
                free(compressed);
                /* Unmark as another thread as accessing */
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }
        free(compressed);

        // fetch current version of files from client
        remove_dir(project_dir_path);
        char * cur_files = receive_file(sd);
        // printf("files:%s\n Done\n", cur_files);
        recursive_unzip(cur_files, TRUE);
        free(cur_files);
        int p_size = strlen(".server_repo/") + strlen(proj_name) + strlen("/.commit")+ 1;
        char commit_rem[p_size];
        sprintf(commit_rem, ".server_repo/%s/.commit", proj_name);
        remove(commit_rem);
        // Increment manifest
        int m_size = strlen(".server_repo/") + strlen(proj_name) + strlen("/.manifest")+ 1;
        char man_f[m_size];
        sprintf(man_f, ".server_repo/%s/.manifest", proj_name);
        FILE * man_fd = fopen(man_f, "r");
        if (man_fd == NULL)
        {
                free(_client_commit);
                fclose(man_fd);
                /* Unmark as another thread as accessing */
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }
        int version;
        fscanf(man_fd, "%d\n", &version);
        fclose(man_fd);
        update_manifest_version(project_dir_path, version+1);
        // increment manifest number

        /* iterate through each file and make sure that these changes are being logged to .histroy */
        int h_size = strlen(".server_bck/") + strlen(proj_name) + strlen("/.history")+ 1;
        char his_f[h_size];
        bzero(his_f, h_size);

        sprintf(his_f, ".server_bck/%s/.history", proj_name);
        int hd = open(his_f, O_RDWR | O_CREAT, 00600);
        lseek(hd, 0, SEEK_END);
        char num_buf[128];
        bzero(num_buf, 128);
        sprintf(num_buf, "%d\n", version);
        if (better_write(hd, num_buf, strlen(num_buf), __FILE__, __LINE__) != 1)
        {

                free(_client_commit);
                close(hd);
                /* Unmark as another thread as accessing */
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }
        if (better_write(hd, client_commit, strlen(client_commit), __FILE__, __LINE__) != 1)
        {

                free(_client_commit);
                close(hd);
                /* Unmark as another thread as accessing */
                pthread_mutex_lock(&access_lock);
                num_access--;
                pthread_mutex_unlock(&access_lock);
                pthread_mutex_unlock(lock);
                return 1;
        }
        free(_client_commit);
        close(hd);
        /* Unmark as another thread as accessing */
        pthread_mutex_lock(&access_lock);
        num_access--;
        pthread_mutex_unlock(&access_lock);
        pthread_mutex_unlock(lock);

        return 0;
}
