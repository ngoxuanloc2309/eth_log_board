#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>

#include "w25q.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*file_system_lock_fn)(void *context);
typedef void (*file_system_unlock_fn)(void *context);

typedef struct file_system_file file_system_FILE;
typedef struct file_system_directory file_system_DIR;

#define FILE_SYSTEM_NAME_MAX (255U)

typedef struct
{
    char name[FILE_SYSTEM_NAME_MAX + 1U];
    bool is_directory;
    uint32_t size;
} file_system_dirent_t;

typedef struct
{
    w25q_t *flash;
    void *lock_context;
    file_system_lock_fn lock;
    file_system_unlock_fn unlock;
} file_system_config_t;

/**
 * @brief Mount the existing LittleFS volume on an initialized W25Q device.
 *
 * A lock and unlock callback are required. The callbacks serialize all file
 * system calls and must not be called recursively. A failed mount never
 * formats or erases the flash.
 *
 * @param config Flash device and task-serialization callbacks.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_mount(const file_system_config_t *config);

/**
 * @brief Explicitly format the W25Q device as LittleFS.
 *
 * This operation erases filesystem contents and must only be called when
 * formatting is intentional. It leaves the volume unmounted.
 *
 * @param config Flash device and task-serialization callbacks.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_format(const file_system_config_t *config);

/**
 * @brief Unmount the volume.
 *
 * The operation fails with EBUSY while any file descriptor is open.
 *
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_unmount(void);

/**
 * @brief Open a file using a stdio-style mode string.
 *
 * Supported modes are "r", "w", or "a", optionally followed by "+" and/or
 * "b". Mode "w" creates or truncates, and mode "a" creates or appends.
 *
 * @param path Absolute or root-relative LittleFS path.
 * @param mode File access mode.
 * @return Stream handle on success; NULL on failure with errno set.
 */
file_system_FILE *file_system_fopen(const char *path, const char *mode);

/**
 * @brief Close a stdio-style stream.
 *
 * @param stream Stream returned by file_system_fopen().
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_fclose(file_system_FILE *stream);

/**
 * @brief Read complete objects from a stdio-style stream.
 *
 * @param buffer Destination buffer.
 * @param size Size of each object in bytes.
 * @param count Maximum number of objects to read.
 * @param stream Stream returned by file_system_fopen().
 * @return Number of complete objects read.
 */
size_t file_system_fread(void *buffer,
                         size_t size,
                         size_t count,
                         file_system_FILE *stream);

/**
 * @brief Write objects to a stdio-style stream.
 *
 * @param buffer Source buffer.
 * @param size Size of each object in bytes.
 * @param count Number of objects to write.
 * @param stream Stream returned by file_system_fopen().
 * @return Number of complete objects written.
 */
size_t file_system_fwrite(const void *buffer,
                          size_t size,
                          size_t count,
                          file_system_FILE *stream);

/**
 * @brief Set the position of a stdio-style stream.
 *
 * @param stream Stream returned by file_system_fopen().
 * @param offset Seek offset.
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_fseek(file_system_FILE *stream,
                          int32_t offset,
                          int whence);

/**
 * @brief Get the current position of a stdio-style stream.
 *
 * @param stream Stream returned by file_system_fopen().
 * @return Current position, or -1 on failure with errno set.
 */
int32_t file_system_ftell(file_system_FILE *stream);

/**
 * @brief Flush pending changes from a stdio-style stream.
 *
 * @param stream Stream returned by file_system_fopen().
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_fflush(file_system_FILE *stream);

/**
 * @brief Write a null-terminated string to a stream.
 *
 * The terminating null byte is not written.
 *
 * @param string String to write.
 * @param stream Stream returned by file_system_fopen().
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_fputs(const char *string, file_system_FILE *stream);

/**
 * @brief Get the size of an open file stream.
 *
 * @param stream Stream returned by file_system_fopen().
 * @return File size in bytes, or -1 on failure with errno set.
 */
int32_t file_system_file_size(file_system_FILE *stream);

