#include "file_system.h"
#include "w25q_reg.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MOCK_FLASH_SIZE (128U * 1024U)
#define MOCK_FLASH_DENSITY_ID (0x10U)
#define TEST_FILE_SYSTEM_OPEN_FILE_LIMIT (8U)
#define TEST_FILE_SYSTEM_OPEN_DIRECTORY_LIMIT (4U)

extern int _open(char *path, int flags, ...);
extern int _read(int fd, char *buffer, int length);
extern int _write(int fd, char *buffer, int length);
extern int _close(int fd);
extern int _lseek(int fd, int offset, int whence);
extern int _fstat(int fd, struct stat *status);
extern int _stat(const char *path, struct stat *status);
extern int _unlink(char *path);
extern int _mkdir(const char *path, mode_t mode);
extern int _rmdir(const char *path);
extern int _rename(const char *old_path, const char *new_path);

typedef struct
{
    uint8_t storage[MOCK_FLASH_SIZE];
    bool selected;
    bool locked;
    uint8_t status;
    uint32_t erase_count;
    uint32_t now_ms;
} mock_flash_t;

static mock_flash_t s_mock_flash;
static sx_spi_if_t s_spi;
static w25q_t s_w25q;

static uint32_t mock_decode_address(const uint8_t *buffer)
{
    return ((uint32_t)buffer[1] << 16U) |
           ((uint32_t)buffer[2] << 8U) |
           (uint32_t)buffer[3];
}

static sx_bus_status_t mock_spi_transfer(void *context,
                                         const uint8_t *tx,
                                         uint8_t *rx,
                                         uint32_t length,
                                         uint32_t timeout_ms)
{
    mock_flash_t *flash = context;
    uint8_t opcode;

    (void)timeout_ms;
    assert(flash != NULL);
    assert(flash->selected);
    assert(tx != NULL);
    assert(rx != NULL);
    assert(length > 0U);
    memset(rx, 0xFF, length);
    opcode = tx[0];

    switch (opcode) {
    case W25Q_CMD_READ_JEDEC_ID:
        assert(length == 4U);
        rx[1] = W25Q_MANUFACTURER_ID_WINBOND;
        rx[2] = 0x40U;
        rx[3] = MOCK_FLASH_DENSITY_ID;
        break;
    case W25Q_CMD_READ_STATUS_REGISTER_1:
        assert(length == 2U);
        rx[1] = flash->status;
        break;
    case W25Q_CMD_WRITE_ENABLE:
        flash->status |= W25Q_STATUS_WRITE_ENABLE_MASK;
        break;
    case W25Q_CMD_READ_DATA: {
        uint32_t address = mock_decode_address(tx);
        uint32_t data_length = length - 4U;
        assert(address <= MOCK_FLASH_SIZE);
        assert(data_length <= MOCK_FLASH_SIZE - address);
        memcpy(&rx[4], &flash->storage[address], data_length);
        break;
    }
    case W25Q_CMD_PAGE_PROGRAM: {
        uint32_t address = mock_decode_address(tx);
        uint32_t data_length = length - 4U;
        uint32_t index;
        assert((flash->status & W25Q_STATUS_WRITE_ENABLE_MASK) != 0U);
        assert(address <= MOCK_FLASH_SIZE);
        assert(data_length <= MOCK_FLASH_SIZE - address);
        for (index = 0U; index < data_length; index++) {
            flash->storage[address + index] &= tx[4U + index];
        }
        flash->status &= (uint8_t)~W25Q_STATUS_WRITE_ENABLE_MASK;
        break;
    }
    case W25Q_CMD_SECTOR_ERASE_4K: {
        uint32_t address = mock_decode_address(tx);
        assert((flash->status & W25Q_STATUS_WRITE_ENABLE_MASK) != 0U);
        assert(address + W25Q_SECTOR_SIZE_BYTES <= MOCK_FLASH_SIZE);
        assert((address % W25Q_SECTOR_SIZE_BYTES) == 0U);
        memset(&flash->storage[address], 0xFF, W25Q_SECTOR_SIZE_BYTES);
        flash->status &= (uint8_t)~W25Q_STATUS_WRITE_ENABLE_MASK;
        flash->erase_count++;
        break;
    }
    default:
        assert(false);
        return SX_BUS_STATUS_IO_ERROR;
    }
    return SX_BUS_STATUS_OK;
}

static sx_bus_status_t mock_chip_select(void *context, bool active)
{
    mock_flash_t *flash = context;

    assert(flash != NULL);
    assert(flash->selected != active);
    flash->selected = active;
    return SX_BUS_STATUS_OK;
}

