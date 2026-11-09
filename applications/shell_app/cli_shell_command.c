#include "cli_shell.h"
#include "file_system.h"
#include "file_system_port.h"
#include "rf_app.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define FILE_SYSTEM_TEST_PATH "/.filesystem-smoke-test"
#define FILE_SYSTEM_TEST_DATA "LittleFS board test\r\n"

#define ZIGBEE_SECURITY_CODE_SIZE (4U)
#define ZIGBEE_SECURITY_ENABLED_MAX (1U)

static int cli_shell_zigbee_handler(ShellContext_t *shell,
                                    int argc,
                                    char *argv[]);
static int cli_shell_upgrade(ShellContext_t *shell,
                                    int argc,
                                    char *argv[]);
static int cli_shell_filesystem_test(ShellContext_t *shell,
                                     int argc,
                                     char *argv[]);
static int cli_shell_sys_reboot(ShellContext_t *shell,
                                     int argc,
                                     char *argv[]);                                     
static void cli_shell_zigbee_connect_callback(ZigbeeMesh_t *mesh,
                                               uint8_t result,
                                               void *arg);
static void cli_shell_zigbee_read_callback(ZigbeeMesh_t *mesh,
                                            uint8_t result,
                                            void *arg);
static void cli_shell_zigbee_write_callback(ZigbeeMesh_t *mesh,
                                             uint8_t result,
                                             void *arg);
static void cli_shell_zigbee_reset_callback(ZigbeeMesh_t *mesh,
                                             uint8_t result,
                                             void *arg);
static void cli_shell_zigbee_sleep_callback(ZigbeeMesh_t *mesh,
                                             uint8_t result,
                                             void *arg);
static bool cli_shell_zigbee_parse_value(const char *text, uint32_t *value);
static bool cli_shell_zigbee_set_parameter(ZigbeeParameter_t *parameters,
                                           const char *option,
                                           const char *text);
static int cli_shell_zigbee_write(ShellContext_t *shell,
                                  int argc,
                                  char *argv[]);
static void cli_shell_zigbee_print_result(ShellContext_t *shell,
                                          const char *operation,
                                          uint8_t result);
static void cli_shell_zigbee_print_usage(ShellContext_t *shell);
static void cli_shell_zigbee_print_write_help(ShellContext_t *shell);

static const Cli_Shell_Cmd s_shell_commands[] = {
    {"help", cli_shell_help_handler, "Lists all commands"},
    {"zigbee",
     cli_shell_zigbee_handler,
     "Zigbee: -c connect, -r read, -w settings, -reset, -sleep"},
    {"fs-test", cli_shell_filesystem_test, "Run a non-destructive filesystem read/write test"},
    {"upgrade",cli_shell_upgrade,"Enter upgrade firmware mode"},
    {"reboot",cli_shell_sys_reboot,"Reboot device"}
};

const Cli_Shell_Cmd *const g_shell_commands = s_shell_commands;
const size_t g_num_shell_commands = ARRAY_SIZE(s_shell_commands);

static int cli_shell_filesystem_test(ShellContext_t *shell,
                                     int argc,
                                     char *argv[])
{
    static const uint8_t test_data[] = FILE_SYSTEM_TEST_DATA;
    uint8_t readback[sizeof(test_data) - 1U];
    int32_t fd = -1;
    int32_t result;
    int32_t expected_size = (int32_t)(sizeof(test_data) - 1U);
    int error_number = 0;
    int cleanup_error = 0;
    bool file_created = false;

    (void)argv;
    if (shell == NULL) {
        return -1;
    }
    if (argc != 1) {
        cli_shell_put_line(shell, "Usage: fs-test");
        return -1;
    }
    if (file_system_port_init() != 0) {
        cli_shell_printf(shell, "Filesystem mount failed: errno=%d\r\n",
                         errno);
        return -1;
    }

    fd = file_system_open(FILE_SYSTEM_TEST_PATH,
                          O_CREAT | O_EXCL | O_RDWR,
                          0U);
    if (fd < 0) {
        cli_shell_printf(shell,
                         "Filesystem test could not create test file: errno=%d\r\n",
                         errno);
        return -1;
    }
    file_created = true;

    result = file_system_write(fd, test_data, (uint32_t)expected_size);
    if (result != expected_size) {
        error_number = (errno != 0) ? errno : EIO;
        goto cleanup;
    }
    if (file_system_sync(fd) != 0) {
        error_number = errno;
        goto cleanup;
    }
    if (file_system_lseek(fd, 0, SEEK_SET) != 0) {
        error_number = errno;
        goto cleanup;
    }
    result = file_system_read(fd, readback, (uint32_t)expected_size);
    if (result != expected_size ||
        memcmp(readback, test_data, (size_t)expected_size) != 0) {
        error_number = (errno != 0) ? errno : EIO;
        goto cleanup;
    }
    if (file_system_close(fd) != 0) {
        fd = -1;
        error_number = errno;
        goto cleanup;
    }
    fd = -1;
    if (file_system_unlink(FILE_SYSTEM_TEST_PATH) != 0) {
        error_number = errno;
        goto cleanup;
    }
    file_created = false;
    cli_shell_put_line(shell, "Filesystem read/write/sync/cleanup test passed");
    return 0;

cleanup:
    if (fd >= 0 && file_system_close(fd) != 0) {
        cleanup_error = errno;
    }
    if (file_created && file_system_unlink(FILE_SYSTEM_TEST_PATH) != 0) {
        cleanup_error = errno;
    }
    cli_shell_printf(shell,
                     "Filesystem test failed: errno=%d cleanup_errno=%d\r\n",
                     error_number,
                     cleanup_error);
    return -1;
}

