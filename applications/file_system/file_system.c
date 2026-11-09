#include "file_system.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "lfs.h"

extern int __io_putchar(int ch) __attribute__((weak));
extern int __io_getchar(void) __attribute__((weak));

#define FILE_SYSTEM_MAX_OPEN_FILES (8U)
#define FILE_SYSTEM_MAX_OPEN_DIRECTORIES (4U)
#define FILE_SYSTEM_FIRST_FD (3)
#define FILE_SYSTEM_CACHE_SIZE (256U)
#define FILE_SYSTEM_LOOKAHEAD_SIZE (16U)
#define FILE_SYSTEM_BLOCK_CYCLES (500)
#define FILE_SYSTEM_PROGRAM_TIMEOUT_MS (100U)
#define FILE_SYSTEM_ERASE_TIMEOUT_MS (1000U)
#define FILE_SYSTEM_SECTOR_SIZE W25Q_SECTOR_SIZE_BYTES
#define FILE_SYSTEM_STAT_BLOCK_SIZE (512U)

struct file_system_file
{
    lfs_file_t file;
    struct lfs_file_config file_config;
    uint8_t cache[FILE_SYSTEM_CACHE_SIZE];
    bool open;
    bool readable;
    bool writable;
};

typedef struct file_system_file file_system_file_t;

struct file_system_directory
{
    lfs_dir_t directory;
    bool open;
};

typedef struct file_system_directory file_system_directory_t;

typedef struct
{
    lfs_t lfs;
    struct lfs_config lfs_config;
    w25q_t *flash;
    void *lock_context;
    file_system_lock_fn lock;
    file_system_unlock_fn unlock;
    uint8_t read_cache[FILE_SYSTEM_CACHE_SIZE];
    uint8_t program_cache[FILE_SYSTEM_CACHE_SIZE];
    uint8_t lookahead[FILE_SYSTEM_LOOKAHEAD_SIZE];
    file_system_file_t files[FILE_SYSTEM_MAX_OPEN_FILES];
    file_system_directory_t directories[FILE_SYSTEM_MAX_OPEN_DIRECTORIES];
    bool configured;
    bool mounted;
} file_system_state_t;

static file_system_state_t s_file_system;

static int file_system_lfs_error_to_errno(int error)
{
    switch (error) {
    case LFS_ERR_OK:
        return 0;
    case LFS_ERR_IO:
    case LFS_ERR_CORRUPT:
        return EIO;
    case LFS_ERR_NOENT:
        return ENOENT;
    case LFS_ERR_EXIST:
        return EEXIST;
    case LFS_ERR_NOTDIR:
        return ENOTDIR;
    case LFS_ERR_ISDIR:
        return EISDIR;
    case LFS_ERR_NOTEMPTY:
        return ENOTEMPTY;
    case LFS_ERR_BADF:
        return EBADF;
    case LFS_ERR_FBIG:
        return EFBIG;
    case LFS_ERR_INVAL:
        return EINVAL;
    case LFS_ERR_NOSPC:
        return ENOSPC;
    case LFS_ERR_NOMEM:
        return ENOMEM;
    case LFS_ERR_NAMETOOLONG:
        return ENAMETOOLONG;
    default:
        return EIO;
    }
}

static int file_system_finish_lfs_error(int error)
{
    if (error < 0) {
        errno = file_system_lfs_error_to_errno(error);
        return -1;
    }
    return error;
}

static bool file_system_lock_active(void)
{
    if (!s_file_system.mounted) {
        errno = ENODEV;
        return false;
    }
    if (!s_file_system.lock(s_file_system.lock_context)) {
        errno = EBUSY;
        return false;
    }
    return true;
}

static void file_system_unlock_active(void)
{
    s_file_system.unlock(s_file_system.lock_context);
}

static int file_system_flash_read(const struct lfs_config *config,
                                  lfs_block_t block,
                                  lfs_off_t offset,
                                  void *buffer,
                                  lfs_size_t size)
{
    file_system_state_t *state = config->context;
    uint32_t address;

    if (state == NULL || state->flash == NULL || buffer == NULL ||
        block >= config->block_count || offset > config->block_size ||
        size > config->block_size - offset) {
        return LFS_ERR_INVAL;
    }

    address = block * config->block_size + offset;
    return w25q_read(state->flash, address, buffer, size) == W25Q_STATUS_OK
               ? 0
               : LFS_ERR_IO;
}

