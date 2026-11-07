#ifndef __FILE_IO_H
#define __FILE_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "lfs.h"
#include "lfs_util.h"

typedef struct file_system_config{
    struct lfs_config cfg; // LittleFS configuration
    lfs_t *lfs; // Pointer to the LittleFS object
    uint32_t block_size; // Size of a block in bytes
    uint32_t block_count; // Number of blocks in the filesystem
    struct file_function_t{
        int (*erase)(const struct lfs_config *c,lfs_size_t block); // Function to erase a block
        int (*read)(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, void *buffer, lfs_size_t size); // Function to read data from a block
        int (*write)(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, const void *buffer, lfs_size_t size); // Function to write data to a block
    } file_function; // Disk operations functions
}file_system_config_t;

/**
 * @brief Initializes the file system with the provided configuration.
 *
 * @param config Pointer to a file_system_config_t structure containing
 *               the configuration parameters for file system initialization.
 *
 * @return int Returns 0 on success, or a negative error code on failure.
 *
 * @note The config pointer must not be NULL.
 */
int file_system_init(file_system_config_t *config);

extern lfs_t lfs;

#define CONFIG_LITTLEFS &lfs

static inline FILE *file_open(const char *path, const char *mode) {
    // Use LittleFS to open a file
    lfs_file_t *file = malloc(sizeof(lfs_file_t));
    int lfs_status;
    if(file == NULL) {
        return NULL; // Memory allocation failed
    }
    int flags = LFS_O_RDONLY;

    if('r' == mode[0]) {
        flags = LFS_O_RDONLY;
    } else if('w' == mode[0]) {
        flags = LFS_O_WRONLY | LFS_O_CREAT;
    } else if('a' == mode[0]) {
        flags = LFS_O_CREAT | LFS_O_WRONLY | LFS_O_APPEND;
    } else if('r' == mode[0] && '+' == mode[1]) {
        flags = LFS_O_RDWR | LFS_O_CREAT;
    } else if('w' == mode[0] && '+' == mode[1]) {
        flags = LFS_O_RDWR | LFS_O_CREAT;
    }else if('a' == mode[0] && '+' == mode[1]) {
        flags = LFS_O_RDWR | LFS_O_APPEND | LFS_O_CREAT;
    } else {
        free(file);
        return NULL; // Unsupported mode
    }
    lfs_status = lfs_file_open(CONFIG_LITTLEFS,file, path, flags);
    if (lfs_status != LFS_ERR_OK) {
        free(file);
        return NULL; // Error opening file
    }
    return (FILE *)file; // Cast to FILE pointer
}

static inline int file_puts(const char *s,lfs_file_t *file) {
    if (file == NULL || s == NULL) {
        return -1; // Invalid file or string
    }
    lfs_ssize_t res = lfs_file_write(CONFIG_LITTLEFS, file, s, strlen(s));
    if (res < 0) {
        return -1; // Error writing to file
    }
    return res; // Return number of characters written
}

void fs_list_dir(const char *path);

#define fopen file_open // Open a file using LittleFS
#define fread(data,n,len,stream) lfs_file_read(CONFIG_LITTLEFS, (lfs_file_t *)stream, data, n * len) // Read data from a file
#define fwrite(data,n,len,stream) lfs_file_write(CONFIG_LITTLEFS, (lfs_file_t *)stream, data, n * len) // Write data to a file
#define fclose(stream) lfs_file_close(CONFIG_LITTLEFS, (lfs_file_t *)stream); free(stream) // Close a file and free resources
#define fseek(stream, offset, whence) lfs_file_seek(CONFIG_LITTLEFS, (lfs_file_t *)stream, offset, whence >= 0 ? 0:-1) // Seek in a file
#define rename(oldpath, newpath) lfs_rename(CONFIG_LITTLEFS, oldpath, newpath) // Rename a file
#define remove(path) lfs_remove(CONFIG_LITTLEFS, path) // Remove a file
#define fputs(s, stream) file_puts(s, (lfs_file_t *)stream) // Write a string to a file
#define ftell(stream) lfs_file_tell(CONFIG_LITTLEFS, (lfs_file_t *)stream) // Get the current position in a file
#define mkdir(path) lfs_mkdir(CONFIG_LITTLEFS, path) // Create a directory
#define rmdir(path) lfs_remove(CONFIG_LITTLEFS, path) // Remove a directory
#define opendir(path) lfs_dir_open(CONFIG_LITTLEFS, path) // Open a directory
#define closedir(dir) lfs_dir_close(CONFIG_LITTLEFS, dir) // Close a directory
#define readdir(dir, info) lfs_dir_read(CONFIG_LITTLEFS, dir, info) // Read a directory entry
#define dir_seek(dir, off) lfs_dir_seek(CONFIG_LITTLEFS, dir, off) // Seek in a directory
#define dir_tell(dir) lfs_dir_tell(CONFIG_LITTLEFS, dir) // Get the current position in a directory
#define dir_rewind(dir) lfs_dir_rewind(CONFIG_LITTLEFS, dir) // Rewind a directory to the beginning
#define file_size(file) lfs_file_size(CONFIG_LITTLEFS, (lfs_file_t *)file) // Get the size of a file


#ifdef __cplusplus
}
#endif


#endif // __FILE_IO_H