static int cli_shell_zigbee_handler(ShellContext_t *shell,
                                    int argc,
                                    char *argv[])
{
    if (shell == NULL || argc < 2 || argv == NULL || argv[1] == NULL) {
        cli_shell_zigbee_print_usage(shell);
        return -1;
    }

    if (strcmp(argv[1], "-w") == 0 && argc == 3 && argv[2] != NULL &&
        (strcmp(argv[2], "help") == 0 ||
         strcmp(argv[2], "--help") == 0)) {
        cli_shell_zigbee_print_write_help(shell);
        return 0;
    }

    if (!rf_app_is_initialized()) {
        cli_shell_put_line(shell, "Zigbee is not initialized");
        return -1;
    }
    if (rf_app_is_busy()) {
        cli_shell_put_line(shell, "Zigbee operation is still in progress");
        return -1;
    }

    if (strcmp(argv[1], "-c") == 0 && argc == 2) {
        if (!rf_app_connect(cli_shell_zigbee_connect_callback, shell)) {
            cli_shell_put_line(shell, "Unable to start Zigbee connect");
            return -1;
        }
        return 0;
    }
    if (strcmp(argv[1], "-r") == 0 && argc == 2) {
        if (!rf_app_read_parameters(cli_shell_zigbee_read_callback, shell)) {
            cli_shell_put_line(shell, "Unable to start Zigbee read");
            return -1;
        }
        return 0;
    }
    if (strcmp(argv[1], "-w") == 0) {
        return cli_shell_zigbee_write(shell, argc, argv);
    }
    if (strcmp(argv[1], "-reset") == 0 && argc == 2) {
        if (!rf_app_reset(cli_shell_zigbee_reset_callback, shell)) {
            cli_shell_put_line(shell, "Unable to start Zigbee reset");
            return -1;
        }
        return 0;
    }
    if (strcmp(argv[1], "-sleep") == 0 && argc == 2) {
        if (!rf_app_sleep(cli_shell_zigbee_sleep_callback, shell)) {
            cli_shell_put_line(shell, "Unable to start Zigbee sleep");
            return -1;
        }
        return 0;
    }

    cli_shell_zigbee_print_usage(shell);
    return -1;
}

static int cli_shell_zigbee_write(ShellContext_t *shell,
                                  int argc,
                                  char *argv[])
{
    const ZigbeeParameter_t *current_parameters = rf_app_get_parameters();
    ZigbeeParameter_t parameters;
    int index = 2;

    if (current_parameters == NULL) {
        cli_shell_put_line(shell, "Zigbee is not initialized");
        return -1;
    }
    parameters = *current_parameters;

    if (argc < 4) {
        cli_shell_zigbee_print_usage(shell);
        return -1;
    }

    while (index < argc) {
        const char *option = argv[index];

        if (option == NULL) {
            cli_shell_zigbee_print_usage(shell);
            return -1;
        }
        if (strcmp(option, "-code") == 0) {
            uint32_t value;
            size_t code_index;

            if (argc - index <= (int)ZIGBEE_SECURITY_CODE_SIZE) {
                cli_shell_zigbee_print_usage(shell);
                return -1;
            }
            for (code_index = 0U; code_index < ZIGBEE_SECURITY_CODE_SIZE;
                 code_index++) {
                const char *code_text = argv[index + 1 + (int)code_index];

                if (code_text == NULL ||
                    !cli_shell_zigbee_parse_value(code_text, &value) ||
                    value > UINT8_MAX) {
                    cli_shell_put_line(shell,
                                       "Invalid security code byte (0-255)");
                    return -1;
                }
                parameters.securityCode[code_index] = (uint8_t)value;
            }
            index += (int)ZIGBEE_SECURITY_CODE_SIZE + 1;
            continue;
        }

        if (index + 1 >= argc || argv[index + 1] == NULL ||
            !cli_shell_zigbee_set_parameter(&parameters, option,
                                            argv[index + 1])) {
            cli_shell_printf(shell, "Invalid Zigbee option or value: %s\n",
                             option);
            cli_shell_zigbee_print_usage(shell);
            return -1;
        }
        index += 2;
    }

    if (!rf_app_write_parameters(&parameters, cli_shell_zigbee_write_callback,
                                 shell)) {
        cli_shell_put_line(shell, "Unable to start Zigbee write");
        return -1;
    }
    return 0;
}