static int file_system_flash_program(const struct lfs_config *config,
                                     lfs_block_t block,
                                     lfs_off_t offset,
                                     const void *buffer,
                                     lfs_size_t size)
{
    file_system_state_t *state = config->context;
    uint32_t address;

    if (state == NULL || state->flash == NULL || buffer == NULL ||
        block >= config->block_count || offset > config->block_size ||
        size > config->block_size - offset) {
        return LFS_ERR_INVAL;
    }

    address = block * config->block_size + offset;
    return w25q_page_program(state->flash, address, buffer, size,
                             FILE_SYSTEM_PROGRAM_TIMEOUT_MS) ==
                   W25Q_STATUS_OK
               ? 0
               : LFS_ERR_IO;
}

static int file_system_flash_erase(const struct lfs_config *config,
                                   lfs_block_t block)
{
    file_system_state_t *state = config->context;
    uint32_t address;

    if (state == NULL || state->flash == NULL ||
        block >= config->block_count) {
        return LFS_ERR_INVAL;
    }

    address = block * config->block_size;
    return w25q_erase_sector_4k(state->flash, address,
                                FILE_SYSTEM_ERASE_TIMEOUT_MS) ==
                   W25Q_STATUS_OK
               ? 0
               : LFS_ERR_IO;
}

static int file_system_flash_sync(const struct lfs_config *config)
{
    file_system_state_t *state = config->context;

    return state != NULL && state->flash != NULL &&
                   state->flash->initialized
               ? 0
               : LFS_ERR_IO;
}

static bool file_system_config_valid(const file_system_config_t *config)
{
    return config != NULL && config->flash != NULL &&
           config->flash->initialized && config->flash->capacity_bytes >=
                                             (2U * FILE_SYSTEM_SECTOR_SIZE) &&
           config->lock != NULL && config->unlock != NULL;
}

static void file_system_configure(const file_system_config_t *config)
{
    memset(&s_file_system, 0, sizeof(s_file_system));
    s_file_system.flash = config->flash;
    s_file_system.lock_context = config->lock_context;
    s_file_system.lock = config->lock;
    s_file_system.unlock = config->unlock;

    s_file_system.lfs_config.context = &s_file_system;
    s_file_system.lfs_config.read = file_system_flash_read;
    s_file_system.lfs_config.prog = file_system_flash_program;
    s_file_system.lfs_config.erase = file_system_flash_erase;
    s_file_system.lfs_config.sync = file_system_flash_sync;
    s_file_system.lfs_config.read_size = 1U;
    s_file_system.lfs_config.prog_size = 1U;
    s_file_system.lfs_config.block_size = FILE_SYSTEM_SECTOR_SIZE;
    s_file_system.lfs_config.block_count =
        config->flash->capacity_bytes / FILE_SYSTEM_SECTOR_SIZE;
    s_file_system.lfs_config.block_cycles = FILE_SYSTEM_BLOCK_CYCLES;
    s_file_system.lfs_config.cache_size = FILE_SYSTEM_CACHE_SIZE;
    s_file_system.lfs_config.lookahead_size = FILE_SYSTEM_LOOKAHEAD_SIZE;
    s_file_system.lfs_config.read_buffer = s_file_system.read_cache;
    s_file_system.lfs_config.prog_buffer = s_file_system.program_cache;
    s_file_system.lfs_config.lookahead_buffer = s_file_system.lookahead;
    s_file_system.configured = true;
}

static bool file_system_any_handle_open(void)
{
    uint32_t index;

    for (index = 0U; index < FILE_SYSTEM_MAX_OPEN_FILES; index++) {
        if (s_file_system.files[index].open) {
            return true;
        }
    }
    for (index = 0U; index < FILE_SYSTEM_MAX_OPEN_DIRECTORIES; index++) {
        if (s_file_system.directories[index].open) {
            return true;
        }
    }
    return false;
}

