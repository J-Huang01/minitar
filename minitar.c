#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "minitar.h"

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 512

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
    // Have to initially set header's checksum to "all blanks"
    memset(header->chksum, ' ', 8);
    unsigned sum = 0;
    char *bytes = (char *)header;
    for (int i = 0; i < sizeof(tar_header); i++) {
        sum += bytes[i];
    }
    snprintf(header->chksum, 8, "%07o", sum);
}

// /*
//  * Populates a tar header block pointed to by 'header' with metadata about
//  * the file identified by 'file_name'.
//  * Returns 0 on success or -1 if an error occurs
//  */
int fill_tar_header(tar_header *header, const char *file_name) {
    memset(header, 0, sizeof(tar_header));
    char err_msg[MAX_MSG_LEN];
    struct stat stat_buf;
    // stat is a system call to inspect file metadata
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    strncpy(header->name, file_name, 100); // Name of the file, null-terminated string
    snprintf(header->mode, 8, "%07o", stat_buf.st_mode & 07777); // Permissions for file, 0-padded octal

    snprintf(header->uid, 8, "%07o", stat_buf.st_uid); // Owner ID of the file, 0-padded octal
    struct passwd *pwd = getpwuid(stat_buf.st_uid); // Look up name corresponding to owner ID
    if (pwd == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->uname, pwd->pw_name, 32); // Owner  name of the file, null-terminated string

    snprintf(header->gid, 8, "%07o", stat_buf.st_gid); // Group ID of the file, 0-padded octal
    struct group *grp = getgrgid(stat_buf.st_gid); // Look up name corresponding to group ID
    if (grp == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->gname, grp->gr_name, 32); // Group name of the file, null-terminated string

    snprintf(header->size, 12, "%011o", (unsigned)stat_buf.st_size); // File size, 0-padded octal
    snprintf(header->mtime, 12, "%011o", (unsigned)stat_buf.st_mtime); // Modification time, 0-padded octal
    header->typeflag = REGTYPE; // File type, always regular file in this project
    strncpy(header->magic, MAGIC, 6); // Special, standardized sequence of bytes
    memcpy(header->version, "00", 2); // A bit weird, sidesteps null termination
    snprintf(header->devmajor, 8, "%07o", major(stat_buf.st_dev)); // Major device number, 0-padded octal
    snprintf(header->devminor, 8, "%07o", minor(stat_buf.st_dev)); // Minor device number, 0-padded octal

    compute_checksum(header);
    return 0;
}

/*
 * Removes 'nbytes' bytes from the file identified by 'file_name'
 * Returns 0 upon success, -1 upon error
 * Note: This function uses lower-level I/O syscalls (not stdio), which we'll learn about later
 */
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
    char err_msg[MAX_MSG_LEN];
    // Note: ftruncate does not work with O_APPEND
    int fd = open(file_name, O_WRONLY);
    if (fd == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to open file %s", file_name);
        perror(err_msg);
        return -1;
    }
    //  Seek to end of file - nbytes
    off_t current_pos = lseek(fd, -1 * nbytes, SEEK_END);
    if (current_pos == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to seek in file %s", file_name);
        perror(err_msg);
        close(fd);
        return -1;
    }
    // Remove all contents of file past current position
    if (ftruncate(fd, current_pos) == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
        perror(err_msg);
        close(fd);
        return -1;
    }
    if (close(fd) == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to close file %s", file_name);
        perror(err_msg);
        return -1;
    }
    return 0;
}
//this is a helper function
//copy_helper can copy content from one 512 block to another
void copy_helper (FILE *target, FILE *source) {
    fseek(source, 0, SEEK_END);//move pointer to end
    int size = ftell(source);//find file size
    fseek(source, 0, SEEK_SET);//move pointer to start

    char buffer[512];//create a 512 bytes block
    memset(buffer, 0, 512);//initial it to all 0

    //loops through until all of the data has been written
    for (int i = 0; i < size; i += 512) {
        int copy_bytes = size - i;//finds the number of bytes to be read from the source file
        if (copy_bytes >= 512) {
            copy_bytes = 512;
        }
        fread(&buffer, copy_bytes, 1, source);
        fwrite(&buffer, 512, 1, target);
    }
}

int create_archive(const char *archive_name, const file_list_t *files) {
    FILE *archive = fopen(archive_name, "w");//open the archive file
    if (archive == NULL) {
        perror("archive open failed");
        return 1;
    }
    node_t *curr_node = files->head;
    tar_header buffer;
    int header;
    while(curr_node != NULL) {//iterate all files in lists.
        FILE *source = fopen(curr_node->name, "r");//open list files
        if (source == NULL) {
            perror("file open failed");
            return 1;
        }
        header = fill_tar_header(&buffer, curr_node->name);
        if (header == 0) {
            fwrite(&buffer, 512, 1, archive);//create the file header
        }
        copy_helper(archive, source);
        curr_node = curr_node->next;
        fclose(source);
    }

    //sets the two 512 blocks of zero bytes at the end of the archive file
    char Footer[1024];
    memset(Footer, 0, 1024);
    fwrite(&Footer, 1024, 1, archive);

    fclose(archive);
    return 0;
}