static bool cli_shell_zigbee_set_parameter(ZigbeeParameter_t *parameters,
                                           const char *option,
                                           const char *text)
{
    uint32_t value;

    if (parameters == NULL || option == NULL || text == NULL ||
        !cli_shell_zigbee_parse_value(text, &value)) {
        return false;
    }

    if (strcmp(option, "-panid") == 0 &&
        value >= ZIGBEE_PAN_ID_MIN && value <= ZIGBEE_PAN_ID_MAX) {
        parameters->PAN_ID = (uint16_t)value;
    } else if (strcmp(option, "-channel") == 0 &&
               value >= ZIGBEE_CHANNEL_MIN && value <= ZIGBEE_CHANNEL_MAX) {
        parameters->Channel = (uint8_t)value;
    } else if (strcmp(option, "-address") == 0 &&
               value >= ZIGBEE_PAN_ID_MIN && value <= ZIGBEE_PAN_ID_MAX) {
        parameters->userAddress = (uint16_t)value;
    } else if (strcmp(option, "-type") == 0 &&
               value >= ZIGBEE_POINT_TYPE_MIN &&
               value <= ZIGBEE_POINT_TYPE_MAX) {
        parameters->pointType = (uint8_t)value;
    } else if (strcmp(option, "-mode") == 0 &&
               value >= ZIGBEE_TRANSFER_MODE_MIN &&
               value <= ZIGBEE_TRANSFER_MODE_MAX) {
        parameters->transferModel = (uint8_t)value;
    } else if (strcmp(option, "-baudrate") == 0 &&
               value >= ZIGBEE_BAUD_RATE_MIN &&
               value <= ZIGBEE_BAUD_RATE_MAX) {
        parameters->uartBraudRate = (uint8_t)value;
    } else if (strcmp(option, "-databits") == 0 &&
               value == ZIGBEE_DATA_BITS_VALUE) {
        parameters->uartDataBits = (uint8_t)value;
    } else if (strcmp(option, "-stopbits") == 0 &&
               value == ZIGBEE_STOP_BITS_VALUE) {
        parameters->uartStopBits = (uint8_t)value;
    } else if (strcmp(option, "-parity") == 0 &&
               value >= ZIGBEE_PARITY_MIN && value <= ZIGBEE_PARITY_MAX) {
        parameters->uartParity = (uint8_t)value;
    } else if (strcmp(option, "-antenna") == 0 &&
               value <= ZIGBEE_ANTENNA_MAX) {
        parameters->antennaSelect = (uint8_t)value;
    } else if (strcmp(option, "-security") == 0 &&
               value <= ZIGBEE_SECURITY_ENABLED_MAX) {
        parameters->isSecurity = (uint8_t)value;
    } else {
        return false;
    }

    return true;
}

static bool cli_shell_zigbee_parse_value(const char *text, uint32_t *value)
{
    uint32_t parsed = 0U;
    uint32_t base = 10U;
    size_t index = 0U;

    if (text == NULL || value == NULL || text[0] == '\0' || text[0] == '-') {
        return false;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16U;
        index = 2U;
        if (text[index] == '\0') {
            return false;
        }
    }

    for (; text[index] != '\0'; index++) {
        uint32_t digit;
        char character = text[index];

        if (character >= '0' && character <= '9') {
            digit = (uint32_t)(character - '0');
        } else if (base == 16U && character >= 'a' && character <= 'f') {
            digit = (uint32_t)(character - 'a') + 10U;
        } else if (base == 16U && character >= 'A' && character <= 'F') {
            digit = (uint32_t)(character - 'A') + 10U;
        } else {
            return false;
        }
        if (digit >= base || parsed > (UINT32_MAX - digit) / base) {
            return false;
        }
        parsed = parsed * base + digit;
    }

    *value = parsed;
    return true;
}

static void cli_shell_zigbee_print_result(ShellContext_t *shell,
                                          const char *operation,
                                          uint8_t result)
{
    if (shell == NULL) {
        return;
    }

    if (result == ZIGBEE_RES_SUCCESS) {
        cli_shell_printf(shell, "Zigbee %s succeeded\n", operation);
    } else if (result == ZIGBEE_RES_TIMEOUT) {
        cli_shell_printf(shell, "Zigbee %s timed out\n", operation);
    } else {
        cli_shell_printf(shell, "Zigbee %s failed\n", operation);
    }
}