static file_system_file_t *file_system_get_file(int fd)
{
    uint32_t index;

    if (fd < FILE_SYSTEM_FIRST_FD ||
        fd >= FILE_SYSTEM_FIRST_FD + (int)FILE_SYSTEM_MAX_OPEN_FILES) {
        return NULL;
    }
    index = (uint32_t)(fd - FILE_SYSTEM_FIRST_FD);
    if (!s_file_system.files[index].open) {
        return NULL;
    }
    return &s_file_system.files[index];
}

static int file_system_stream_to_fd(const file_system_FILE *stream)
{
    uint32_t index;

    if (stream == NULL) {
        errno = EBADF;
        return -1;
    }
    for (index = 0U; index < FILE_SYSTEM_MAX_OPEN_FILES; index++) {
        if (stream == &s_file_system.files[index]) {
            return (int)(index + FILE_SYSTEM_FIRST_FD);
        }
    }
    errno = EBADF;
    return -1;
}

static file_system_directory_t *file_system_get_directory(
    const file_system_DIR *directory)
{
    uint32_t index;

    if (directory == NULL) {
        return NULL;
    }
    for (index = 0U; index < FILE_SYSTEM_MAX_OPEN_DIRECTORIES; index++) {
        if (directory == &s_file_system.directories[index] &&
            s_file_system.directories[index].open) {
            return &s_file_system.directories[index];
        }
    }
    return NULL;
}

static bool file_system_parse_mode(const char *mode, int *flags)
{
    bool plus = false;
    bool binary = false;

    if (mode == NULL || mode[0] == '\0') {
        return false;
    }

    switch (mode[0]) {
    case 'r':
        *flags = O_RDONLY;
        break;
    case 'w':
        *flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
    case 'a':
        *flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
    default:
        return false;
    }

    for (mode++; *mode != '\0'; mode++) {
        if (*mode == '+' && !plus) {
            plus = true;
        } else if (*mode == 'b' && !binary) {
            binary = true;
        } else {
            return false;
        }
    }

    if (plus) {
        *flags &= ~O_ACCMODE;
        *flags |= O_RDWR;
    }
    return true;
}

static int file_system_fill_stat(const struct lfs_info *info,
                                struct stat *status)
{
    if (info->type == LFS_TYPE_DIR) {
        status->st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR |
                          S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
        status->st_size = 0;
    } else if (info->type == LFS_TYPE_REG) {
        status->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        status->st_size = (off_t)info->size;
    } else {
        errno = EIO;
        return -1;
    }

    status->st_nlink = 1;
    status->st_blksize = FILE_SYSTEM_SECTOR_SIZE;
    status->st_blocks =
        (info->size + FILE_SYSTEM_STAT_BLOCK_SIZE - 1U) /
        FILE_SYSTEM_STAT_BLOCK_SIZE;
    return 0;
}

int32_t file_system_mount(const file_system_config_t *config)
{
    int result;

    if (!file_system_config_valid(config)) {
        errno = EINVAL;
        return -1;
    }
    if (!config->lock(config->lock_context)) {
        errno = EBUSY;
        return -1;
    }
    if (s_file_system.mounted) {
        config->unlock(config->lock_context);
        errno = EBUSY;
        return -1;
    }

    file_system_configure(config);
    result = lfs_mount(&s_file_system.lfs, &s_file_system.lfs_config);
    if (result == 0) {
        s_file_system.mounted = true;
    }
    config->unlock(config->lock_context);
    return file_system_finish_lfs_error(result);
}

int32_t file_system_format(const file_system_config_t *config)
{
    int result;

    if (!file_system_config_valid(config)) {
        errno = EINVAL;
        return -1;
    }
    if (!config->lock(config->lock_context)) {
        errno = EBUSY;
        return -1;
    }
    if (s_file_system.mounted || file_system_any_handle_open()) {
        config->unlock(config->lock_context);
        errno = EBUSY;
        return -1;
    }

    file_system_configure(config);
    result = lfs_format(&s_file_system.lfs, &s_file_system.lfs_config);
    config->unlock(config->lock_context);
    return file_system_finish_lfs_error(result);
}