/**
 * @brief Open a file using POSIX open flags.
 *
 * Supported flags are O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_EXCL, O_TRUNC,
 * O_APPEND, and O_CLOEXEC where available. The mode is accepted for POSIX
 * compatibility but LittleFS does not store permissions.
 *
 * @param path Absolute or root-relative LittleFS path.
 * @param flags POSIX open flags.
 * @param mode POSIX create mode; ignored by LittleFS.
 * @return Nonnegative file descriptor on success; -1 on failure with errno.
 */
int32_t file_system_open(const char *path, int flags, uint32_t mode);

/**
 * @brief Close an open file descriptor.
 *
 * @param fd File descriptor returned by file_system_open().
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_close(int fd);

/**
 * @brief Read bytes from an open file.
 *
 * @param fd File descriptor.
 * @param buffer Destination buffer.
 * @param length Maximum bytes to read.
 * @return Bytes read, 0 at end of file, or -1 on failure with errno set.
 */
int32_t file_system_read(int fd, void *buffer, uint32_t length);

/**
 * @brief Write bytes to an open file.
 *
 * @param fd File descriptor.
 * @param buffer Source buffer.
 * @param length Number of bytes to write.
 * @return Bytes written or -1 on failure with errno set.
 */
int32_t file_system_write(int fd, const void *buffer, uint32_t length);

/**
 * @brief Seek within an open file.
 *
 * @param fd File descriptor.
 * @param offset Seek offset.
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END.
 * @return New file position or -1 on failure with errno set.
 */
int32_t file_system_lseek(int fd, int32_t offset, int whence);

/**
 * @brief Flush pending file changes to the flash device.
 *
 * @param fd File descriptor.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_sync(int fd);

/**
 * @brief Get metadata for an open file descriptor.
 *
 * @param fd File descriptor.
 * @param status Destination POSIX stat structure.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_fstat(int fd, struct stat *status);

/**
 * @brief Get metadata for a path.
 *
 * @param path Absolute or root-relative LittleFS path.
 * @param status Destination POSIX stat structure.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_stat(const char *path, struct stat *status);

/**
 * @brief Get logical filesystem capacity and allocated space.
 *
 * @param total_bytes Destination for total logical volume capacity.
 * @param used_bytes Destination for allocated LittleFS block capacity.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_get_usage(uint32_t *total_bytes, uint32_t *used_bytes);

/**
 * @brief Remove a file.
 *
 * @param path File path.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_unlink(const char *path);

/**
 * @brief Create a directory.
 *
 * @param path Directory path.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_mkdir(const char *path);

/**
 * @brief Remove an empty directory.
 *
 * @param path Directory path.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_rmdir(const char *path);

/**
 * @brief Rename or move a file or directory.
 *
 * @param old_path Existing path.
 * @param new_path Destination path.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_rename(const char *old_path, const char *new_path);

/**
 * @brief Remove a file or an empty directory.
 *
 * @param path Path to remove.
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_remove(const char *path);

/**
 * @brief Open a directory for bounded entry enumeration.
 *
 * @param path Directory path.
 * @return Directory handle on success; NULL on failure with errno set.
 */
file_system_DIR *file_system_opendir(const char *path);

/**
 * @brief Close an open directory handle.
 *
 * @param directory Handle returned by file_system_opendir().
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_closedir(file_system_DIR *directory);

/**
 * @brief Read the next entry from a directory.
 *
 * @param directory Handle returned by file_system_opendir().
 * @param entry Destination entry.
 * @return 1 when an entry is read, 0 at end of directory, or -1 on failure.
 */
int32_t file_system_readdir(file_system_DIR *directory,
                            file_system_dirent_t *entry);

/**
 * @brief Seek to a previously returned directory position.
 *
 * @param directory Handle returned by file_system_opendir().
 * @param offset Position returned by file_system_telldir().
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_seekdir(file_system_DIR *directory, uint32_t offset);

/**
 * @brief Get the current directory position.
 *
 * @param directory Handle returned by file_system_opendir().
 * @return Current position, or -1 on failure with errno set.
 */
int32_t file_system_telldir(file_system_DIR *directory);

/**
 * @brief Rewind a directory stream to its beginning.
 *
 * @param directory Handle returned by file_system_opendir().
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_rewinddir(file_system_DIR *directory);

#ifdef __cplusplus
}
#endif

#endif