static void cli_shell_zigbee_connect_callback(ZigbeeMesh_t *mesh,
                                               uint8_t result,
                                               void *arg)
{
    (void)mesh;
    cli_shell_zigbee_print_result((ShellContext_t *)arg, "connect", result);
}

static void cli_shell_zigbee_read_callback(ZigbeeMesh_t *mesh,
                                            uint8_t result,
                                            void *arg)
{
    ShellContext_t *shell = (ShellContext_t *)arg;

    cli_shell_zigbee_print_result(shell, "read", result);
    if (result == ZIGBEE_RES_SUCCESS && mesh != NULL && shell != NULL) {
        const ZigbeeParameter_t *parameters = &mesh->param;

        cli_shell_printf(shell, "PAN ID: 0x%04X\n",
                         (uint32_t)parameters->PAN_ID);
        cli_shell_printf(shell, "Channel: %u\n",
                         (uint32_t)parameters->Channel);
        cli_shell_printf(shell, "Address: 0x%04X\n",
                         (uint32_t)parameters->userAddress);
        cli_shell_printf(shell, "Type: %u\n",
                         (uint32_t)parameters->pointType);
        cli_shell_printf(shell, "Transfer mode: %u\n",
                         (uint32_t)parameters->transferModel);
        cli_shell_printf(shell, "Baud rate setting: %u\n",
                         (uint32_t)parameters->uartBraudRate);
        cli_shell_printf(shell, "Data bits: %u\n",
                         (uint32_t)parameters->uartDataBits);
        cli_shell_printf(shell, "Stop bits: %u\n",
                         (uint32_t)parameters->uartStopBits);
        cli_shell_printf(shell, "Parity: %u\n",
                         (uint32_t)parameters->uartParity);
        cli_shell_printf(shell, "Antenna: %u\n",
                         (uint32_t)parameters->antennaSelect);
        cli_shell_printf(shell, "Security: %u\n",
                         (uint32_t)parameters->isSecurity);
    }
}

static void cli_shell_zigbee_write_callback(ZigbeeMesh_t *mesh,
                                             uint8_t result,
                                             void *arg)
{
    (void)mesh;
    cli_shell_zigbee_print_result((ShellContext_t *)arg, "write", result);
}

static void cli_shell_zigbee_reset_callback(ZigbeeMesh_t *mesh,
                                             uint8_t result,
                                             void *arg)
{
    (void)mesh;
    cli_shell_zigbee_print_result((ShellContext_t *)arg, "reset", result);
}

static void cli_shell_zigbee_sleep_callback(ZigbeeMesh_t *mesh,
                                             uint8_t result,
                                             void *arg)
{
    (void)mesh;
    cli_shell_zigbee_print_result((ShellContext_t *)arg, "sleep", result);
}

static void cli_shell_zigbee_print_usage(ShellContext_t *shell)
{
    if (shell == NULL) {
        return;
    }

    cli_shell_put_line(shell, "Usage:");
    cli_shell_put_line(shell, "  zigbee -c | -r | -reset | -sleep");
    cli_shell_put_line(shell, "  zigbee -w help");
}

static void cli_shell_zigbee_print_write_help(ShellContext_t *shell)
{
    if (shell == NULL) {
        return;
    }

    cli_shell_put_line(shell, "Zigbee write usage:");
    cli_shell_put_line(
        shell,
        "  zigbee -w [-panid value] [-channel value] [-address value]");
    cli_shell_put_line(
        shell,
        "    [-type value] [-mode value] [-baudrate value] [-databits 8]");
    cli_shell_put_line(
        shell,
        "    [-stopbits 1] [-parity value] [-antenna 0|1] [-security 0|1]");
    cli_shell_put_line(shell, "    [-code byte byte byte byte]");
    cli_shell_put_line(
        shell,
        "PAN ID/address: 0x0001-0xFF00; channel: 11-26.");
    cli_shell_put_line(
        shell,
        "Type: 1 coordinator, 2 router, 3 end device; mode: 1-5.");
    cli_shell_put_line(
        shell,
        "Baud rate: 1=1200 through 8=115200; parity: 1 none, 2 even, 3 odd.");
}
#include "dtu_board.h"


static int cli_shell_upgrade(ShellContext_t *shell,
                             int argc,
                             char *argv[])
{
    (void)argc;
    (void)argv;
    cli_shell_printf(shell, "Enter upgrade mode\r\n");
    bsp_delay(1000);
    enter_bootloader_mode();
    return 0;
}

static int cli_shell_sys_reboot(ShellContext_t *shell,
                                     int argc,
                                     char *argv[]){
    cli_shell_printf(shell,"System reboot ...");
    bsp_restart();
                                     }