static uint32_t mock_get_time_ms(void *context)
{
    mock_flash_t *flash = context;

    return flash->now_ms;
}

static void mock_delay_ms(void *context, uint32_t delay_ms)
{
    mock_flash_t *flash = context;

    flash->now_ms += delay_ms;
}

static bool mock_lock(void *context)
{
    mock_flash_t *flash = context;

    if (flash->locked) {
        return false;
    }
    flash->locked = true;
    return true;
}

static void mock_unlock(void *context)
{
    mock_flash_t *flash = context;

    assert(flash->locked);
    flash->locked = false;
}

static void test_initialize_flash(void)
{
    const w25q_config_t config = {
        .spi = &s_spi,
        .chip_select_context = &s_mock_flash,
        .chip_select = mock_chip_select,
        .time_context = &s_mock_flash,
        .get_time_ms = mock_get_time_ms,
        .delay_ms = mock_delay_ms,
        .transfer_timeout_ms = 100U,
    };

    memset(&s_mock_flash, 0, sizeof(s_mock_flash));
    memset(s_mock_flash.storage, 0xFF, sizeof(s_mock_flash.storage));
    s_spi.context = &s_mock_flash;
    s_spi.transfer = mock_spi_transfer;
    s_spi.transfer_async = NULL;

    assert(w25q_init(&s_w25q, &config, NULL) == W25Q_STATUS_OK);
    s_w25q.capacity_bytes = MOCK_FLASH_SIZE;
}

static file_system_config_t test_file_system_config(void)
{
    file_system_config_t config = {
        .flash = &s_w25q,
        .lock_context = &s_mock_flash,
        .lock = mock_lock,
        .unlock = mock_unlock,
    };

    return config;
}

static void test_posix_file_operations(void)
{
    static const char message[] = "littlefs on w25q";
    char buffer[sizeof(message)] = {0};
    struct stat status;
    file_system_config_t config = test_file_system_config();
    int32_t fd;

    assert(file_system_mount(&config) == -1);
    assert(s_mock_flash.erase_count == 0U);

    assert(file_system_format(&config) == 0);
    assert(file_system_mount(&config) == 0);

    fd = file_system_open("/hello.txt", O_CREAT | O_EXCL | O_RDWR, 0600U);
    assert(fd >= 3);
    assert(file_system_write(fd, message, sizeof(message)) ==
           (int32_t)sizeof(message));
    assert(file_system_lseek(fd, 0, SEEK_SET) == 0);
    assert(file_system_read(fd, buffer, sizeof(buffer)) ==
           (int32_t)sizeof(message));
    assert(memcmp(buffer, message, sizeof(message)) == 0);
    assert(file_system_fstat(fd, &status) == 0);
    assert(S_ISREG(status.st_mode));
    assert(status.st_size == (off_t)sizeof(message));
    assert(file_system_unmount() == -1);
    assert(errno == EBUSY);
    assert(file_system_sync(fd) == 0);
    assert(file_system_close(fd) == 0);

    {
        uint32_t total_bytes = 0U;
        uint32_t used_bytes = 0U;

        assert(file_system_get_usage(&total_bytes, &used_bytes) == 0);
        assert(total_bytes == MOCK_FLASH_SIZE);
        assert(used_bytes > 0U);
        assert(used_bytes <= total_bytes);
        assert(file_system_get_usage(NULL, &used_bytes) == -1);
        assert(errno == EINVAL);
    }

    fd = file_system_open("/hello.txt", O_CREAT | O_EXCL | O_WRONLY, 0U);
    assert(fd == -1);
    assert(errno == EEXIST);
    fd = file_system_open("/hello.txt", O_RDONLY, 0U);
    assert(fd >= 3);
    assert(file_system_write(fd, message, 1U) == -1);
    assert(errno == EBADF);
    assert(file_system_close(fd) == 0);

    assert(file_system_stat("/hello.txt", &status) == 0);
    assert(S_ISREG(status.st_mode));
    assert(status.st_size == (off_t)sizeof(message));
    assert(file_system_mkdir("/data") == 0);
    assert(file_system_stat("/data", &status) == 0);
    assert(S_ISDIR(status.st_mode));
    assert(file_system_unlink("/data") == -1);
    assert(errno == EISDIR);
    assert(file_system_rename("/hello.txt", "/data/renamed.txt") == 0);
    assert(file_system_unlink("/data/renamed.txt") == 0);
    assert(file_system_rmdir("/data") == 0);
    assert(file_system_close(-1) == -1);
    assert(file_system_unmount() == 0);
}

