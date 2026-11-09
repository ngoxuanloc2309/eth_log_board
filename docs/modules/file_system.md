# POSIX File System Module

## Purpose

Provide a bounded POSIX-style file API backed by LittleFS on an initialized
Winbond W25Q SPI flash device. The module also supplies Newlib syscall hooks for file I/O, metadata,
`unlink`, `mkdir`, `rmdir`, and `rename`.

## Responsibilities

- Adapt LittleFS block operations to the W25Q read, page-program, and 4 KiB
  sector-erase APIs.
- Mount and unmount one flash volume; formatting is a separate explicit
  operation and is never performed automatically.
- Manage a fixed pool of eight file descriptors and static LittleFS caches.
- Translate LittleFS errors to POSIX `errno` values.
- Serialize operations through caller-provided lock and unlock callbacks.

## Public API

- `file_system_port_init()` — initialize the board W25Q device and mount the
  existing volume from task context.
- `file_system_port_is_mounted()` — read the current mount state without
  initializing, mounting, or formatting storage.
- `file_system_port_format()` — explicitly erase/reformat board flash; it is
  destructive and is never called automatically.
- `file_system_mount()` / `file_system_unmount()`
- `file_system_format()` — destructive; explicitly erases/reformats the volume
- `file_system_open()`, `file_system_read()`, `file_system_write()`,
  `file_system_lseek()`, `file_system_sync()`, `file_system_close()`
- `file_system_stat()`, `file_system_fstat()`, `file_system_unlink()`
- `file_system_get_usage()` — logical capacity and LittleFS allocated block
  usage
- `file_system_mkdir()`, `file_system_rmdir()`, `file_system_rename()`
- `file_system_fopen()`, `file_system_fclose()`, `file_system_fread()`,
  `file_system_fwrite()`, `file_system_fseek()`, `file_system_ftell()`,
  `file_system_fflush()`, `file_system_fputs()`,
  `file_system_file_size()`
- `file_system_remove()` and bounded directory iteration with
  `file_system_opendir()`, `file_system_readdir()`,
  `file_system_seekdir()`, `file_system_telldir()`,
  `file_system_rewinddir()`, and `file_system_closedir()`

The descriptor pool is fixed at eight entries and descriptors start at 3.
LittleFS does not store POSIX ownership, permissions, timestamps, links, or
directory-stream state; file mode is accepted but ignored. The stdio-style API
uses an opaque `file_system_FILE` handle and supports `r`, `w`, and `a` modes
with optional `+` and `b` modifiers; it does not replace the C library's
`FILE` API.
Directory iteration uses a fixed pool of four opaque `file_system_DIR`
handles. `file_system_readdir()` returns 1 for an entry, 0 at end of
directory, and -1 on error; it copies the entry name, type, and size into the
caller-provided `file_system_dirent_t`.
`file_system_get_usage()` returns the logical volume size and the number of
LittleFS blocks currently allocated, in bytes. Allocated usage is block-granular
and is not a sum of file lengths.

Example:

```c
file_system_FILE *file = file_system_fopen("/logs/boot.txt", "a");
if (file != NULL) {
    static const char message[] = "boot complete\n";
    if (file_system_fwrite(message, 1U, sizeof(message) - 1U, file) !=
        sizeof(message) - 1U) {
        /* Handle the write error. */
    }
    if (file_system_fclose(file) != 0) {
        /* Handle the close error. */
    }
}
```

Create the parent directory with `file_system_mkdir("/logs")` before opening
the file; parent directories are not created automatically.

## Dependencies

- `littlefs` (`libs/fs`)
- `w25q_driver`
- An initialized `w25q_t` device.
- A non-recursive lock/unlock pair suitable for serializing all filesystem
  operations across tasks.

## Data Flow

```text
POSIX calls / Newlib syscalls
        ↓
file_system module (descriptor pool, error mapping, LittleFS config)
        ↓
LittleFS block callbacks
        ↓
w25q_driver
        ↓
sx_spi_if + chip-select callbacks
```

## Initialization

The FreeRTOS startup task calls `file_system_port_init()`. The port initializes
the board W25Q device on SPI1, creates a static mutex, and mounts the existing
LittleFS volume. If the device is blank or the volume is invalid, initialization
fails and reports `errno`; formatting is never automatic. To format
intentionally, call `file_system_port_format()` from task context, then call
`file_system_port_init()` to mount the newly formatted volume.

The board port maps W25Q chip select to active-low PD6 and uses the SPI1 bus
adapter. Flash ready polling delays in task context using FreeRTOS.

From the serial shell, `fs-test` runs an on-demand board smoke test: it creates
an exclusive temporary file, writes and syncs data, reads it back for
comparison, closes the file, and removes it. It refuses to overwrite an
existing `/.filesystem-smoke-test` file.

## Host tests

The POSIX API and Newlib syscall adapters are tested against a mock W25Q device:

```sh
cmake -S applications/file_system/tests -B build/file-system-tests
cmake --build build/file-system-tests
ctest --test-dir build/file-system-tests --output-on-failure
```

## Thread Safety

All mounted-volume operations acquire the provided lock once and release it
before returning. The callbacks must not recursively call filesystem APIs.
Mount, format, and unmount must be coordinated with users so they do not race
file operations. The LittleFS instance and descriptor pool are singleton
resources.