int append_files_to_archive(const char *archive_name, const file_list_t *files) {
    
    FILE *archive;
    if (!(archive = fopen(archive_name, "r"))){//check whether archive exist
        perror("archive not exist");
        return 1;
    }
    int remove_footer = remove_trailing_bytes(archive_name, 1024);//delete footer before append new file
    if (remove_footer == 1) {
        perror("remove footer failed");
        return 1;
    }
    //append file to archive
    node_t *curr_node = files->head;
    tar_header buffer;
    int header;
    archive = fopen(archive_name, "a");//"a" is to append data to a file
    if (archive == NULL) {//check if archive open
        perror("archive open failed");
        return 1;
    }
    while(curr_node != NULL) {
        FILE *source = fopen(curr_node->name, "r");//open file
        if (source == NULL) {//check error
            perror("file open failed");
            return 1;
        }
        header = fill_tar_header(&buffer, curr_node->name);
        if (header == 0) {
            fwrite(&buffer, 512, 1, archive);//create the file header
        }
        copy_helper(archive, source);
        curr_node = curr_node->next;
        fclose(source);
    }

    char Footer[1024];//set new footer blocks
    memset(Footer, 0, 1024);
    fwrite(&Footer, 1024, 1, archive);

    fclose(archive);
    return 0;
}
 
int get_archive_file_list(const char *archive_name, file_list_t *files) {
    FILE *archive = fopen(archive_name, "r");
    if (archive == NULL) {
        perror("archive open failed");
        return 1;
    }
    fseek(archive, 0, SEEK_END);//calcute size without footer
    int size = ftell(archive) - 1024;//1024 is the footer size
    fseek(archive, 0, SEEK_SET);//set pointer point to begin position
    int i = 0;
    while (ftell(archive) < size){
        tar_header header; 
        fread(&header, sizeof(tar_header), 1, archive);//read header from the archive
        if(ferror(archive) != 0){
            perror("read failed");
            return 1;
        }
        
        int add_list = file_list_add(files, header.name);//add file name to the list
        if (add_list == 1) {
            perror("add list failed");
            return 1;
        }
        int h_size = 0;//the size of the header file
        int oct_pow = 1;
        for (int j = 11; j >= 0; j--){
        if (header.size[j] >= '0' && header.size[j] <= '9') {
            h_size += (header.size[j] - '0') * oct_pow;
            oct_pow = oct_pow << 3;//transfer octal number to decimal number
        }
    }
        i += (h_size/512 + 2) * 512;//compute the position of the next header
        fseek(archive, i, SEEK_SET);
    }
    return 0;
}


int extract_files_from_archive(const char *archive_name) {
    FILE *archive = fopen(archive_name, "r");
    if (archive == NULL) {//check if archive opened
        perror("archive open failed");
        return 1;
    }
    // fseek(archive, 0, SEEK_END);//calcute size without footer
    // int size = ftell(archive) - 1024;//1024 is the footer size
    // fseek(archive, 0, SEEK_SET);//set pointer point to begin position

    char* buffer = malloc(512);

    fread(buffer,100,1,archive); 
        if(ferror(archive) != 0){
            perror("read failed");
            return 1;
        }
    while ((int) buffer[0] !=0){//loop until the file pointer before the footer;
        FILE *source = fopen(buffer, "w");
        if (source == NULL) {
            perror("file open failed");
            return 1;
        }
        fseek(archive,24,SEEK_CUR);//read size
        memset(buffer,0,512);
        fread(buffer,12,1,archive);

        int h_size = 0;//the size of the header file
        int oct_pow = 1;
        for (int j = 11; j >= 0; j--){
            if (buffer[j] >= '0' && buffer[j] <= '9') {
                h_size += (buffer[j] - '0') * oct_pow;
                oct_pow = oct_pow << 3;//transfer octal number to decimal number
            }
        }   
        fseek(archive,376,SEEK_CUR);

        int padding_zero = h_size % 512;//padding zero left in the block
        int num = h_size/512;
        for(int i = 0; i < num; i++){
            fread(buffer,512,1,archive);
            fwrite(buffer,512,1,source);
        }

        if(padding_zero){
            fread(buffer, h_size, 1, archive);//reads the contents of the header file from the archive
            fwrite(buffer, h_size, 1, source);//writes it to the newly opened file
            fseek(archive,(512 - padding_zero),SEEK_CUR);
            memset(buffer,0,512);
        }
        fclose(source);
        // the next read will be at the beginning of the next header
        fread(buffer, 100,1, archive);
    }
    free(buffer);
    fclose(archive);
    return 0;
}