static void test_posix_open_flags_and_descriptor_limit(void)
{
    static const char original[] = "original";
    static const char suffix[] = "-append";
    static const char expected[] = "original-append";
    char buffer[sizeof(expected)] = {0};
    int32_t descriptors[TEST_FILE_SYSTEM_OPEN_FILE_LIMIT];
    struct stat status;
    file_system_config_t config = test_file_system_config();
    int32_t fd;
    uint32_t index;

    assert(file_system_mount(&config) == 0);
    fd = file_system_open("/flags.txt", O_CREAT | O_TRUNC | O_WRONLY, 0600U);
    assert(fd >= 3);
    assert(file_system_write(fd, original, sizeof(original) - 1U) ==
           (int32_t)(sizeof(original) - 1U));
    assert(file_system_close(fd) == 0);

    fd = file_system_open("/flags.txt", O_WRONLY | O_APPEND, 0U);
    assert(fd >= 3);
    assert(file_system_lseek(fd, 0, SEEK_SET) == 0);
    assert(file_system_write(fd, suffix, sizeof(suffix) - 1U) ==
           (int32_t)(sizeof(suffix) - 1U));
    assert(file_system_close(fd) == 0);

    fd = file_system_open("/flags.txt", O_RDONLY | O_TRUNC, 0U);
    assert(fd == -1);
    assert(errno == EINVAL);
    fd = file_system_open("/flags.txt", O_RDONLY, 0U);
    assert(fd >= 3);
    assert(file_system_read(fd, buffer, sizeof(buffer)) ==
           (int32_t)(sizeof(expected) - 1U));
    assert(memcmp(buffer, expected, sizeof(expected) - 1U) == 0);
    assert(file_system_close(fd) == 0);

    fd = file_system_open("/flags.txt", O_WRONLY | O_TRUNC, 0U);
    assert(fd >= 3);
    assert(file_system_fstat(fd, &status) == 0);
    assert(status.st_size == 0);
    assert(file_system_close(fd) == 0);

    for (index = 0U;
         index < (uint32_t)(sizeof(descriptors) / sizeof(descriptors[0]));
         index++) {
        descriptors[index] = file_system_open("/flags.txt", O_RDONLY, 0U);
        assert(descriptors[index] >= 3);
    }
    assert(file_system_open("/flags.txt", O_RDONLY, 0U) == -1);
    assert(errno == EMFILE);
    for (index = 0U;
         index < (uint32_t)(sizeof(descriptors) / sizeof(descriptors[0]));
         index++) {
        assert(file_system_close((int)descriptors[index]) == 0);
    }

    assert(file_system_unlink("/flags.txt") == 0);
    assert(file_system_unmount() == 0);
}

static void test_stdio_style_file_operations(void)
{
    static const uint16_t values[] = {0x1234U, 0x5678U, 0x9ABCU};
    uint16_t readback[sizeof(values) / sizeof(values[0])] = {0U};
    file_system_config_t config = test_file_system_config();
    file_system_FILE *stream;

    assert(file_system_mount(&config) == 0);
    stream = file_system_fopen("/stdio.bin", "w+b");
    assert(stream != NULL);
    assert(file_system_fputs("header\n", stream) == 0);
    assert(file_system_file_size(stream) == 7);
    assert(file_system_fseek(stream, 0, SEEK_SET) == 0);
    {
        char text[7] = {0};
        assert(file_system_fread(text, sizeof(text), 1U, stream) == 1U);
        assert(memcmp(text, "header\n", sizeof(text)) == 0);
    }
    assert(file_system_fseek(stream, 0, SEEK_SET) == 0);
    assert(file_system_fwrite(values, sizeof(values[0]),
                              sizeof(values) / sizeof(values[0]), stream) ==
           sizeof(values) / sizeof(values[0]));
    assert(file_system_ftell(stream) == (int32_t)sizeof(values));
    assert(file_system_fflush(stream) == 0);
    assert(file_system_fseek(stream, 0, SEEK_SET) == 0);
    assert(file_system_fread(readback, sizeof(readback[0]),
                             sizeof(readback) / sizeof(readback[0]), stream) ==
           sizeof(readback) / sizeof(readback[0]));
    assert(memcmp(readback, values, sizeof(values)) == 0);
    assert(file_system_fread(readback, SIZE_MAX, 2U, stream) == 0U);
    assert(errno == EINVAL);
    assert(file_system_fclose(stream) == 0);

    stream = file_system_fopen("/stdio.bin", "r");
    assert(stream != NULL);
    assert(file_system_fwrite(values, sizeof(values[0]), 1U, stream) == 0U);
    assert(errno == EBADF);
    assert(file_system_fclose(stream) == 0);
    assert(file_system_fopen("/stdio.bin", "invalid") == NULL);
    assert(errno == EINVAL);
    assert(file_system_unlink("/stdio.bin") == 0);
    assert(file_system_unmount() == 0);
}