int32_t file_system_unmount(void)
{
    int result;

    if (!file_system_lock_active()) {
        return -1;
    }
    if (file_system_any_handle_open()) {
        file_system_unlock_active();
        errno = EBUSY;
        return -1;
    }

    result = lfs_unmount(&s_file_system.lfs);
    if (result == 0) {
        s_file_system.mounted = false;
    }
    file_system_unlock_active();
    return file_system_finish_lfs_error(result);
}

file_system_FILE *file_system_fopen(const char *path, const char *mode)
{
    int flags;
    int32_t fd;

    if (path == NULL ||
        !file_system_parse_mode(mode, &flags)) {
        errno = EINVAL;
        return NULL;
    }
    fd = file_system_open(path, flags, 0U);
    if (fd < 0) {
        return NULL;
    }
    return &s_file_system.files[(uint32_t)(fd - FILE_SYSTEM_FIRST_FD)];
}

int32_t file_system_fclose(file_system_FILE *stream)
{
    int fd = file_system_stream_to_fd(stream);

    return fd < 0 ? -1 : file_system_close(fd);
}

size_t file_system_fread(void *buffer,
                         size_t size,
                         size_t count,
                         file_system_FILE *stream)
{
    int fd;
    uint32_t bytes_to_read;
    int32_t result;

    if (size == 0U || count == 0U) {
        return 0U;
    }
    if (size > (size_t)INT32_MAX ||
        count > (size_t)INT32_MAX / size) {
        errno = EINVAL;
        return 0U;
    }
    fd = file_system_stream_to_fd(stream);
    if (fd < 0) {
        return 0U;
    }
    bytes_to_read = (uint32_t)(size * count);
    result = file_system_read(fd, buffer, bytes_to_read);
    return result < 0 ? 0U : (size_t)result / size;
}

size_t file_system_fwrite(const void *buffer,
                          size_t size,
                          size_t count,
                          file_system_FILE *stream)
{
    int fd;
    uint32_t bytes_to_write;
    int32_t result;

    if (size == 0U || count == 0U) {
        return 0U;
    }
    if (size > (size_t)INT32_MAX ||
        count > (size_t)INT32_MAX / size) {
        errno = EINVAL;
        return 0U;
    }
    fd = file_system_stream_to_fd(stream);
    if (fd < 0) {
        return 0U;
    }
    bytes_to_write = (uint32_t)(size * count);
    result = file_system_write(fd, buffer, bytes_to_write);
    return result < 0 ? 0U : (size_t)result / size;
}

int32_t file_system_fseek(file_system_FILE *stream,
                          int32_t offset,
                          int whence)
{
    int fd = file_system_stream_to_fd(stream);

    if (fd < 0) {
        return -1;
    }
    return file_system_lseek(fd, offset, whence) < 0 ? -1 : 0;
}

int32_t file_system_ftell(file_system_FILE *stream)
{
    int fd = file_system_stream_to_fd(stream);

    if (fd < 0) {
        return -1;
    }
    return file_system_lseek(fd, 0, SEEK_CUR);
}

int32_t file_system_fflush(file_system_FILE *stream)
{
    int fd = file_system_stream_to_fd(stream);

    return fd < 0 ? -1 : file_system_sync(fd);
}

