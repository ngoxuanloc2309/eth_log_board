#include "file_io.h"
#include "logger.h"

static const char *TAG = "FS";

lfs_t lfs;

int littlefs_flash_sync (const struct lfs_config * c);
int file_system_init(file_system_config_t *config){
    if (config == NULL) {
        return -1; // Invalid configuration
    }
    if(config->block_size == 0 || config->block_count == 0) {
        log_error(TAG, "Invalid block size or block count");
        return -2; // Invalid block size or count
    }
    // Initialize the LittleFS configuration
    config->cfg.context = NULL;
    config->cfg.read = config->file_function.read;
    config->cfg.prog = config->file_function.write;
    config->cfg.erase = config->file_function.erase;
    config->cfg.sync = littlefs_flash_sync;
    config->cfg.block_cycles = 100;
    config->cfg.cache_size = 16;
    config->cfg.lookahead_size = 16;
    config->cfg.read_size = 16;
    config->cfg.prog_size = 16;
    config->cfg.name_max = 24;
    config->cfg.block_count = config->block_count;
    config->cfg.block_size = config->block_size;
    config->cfg.context = config;

    config->lfs = &lfs;

    // Initialize the LittleFS filesystem
    log_info(TAG,"mount...");
    int lfs_status = lfs_mount(config->lfs, &config->cfg);
    if (lfs_status != LFS_ERR_OK) {
        log_info(TAG,"format...");
        if(LFS_ERR_OK != lfs_format(config->lfs, &config->cfg)){
            log_error(TAG, "Failed to format LittleFS");
            return -3; // Failed to format filesystem
        }
        log_info(TAG,"formated!");
        
        if(LFS_ERR_OK != lfs_mount(config->lfs, &config->cfg)){
            log_error(TAG, "Failed to mount LittleFS");
            return -4; // Failed to mount filesystem
        }
        lfs_mkdir(config->lfs, "/");   
    }
    log_info(TAG,"mounted!");
    log_info(TAG, "LittleFS formatted and mounted successfully");
    return LFS_ERR_OK; // Filesystem formatted and mounted successfully
}
void fs_list_dir(const char *path)
{
    lfs_dir_t dir;
    struct lfs_info info;
    int err;

    err = lfs_dir_open(CONFIG_LITTLEFS, &dir, path);
    if (err < 0)
    {
        log_error(TAG, "Failed to open dir %s, err=%d", path, err);
        return;
    }

    log_info(TAG, "Listing directory: %s", path);

    while (true)
    {
        err = lfs_dir_read(CONFIG_LITTLEFS, &dir, &info);
        if (err < 0)
        {
            log_error(TAG, "Read dir error %d", err);
            break;
        }

        if (err == 0)
        {
            // end of directory
            break;
        }

        if (info.type == LFS_TYPE_DIR)
        {
            log_info(TAG, "%s", info.name);
        }
        else if (info.type == LFS_TYPE_REG)
        {
            log_info(TAG, "%s", info.name, info.size);
        }
    }
    lfs_dir_close(CONFIG_LITTLEFS, &dir);
}
int littlefs_flash_sync(const struct lfs_config *c)
{
    file_system_config_t *config = (file_system_config_t *)c->context;
    if (config == NULL || config->lfs == NULL) {
        log_error(TAG, "Invalid file system configuration or LittleFS object");
        return -1; // Invalid configuration or LittleFS object
    }
    // Currently, LittleFS does not require a specific sync operation
    // as it handles synchronization internally.
    return LFS_ERR_OK; // Return success
}