static void test_directory_operations(void)
{
    static const char content[] = "entry";
    file_system_DIR *directories[TEST_FILE_SYSTEM_OPEN_DIRECTORY_LIMIT];
    file_system_dirent_t first_entry;
    file_system_dirent_t second_entry;
    file_system_dirent_t repeated_entry;
    file_system_config_t config = test_file_system_config();
    file_system_FILE *stream;
    uint32_t index;
    int32_t position;
    bool found_file = false;
    bool found_directory = false;

    assert(file_system_mount(&config) == 0);
    assert(file_system_mkdir("/listing") == 0);
    assert(file_system_mkdir("/listing/subdir") == 0);
    stream = file_system_fopen("/listing/item.txt", "w");
    assert(stream != NULL);
    assert(file_system_fputs(content, stream) == 0);
    assert(file_system_fclose(stream) == 0);

    directories[0] = file_system_opendir("/listing");
    assert(directories[0] != NULL);
    assert(file_system_unmount() == -1);
    assert(errno == EBUSY);

    assert(file_system_readdir(directories[0], &first_entry) == 1);
    position = file_system_telldir(directories[0]);
    assert(position >= 0);
    assert(file_system_readdir(directories[0], &second_entry) == 1);
    assert(file_system_seekdir(directories[0], (uint32_t)position) == 0);
    assert(file_system_readdir(directories[0], &repeated_entry) == 1);
    assert(strcmp(second_entry.name, repeated_entry.name) == 0);
    assert(file_system_rewinddir(directories[0]) == 0);

    do {
        int32_t result = file_system_readdir(directories[0], &first_entry);
        assert(result >= 0);
        if (result == 0) {
            break;
        }
        if (strcmp(first_entry.name, "item.txt") == 0) {
            assert(!first_entry.is_directory);
            assert(first_entry.size == sizeof(content) - 1U);
            found_file = true;
        } else if (strcmp(first_entry.name, "subdir") == 0) {
            assert(first_entry.is_directory);
            found_directory = true;
        }
    } while (true);
    assert(found_file);
    assert(found_directory);

    for (index = 1U; index < TEST_FILE_SYSTEM_OPEN_DIRECTORY_LIMIT; index++) {
        directories[index] = file_system_opendir("/listing");
        assert(directories[index] != NULL);
    }
    assert(file_system_opendir("/listing") == NULL);
    assert(errno == EMFILE);
    for (index = 0U; index < TEST_FILE_SYSTEM_OPEN_DIRECTORY_LIMIT; index++) {
        assert(file_system_closedir(directories[index]) == 0);
    }
    assert(file_system_closedir(directories[0]) == -1);
    assert(errno == EBADF);

    assert(file_system_remove("/listing/item.txt") == 0);
    assert(file_system_remove("/listing/subdir") == 0);
    assert(file_system_remove("/listing") == 0);
    assert(file_system_unmount() == 0);
}

static void test_newlib_syscall_bridge(void)
{
    static char data[] = "syscalls";
    char readback[sizeof(data)] = {0};
    struct stat status;
    int fd;

    assert(file_system_mount(&(file_system_config_t){
               .flash = &s_w25q,
               .lock_context = &s_mock_flash,
               .lock = mock_lock,
               .unlock = mock_unlock,
           }) == 0);
    fd = _open("/syscall.txt", O_CREAT | O_RDWR, 0600);
    assert(fd >= 3);
    assert(_write(fd, data, (int)sizeof(data)) == (int)sizeof(data));
    assert(_lseek(fd, 0, SEEK_SET) == 0);
    assert(_read(fd, readback, (int)sizeof(readback)) ==
           (int)sizeof(readback));
    assert(memcmp(readback, data, sizeof(data)) == 0);
    assert(_fstat(fd, &status) == 0);
    assert(S_ISREG(status.st_mode));
    assert(_close(fd) == 0);
    assert(_stat("/syscall.txt", &status) == 0);
    assert(_mkdir("/syscall-dir", 0700U) == 0);
    assert(_rename("/syscall.txt", "/syscall-dir/renamed.txt") == 0);
    assert(_rmdir("/syscall-dir") == -1);
    assert(errno == ENOTEMPTY);
    assert(_unlink("/syscall-dir/renamed.txt") == 0);
    assert(_rmdir("/syscall-dir") == 0);
    assert(file_system_unmount() == 0);
}

int main(void)
{
    test_initialize_flash();
    test_posix_file_operations();
    test_posix_open_flags_and_descriptor_limit();
    test_stdio_style_file_operations();
    test_directory_operations();
    test_newlib_syscall_bridge();
    return 0;
}