int32_t file_system_fputs(const char *string, file_system_FILE *stream)
{
    size_t length;
    int fd;
    int32_t result;

    if (string == NULL) {
        errno = EINVAL;
        return -1;
    }
    fd = file_system_stream_to_fd(stream);
    if (fd < 0) {
        return -1;
    }
    length = strlen(string);
    if (length > (size_t)INT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    result = file_system_write(fd, string, (uint32_t)length);
    if (result < 0) {
        return -1;
    }
    if ((size_t)result != length) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int32_t file_system_file_size(file_system_FILE *stream)
{
    int fd = file_system_stream_to_fd(stream);
    file_system_file_t *file;
    lfs_soff_t result;

    if (fd < 0 || !file_system_lock_active()) {
        return -1;
    }
    file = file_system_get_file(fd);
    if (file == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }
    result = lfs_file_size(&s_file_system.lfs, &file->file);
    file_system_unlock_active();
    if (result < 0) {
        errno = file_system_lfs_error_to_errno((int)result);
        return -1;
    }
    return result;
}

int32_t file_system_open(const char *path, int flags, uint32_t mode)
{
    int lfs_flags = 0;
    int access_mode;
    int allowed_flags = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND;
    uint32_t index;
    file_system_file_t *file = NULL;
    int result;

    (void)mode;
#ifdef O_CLOEXEC
    allowed_flags |= O_CLOEXEC;
#endif
#ifdef O_BINARY
    allowed_flags |= O_BINARY;
#endif
    if (path == NULL || (flags & ~allowed_flags) != 0) {
        errno = EINVAL;
        return -1;
    }
    access_mode = flags & O_ACCMODE;
    if (access_mode == O_RDONLY) {
        lfs_flags = LFS_O_RDONLY;
    } else if (access_mode == O_WRONLY) {
        lfs_flags = LFS_O_WRONLY;
    } else if (access_mode == O_RDWR) {
        lfs_flags = LFS_O_RDWR;
    } else {
        errno = EINVAL;
        return -1;
    }
    if ((flags & O_TRUNC) != 0 && access_mode == O_RDONLY) {
        errno = EINVAL;
        return -1;
    }
    if ((flags & O_APPEND) != 0 && access_mode == O_RDONLY) {
        errno = EINVAL;
        return -1;
    }
    if ((flags & O_CREAT) != 0) {
        lfs_flags |= LFS_O_CREAT;
    }
    if ((flags & O_EXCL) != 0) {
        lfs_flags |= LFS_O_EXCL;
    }
    if ((flags & O_TRUNC) != 0) {
        lfs_flags |= LFS_O_TRUNC;
    }
    if ((flags & O_APPEND) != 0) {
        lfs_flags |= LFS_O_APPEND;
    }
    if (!file_system_lock_active()) {
        return -1;
    }

    for (index = 0U; index < FILE_SYSTEM_MAX_OPEN_FILES; index++) {
        if (!s_file_system.files[index].open) {
            file = &s_file_system.files[index];
            break;
        }
    }
    if (file == NULL) {
        file_system_unlock_active();
        errno = EMFILE;
        return -1;
    }

    memset(&file->file_config, 0, sizeof(file->file_config));
    file->file_config.buffer = file->cache;
    result = lfs_file_opencfg(&s_file_system.lfs, &file->file, path,
                              lfs_flags, &file->file_config);
    if (result == 0) {
        file->open = true;
        file->readable = access_mode != O_WRONLY;
        file->writable = access_mode != O_RDONLY;
    }
    file_system_unlock_active();
    if (result < 0) {
        errno = file_system_lfs_error_to_errno(result);
        return -1;
    }
    return (int32_t)(index + FILE_SYSTEM_FIRST_FD);
}

int32_t file_system_close(int fd)
{
    file_system_file_t *file;
    int result;

    if (!file_system_lock_active()) {
        return -1;
    }
    file = file_system_get_file(fd);
    if (file == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }

    result = lfs_file_close(&s_file_system.lfs, &file->file);
    memset(file, 0, sizeof(*file));
    file_system_unlock_active();
    return file_system_finish_lfs_error(result);
}

int32_t file_system_read(int fd, void *buffer, uint32_t length)
{
    file_system_file_t *file;
    lfs_ssize_t result;

    if (buffer == NULL && length > 0U) {
        errno = EINVAL;
        return -1;
    }
    if (length > INT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    file = file_system_get_file(fd);
    if (file == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }
    if (!file->readable) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }

    result = lfs_file_read(&s_file_system.lfs, &file->file, buffer, length);
    file_system_unlock_active();
    if (result < 0) {
        errno = file_system_lfs_error_to_errno((int)result);
        return -1;
    }
    return result;
}

int32_t file_system_write(int fd, const void *buffer, uint32_t length)
{
    file_system_file_t *file;
    lfs_ssize_t result;

    if (buffer == NULL && length > 0U) {
        errno = EINVAL;
        return -1;
    }
    if (length > INT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    file = file_system_get_file(fd);
    if (file == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }
    if (!file->writable) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }

    result = lfs_file_write(&s_file_system.lfs, &file->file, buffer, length);
    file_system_unlock_active();
    if (result < 0) {
        errno = file_system_lfs_error_to_errno((int)result);
        return -1;
    }
    return result;
}

int32_t file_system_lseek(int fd, int32_t offset, int whence)
{
    file_system_file_t *file;
    int lfs_whence;
    lfs_soff_t result;

    if (whence == SEEK_SET) {
        lfs_whence = LFS_SEEK_SET;
    } else if (whence == SEEK_CUR) {
        lfs_whence = LFS_SEEK_CUR;
    } else if (whence == SEEK_END) {
        lfs_whence = LFS_SEEK_END;
    } else {
        errno = EINVAL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    file = file_system_get_file(fd);
    if (file == NULL) {
        file_system_unlock_active();                                                         
        errno = EBADF;
        return -1;
    }

    result = lfs_file_seek(&s_file_system.lfs, &file->file, offset,
                           lfs_whence);
    file_system_unlock_active();
    if (result < 0) {
        errno = file_system_lfs_error_to_errno((int)result);
        return -1;
    }
    return result;
}

int32_t file_system_sync(int fd)
{
    file_system_file_t *file;
    int result;

    if (!file_system_lock_active()) {
        return -1;
    }
    file = file_system_get_file(fd);
    if (file == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }

    result = lfs_file_sync(&s_file_system.lfs, &file->file);
    file_system_unlock_active();
    return file_system_finish_lfs_error(result);
}

int32_t file_system_fstat(int fd, struct stat *status)
{
    file_system_file_t *file;
    lfs_soff_t size;
    struct lfs_info info = {0};

    if (status == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    file = file_system_get_file(fd);
    if (file == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }

    size = lfs_file_size(&s_file_system.lfs, &file->file);
    if (size < 0) {
        file_system_unlock_active();
        errno = file_system_lfs_error_to_errno((int)size);
        return -1;
    }
    info.type = LFS_TYPE_REG;
    info.size = (lfs_size_t)size;
    memset(status, 0, sizeof(*status));
    {
        int result = file_system_fill_stat(&info, status);
        file_system_unlock_active();
        return result;
    }
}

int32_t file_system_stat(const char *path, struct stat *status)
{
    struct lfs_info info;
    int result;

    if (path == NULL || status == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    result = lfs_stat(&s_file_system.lfs, path, &info);
    if (result == 0) {
        memset(status, 0, sizeof(*status));
        result = file_system_fill_stat(&info, status);
    } else {
        errno = file_system_lfs_error_to_errno(result);
        result = -1;
    }
    file_system_unlock_active();
    return result;
}

int32_t file_system_get_usage(uint32_t *total_bytes, uint32_t *used_bytes)
{
    lfs_ssize_t used_blocks;
    uint64_t total_capacity;
    uint64_t used_capacity;

    if (total_bytes == NULL || used_bytes == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }

    used_blocks = lfs_fs_size(&s_file_system.lfs);
    total_capacity =
        (uint64_t)s_file_system.lfs_config.block_size *
        (uint64_t)s_file_system.lfs_config.block_count;
    used_capacity = used_blocks < 0
                        ? 0U
                        : (uint64_t)used_blocks *
                              (uint64_t)s_file_system.lfs_config.block_size;
    if (used_blocks < 0) {
        file_system_unlock_active();
        return file_system_finish_lfs_error((int)used_blocks);
    }
    if (total_capacity > UINT32_MAX || used_capacity > total_capacity) {
        file_system_unlock_active();
        errno = EOVERFLOW;
        return -1;
    }

    *total_bytes = (uint32_t)total_capacity;
    *used_bytes = (uint32_t)used_capacity;
    file_system_unlock_active();
    return 0;
}

int32_t file_system_unlink(const char *path)
{
    struct lfs_info info;
    int result;

    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    result = lfs_stat(&s_file_system.lfs, path, &info);
    if (result == 0 && info.type == LFS_TYPE_DIR) {
        errno = EISDIR;
        result = -1;
    } else if (result == 0) {
        result = lfs_remove(&s_file_system.lfs, path);
        if (result < 0) {
            errno = file_system_lfs_error_to_errno(result);
            result = -1;
        }
    } else {
        errno = file_system_lfs_error_to_errno(result);
        result = -1;
    }
    file_system_unlock_active();
    return result;
}

int32_t file_system_mkdir(const char *path)
{
    int result;

    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    result = lfs_mkdir(&s_file_system.lfs, path);
    file_system_unlock_active();
    return file_system_finish_lfs_error(result);
}

int32_t file_system_rmdir(const char *path)
{
    struct lfs_info info;
    int result;

    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    result = lfs_stat(&s_file_system.lfs, path, &info);
    if (result < 0) {
        errno = file_system_lfs_error_to_errno(result);
        result = -1;
    } else if (info.type != LFS_TYPE_DIR) {
        errno = ENOTDIR;
        result = -1;
    } else {
        result = lfs_remove(&s_file_system.lfs, path);
        if (result < 0) {
            errno = file_system_lfs_error_to_errno(result);
        }
    }
    file_system_unlock_active();
    return result < 0 ? -1 : 0;
}

int32_t file_system_rename(const char *old_path, const char *new_path)
{
    int result;

    if (old_path == NULL || new_path == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    result = lfs_rename(&s_file_system.lfs, old_path, new_path);
    file_system_unlock_active();
    return file_system_finish_lfs_error(result);
}

int32_t file_system_remove(const char *path)
{
    int result;

    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    result = lfs_remove(&s_file_system.lfs, path);
    file_system_unlock_active();
    return file_system_finish_lfs_error(result);
}

file_system_DIR *file_system_opendir(const char *path)
{
    uint32_t index;
    file_system_directory_t *directory = NULL;
    int result;

    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (!file_system_lock_active()) {
        return NULL;
    }
    for (index = 0U; index < FILE_SYSTEM_MAX_OPEN_DIRECTORIES; index++) {
        if (!s_file_system.directories[index].open) {
            directory = &s_file_system.directories[index];
            break;
        }
    }
    if (directory == NULL) {
        file_system_unlock_active();
        errno = EMFILE;
        return NULL;
    }

    result = lfs_dir_open(&s_file_system.lfs, &directory->directory, path);
    if (result == 0) {
        directory->open = true;
    }
    file_system_unlock_active();
    if (result < 0) {
        errno = file_system_lfs_error_to_errno(result);
        return NULL;
    }
    return directory;
}

int32_t file_system_closedir(file_system_DIR *directory)
{
    file_system_directory_t *entry;
    int result;

    if (!file_system_lock_active()) {
        return -1;
    }
    entry = file_system_get_directory(directory);
    if (entry == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }
    result = lfs_dir_close(&s_file_system.lfs, &entry->directory);
    if (result == 0) {
        memset(entry, 0, sizeof(*entry));
    }
    file_system_unlock_active();
    return file_system_finish_lfs_error(result);
}

int32_t file_system_readdir(file_system_DIR *directory,
                            file_system_dirent_t *entry)
{
    file_system_directory_t *dir;
    struct lfs_info info;
    size_t name_length;
    int result;

    if (entry == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!file_system_lock_active()) {
        return -1;
    }
    dir = file_system_get_directory(directory);
    if (dir == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }
    result = lfs_dir_read(&s_file_system.lfs, &dir->directory, &info);
    if (result <= 0) {
        file_system_unlock_active();
        return file_system_finish_lfs_error(result);
    }

    name_length = strlen(info.name);
    if (name_length > FILE_SYSTEM_NAME_MAX) {
        file_system_unlock_active();
        errno = EIO;
        return -1;
    }
    memset(entry, 0, sizeof(*entry));
    memcpy(entry->name, info.name, name_length + 1U);
    if (info.type == LFS_TYPE_DIR) {
        entry->is_directory = true;
    } else if (info.type != LFS_TYPE_REG) {
        file_system_unlock_active();
        errno = EIO;
        return -1;
    }
    entry->size = info.size;
    file_system_unlock_active();
    return 1;
}

int32_t file_system_seekdir(file_system_DIR *directory, uint32_t offset)
{
    file_system_directory_t *entry;
    int result;

    if (!file_system_lock_active()) {
        return -1;
    }
    entry = file_system_get_directory(directory);
    if (entry == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }
    result = lfs_dir_seek(&s_file_system.lfs, &entry->directory, offset);
    file_system_unlock_active();
    return file_system_finish_lfs_error(result);
}

int32_t file_system_telldir(file_system_DIR *directory)
{
    file_system_directory_t *entry;
    lfs_soff_t result;

    if (!file_system_lock_active()) {
        return -1;
    }
    entry = file_system_get_directory(directory);
    if (entry == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }
    result = lfs_dir_tell(&s_file_system.lfs, &entry->directory);
    file_system_unlock_active();
    if (result < 0) {
        errno = file_system_lfs_error_to_errno((int)result);
        return -1;
    }
    return result;
}

int32_t file_system_rewinddir(file_system_DIR *directory)
{
    file_system_directory_t *entry;
    int result;

    if (!file_system_lock_active()) {
        return -1;
    }
    entry = file_system_get_directory(directory);
    if (entry == NULL) {
        file_system_unlock_active();
        errno = EBADF;
        return -1;
    }
    result = lfs_dir_rewind(&s_file_system.lfs, &entry->directory);
    file_system_unlock_active();
    return file_system_finish_lfs_error(result);
}

int _open(char *path, int flags, ...)
{
    return file_system_open(path, flags, 0U);
}

int _read(int fd, char *buffer, int length)
{
    int index;
    int received = 0;

    if (fd <= 2) {
        if (fd != 0 || buffer == NULL || length < 0 ||
            __io_getchar == NULL) {
            errno = EBADF;
            return -1;
        }
        for (index = 0; index < length; index++) {
            int value = __io_getchar();
            if (value < 0) {
                if (received == 0) {
                    errno = EIO;
                    return -1;
                }
                break;
            }
            buffer[received] = (char)value;
            received++;
        }
        return received;
    }
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    return file_system_read(fd, buffer, (uint32_t)length);
}

int _write(int fd, char *buffer, int length)
{
    int index;

    if (fd <= 2) {
        if ((fd != 1 && fd != 2) || buffer == NULL || length < 0 ||
            __io_putchar == NULL) {
            errno = EBADF;
            return -1;
        }
        for (index = 0; index < length; index++) {
            if (__io_putchar((unsigned char)buffer[index]) < 0) {
                if (index == 0) {
                    errno = EIO;
                    return -1;
                }
                break;
            }
        }
        return index;
    }
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    return file_system_write(fd, buffer, (uint32_t)length);
}

int _close(int fd)
{
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (fd <= 2) {
        return 0;
    }
    return file_system_close(fd);
}

int _fstat(int fd, struct stat *status)
{
    if (fd >= 0 && fd <= 2) {
        if (status == NULL) {
            errno = EINVAL;
            return -1;
        }
        status->st_mode = S_IFCHR;
        status->st_size = 0;
        return 0;
    }
    return file_system_fstat(fd, status);
}

int _isatty(int fd)
{
    return fd >= 0 && fd <= 2 ? 1 : 0;
}

int _lseek(int fd, int offset, int whence)
{
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (fd <= 2) {
        errno = ESPIPE;
        return -1;
    }
    return file_system_lseek(fd, offset, whence);
}

int _unlink(char *path)
{
    return file_system_unlink(path);
}

int _mkdir(const char *path, mode_t mode)
{
    (void)mode;
    return file_system_mkdir(path);
}

int _rmdir(const char *path)
{
    return file_system_rmdir(path);
}

int _rename(const char *old_path, const char *new_path)
{
    return file_system_rename(old_path, new_path);
}

int _stat(const char *path, struct stat *status)
{
    return file_system_stat(path, status);
}
