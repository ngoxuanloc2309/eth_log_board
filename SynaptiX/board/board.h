#ifndef BOARD_H
#define BOARD_H
#ifdef __cplusplus
extern "C" {
#endif

#include "gpio.h"
#include "usart.h"
#include "main.h"
#include "usb.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include <string.h>
#include "iwdg.h"
#include <stdio.h>
#include "adc.h"
// #include "usb_device.h"
// #include "usbd_cdc_acm_if.h"
#include "spi.h"

// #include "stm32h5xx_nucleo.h"

#define BOARD_NAME           "SX-ZB-ETH-V2.0"
#define BOARD_VENDOR         "SynaptiX"
#define BOARD_URL            "https://www.synaptix-io.com"

// Version
#define BSP_VERSION_MAJOR 2
#define BSP_VERSION_MINOR 0
#define BSP_VERSION_PATCH 0
#define BSP_VERSION_RC    0
#define BSP_VERSION ((BSP_VERSION_MAJOR << 24) | (BSP_VERSION_MINOR << 16) | (BSP_VERSION_PATCH << 8) | (BSP_VERSION_RC))
#define BSP_HW_VERSION_STR "2.0.0.rc0"
#define BSP_SW_VERSION_MAJOR 2
#define BSP_SW_VERSION_MINOR 0
#define BSP_SW_VERSION_PATCH 1
#define BSP_SW_VERSION_RC    3
#define BSP_SW_VERSION_STR "2.0.2.rc0"
#define VIRTUAL __attribute__((weak))

#define SPIF_DEBUG_DISABLE 0
#define SPIF_DEBUG_MIN 1
#define SPIF_DEBUG_FULL 2

#define SPIF_PLATFORM_HAL 0
#define SPIF_PLATFORM_HAL_DMA 1

#define SPIF_RTOS_DISABLE 0
#define SPIF_RTOS_CMSIS_V1 1
#define SPIF_RTOS_CMSIS_V2 2
#define SPIF_RTOS_THREADX 3
#define SPIF_DEBUG SPIF_DEBUG_DISABLE
#define SPIF_PLATFORM SPIF_PLATFORM_HAL
#define SPIF_RTOS SPIF_RTOS_CMSIS_V2

#define BSP_SPI_HANDLE &hspi1
#define BSP_SPI_CS_Port SPI1_CS_GPIO_Port
#define BSP_SPI_CS_Pin SPI1_CS_Pin

#define BSP_LED_NETWORK_Port LED0_GPIO_Port
#define BSP_LED_NETWORK_Pin  LED0_Pin

#define BSP_LED_ACTIVITY_Port LED1_GPIO_Port
#define BSP_LED_ACTIVITY_Pin  LED1_Pin

#define BSP_LED_ERROR_Port LED2_GPIO_Port
#define BSP_LED_ERROR_Pin  LED2_Pin


#define bsp_led_network_on() HAL_GPIO_WritePin(BSP_LED_NETWORK_Port,BSP_LED_NETWORK_Pin,GPIO_PIN_SET)
#define bsp_led_network_off() HAL_GPIO_WritePin(BSP_LED_NETWORK_Port,BSP_LED_NETWORK_Pin,GPIO_PIN_RESET)
#define bsp_led_network_toggle() HAL_GPIO_TogglePin(BSP_LED_NETWORK_Port,BSP_LED_NETWORK_Pin)

#define bsp_led_activity_on() HAL_GPIO_WritePin(BSP_LED_ACTIVITY_Port,BSP_LED_ACTIVITY_Pin,GPIO_PIN_SET)
#define bsp_led_activity_off() HAL_GPIO_WritePin(BSP_LED_ACTIVITY_Port,BSP_LED_ACTIVITY_Pin,GPIO_PIN_RESET)
#define bsp_led_activity_toggle() HAL_GPIO_TogglePin(BSP_LED_ACTIVITY_Port,BSP_LED_ACTIVITY_Pin)   

#define bsp_led_error_on() HAL_GPIO_WritePin(BSP_LED_ERROR_Port,BSP_LED_ERROR_Pin,GPIO_PIN_SET)
#define bsp_led_error_off() HAL_GPIO_WritePin(BSP_LED_ERROR_Port,BSP_LED_ERROR_Pin,GPIO_PIN_RESET)
#define bsp_led_error_toggle() HAL_GPIO_TogglePin(BSP_LED_ERROR_Port,BSP_LED_ERROR_Pin)
static inline char *bsp_get_hw_version(){
    // static char hw_version[20];
    // memset(hw_version,0,20);
    // sprintf(hw_version,"v%d.%d.%d.rc%d ",BSP_VERSION_MAJOR,BSP_VERSION_MINOR,BSP_VERSION_PATCH,BSP_VERSION_RC);
    return BSP_HW_VERSION_STR;
}
static inline char *bsp_get_sw_version(){
    // static char sw_version[20];
    // memset(sw_version,0,20);
    // sprintf(sw_version,"v%d.%d.%d.rc%d ",BSP_SW_VERSION_MAJOR,BSP_SW_VERSION_MINOR,BSP_SW_VERSION_PATCH,BSP_SW_VERSION_RC);
    return BSP_SW_VERSION_STR;
}
// Logging port

#define BSP_UART_PORT_NUM 5

#define LOG_PORT 0

// RS485 port
#define RS485_0_PORT 1
#define RS485_0_DE_GPIO_Port UART7_DE_GPIO_Port
#define RS485_0_DE_Pin UART7_DE_Pin

#define RS485_1_PORT 2
#define RS485_1_DE_GPIO_Port USART3_DE_GPIO_Port
#define RS485_1_DE_Pin USART3_DE_Pin

#define RS485_2_PORT 3
#define RS485_2_DE_GPIO_Port USART10_DE_GPIO_Port
#define RS485_2_DE_Pin USART10_DE_Pin
// RF port
#define RF_PORT 4

// Button


#define SERIAL_WORDLENGTH_8B UART_WORDLENGTH_8B
#define SERIAL_WORDLENGTH_9B UART_WORDLENGTH_9B

#define SERIAL_STOPBITS_1     UART_STOPBITS_1
#define SERIAL_STOPBITS_2     UART_STOPBITS_2

#define SERIAL_PARITY_NONE    UART_PARITY_NONE
#define SERIAL_PARITY_EVEN    UART_PARITY_EVEN
#define SERIAL_PARITY_ODD     UART_PARITY_ODD

// LED

#define bsp_led_on(x) BSP_LED_On(x)
#define bsp_led_off(x) BSP_LED_Off(x)

void board_init(void);
void board_deinit(void);


void uart_init(int port, int baudrate ,uint32_t rx_length);
void uart_write_bytes(int port, const uint8_t *data, uint32_t length);
int uart_read_bytes(int port, uint8_t *data, uint32_t length, uint32_t ticks_to_wait);
int uart_get_buffered_data_len(int port, size_t *length);
void uart_flush(int port);

/**
 * @brief Unlocks the FLASH control register access.
 *
 * This macro calls the HAL_FLASH_Unlock() function to enable write access to the FLASH control registers.
 * It is typically used before performing any FLASH memory operations such as programming or erasing.
 *
 * @note Ensure to lock the FLASH control register after operations by calling the appropriate lock function.
 */
#define bsp_flash_unlock() HAL_FLASH_Unlock()
/**
 * @brief   Locks the FLASH control register access.
 *
 * This macro calls the HAL_FLASH_Lock() function to disable write access
 * to the FLASH control registers, preventing accidental modification of
 * FLASH memory. It is typically used after FLASH operations are complete
 * to enhance system safety.
 */
#define bsp_flash_lock() HAL_FLASH_Lock()
#define NUMBER_SECTOR_OF_BANK 128
static inline uint32_t bsp_flash_get_sector(uint32_t address){
    uint32_t sector = ( (address - FLASH_BASE) / FLASH_SECTOR_SIZE);
    return sector % NUMBER_SECTOR_OF_BANK;
}

static inline uint32_t bsp_flash_get_bank(uint32_t address){
   uint32_t sector = ( (address - FLASH_BASE) / FLASH_SECTOR_SIZE);

   return (sector < NUMBER_SECTOR_OF_BANK)  ? FLASH_BANK_1 : FLASH_BANK_2 ;
}
/**
 * @brief Erases one or more pages in flash memory starting from the specified address.
 *
 * This function erases a specified number of flash memory pages, beginning at the given page address.
 *
 * @param page_address The starting address of the flash page to erase.
 * @param num_pages The number of consecutive pages to erase.
 * @return uint32_t Status code indicating success or failure of the erase operation.
 */
static inline uint32_t bsp_flash_erase(uint32_t address,uint32_t num_sector){
    FLASH_EraseInitTypeDef erase = {.Banks = bsp_flash_get_bank(address),.NbSectors = num_sector,.Sector = bsp_flash_get_sector(address),.TypeErase = FLASH_TYPEERASE_SECTORS};
    uint32_t page_err = 0;
    if(HAL_FLASHEx_Erase(&erase,&page_err) != HAL_OK){
        return page_err;
    }
    return 0;
}

/**
 * @brief Programs data to the flash memory at the specified address.
 *
 * This function writes a block of data to the flash memory starting at the given address.
 *
 * @param address The starting address in flash memory where data will be programmed.
 * @param data Pointer to the data buffer to be written to flash.
 * @param len The number of bytes to program from the data buffer.
 * @return uint32_t Status code indicating success or failure of the operation.
 */
static inline uint32_t bsp_flash_program(uint32_t addr,uint8_t *buf,uint32_t len) {
    // HAL_FLASH_Unlock();
    __disable_irq();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    for (uint32_t i = 0; i < len; i += 16) {
        uint8_t quadword[16];
        memset(quadword, 0xFF, 16);
        memcpy(quadword, &buf[i], (len - i >= 16) ? 16 : (len - i));
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, addr + i, (uint32_t)quadword) != HAL_OK) {
            __enable_irq();
            // HAL_FLASH_Lock();
            return -1;
        }
    }
    __enable_irq();
    // HAL_FLASH_Lock();
    return 0;
}

/**
 * @brief Reads data from flash memory at the specified address.
 *
 * @param address The starting address in flash memory to read from.
 * @param data Pointer to the buffer where the read data will be stored.
 * @param len Number of bytes to read from flash memory.
 * @return uint32_t Status code indicating success or failure of the read operation.
 */
static inline uint32_t bsp_flash_read(uint32_t address,void *data,uint32_t len){
    memcpy(data,(void*)address,len);
    return 0;
}

void bsp_restart();
#define bsp_get_chip_id() HAL_GetDEVID()
#define bsp_get_tick() HAL_GetTick()
#define bsp_delay(ms) vTaskDelay(pdMS_TO_TICKS(ms))
#define bsp_iwdg_refresh() HAL_IWDG_Refresh(&hiwdg)
#define bsp_enable_irq() __enable_irq()
#define bsp_disable_irq() __disable_irq()

// #define bsp_button_status() !HAL_GPIO_ReadPin(BUTTON_GPIO_Port,BUTTON_Pin)

#define BT_ST_ON 1
#define BT_ST_OFF 0

#define bsp_button_status() !HAL_GPIO_ReadPin(BUTTON_GPIO_Port,BUTTON_Pin)
float bsp_get_mcu_temperature();

#define USB_ENABLE 1

#if USB_ENABLE

#define USB_MAX_CHANNEL 1

#define USB_MB -1
#define USB_SHELL 0
#define USB_RPC -1

/**
 * @brief Transmits data over USB CDC (Communications Device Class).
 *
 * This function sends a buffer of data over the specified USB CDC channel.
 *
 * @param ch   The USB CDC channel number to transmit on.
 * @param buff Pointer to the buffer containing the data to transmit.
 * @param len  The number of bytes to transmit from the buffer.
 * @return     0 on success, or a negative error code on failure.
 */
int bsp_usb_cdc_transmit(int ch,uint8_t *buff,size_t len);
/**
 * @brief Reads data from the specified USB CDC channel.
 *
 * This function attempts to read up to 'len' bytes of data from the USB CDC interface
 * associated with the given channel 'ch' into the buffer 'buff'.
 *
 * @param ch    The USB CDC channel number to read from.
 * @param buff  Pointer to the buffer where the read data will be stored.
 * @param len   The maximum number of bytes to read.
 * @return      The number of bytes actually read, or a negative value on error.
 */
int bsp_usb_cdc_read(int ch,uint8_t *buff,size_t len);
/**
 * @brief Callback function for receiving data over USB CDC.
 *
 * This function is called when data is received on the specified USB CDC channel.
 *
 * @param ch   The USB CDC channel number.
 * @param buff Pointer to the buffer containing the received data.
 * @param len  The length of the received data in bytes.
 * @return     Status code (implementation-defined).
 */
int bsp_usb_cdc_rx_callback(int ch,uint8_t *buff,size_t len);
/**
 * @brief Returns the number of bytes available to read from the USB CDC interface.
 *
 * This function checks how many bytes are currently available in the receive buffer
 * for the specified USB CDC channel.
 *
 * @param ch The USB CDC channel number to query.
 * @return The number of bytes available to read from the specified channel.
 */
uint32_t bsp_usb_cdc_available(int ch);
/**
 * @brief Flushes the USB CDC (Communication Device Class) buffer for the specified channel.
 *
 * This function clears any pending data in the USB CDC buffer associated with the given channel,
 * ensuring that the buffer is empty and ready for new data transmission or reception.
 *
 * @param ch The channel number of the USB CDC interface to flush.
 */
void bsp_usb_cdc_flush(int ch);
#endif


/**
 * @brief Allocates a block of memory of the specified size.
 *
 * This function reserves a contiguous block of memory from the heap,
 * similar to the standard malloc function. The allocated memory is
 * uninitialized.
 *
 * @param size The number of bytes to allocate.
 * @return A pointer to the beginning of the allocated memory block,
 *         or NULL if the allocation fails.
 */
void *bsp_malloc(size_t size);
/**
 * @brief Allocates memory for an array of elements and initializes all bytes to zero.
 *
 * This function allocates memory for an array of @p num elements, each of them @p size bytes long,
 * and returns a pointer to the allocated memory. The memory is set to zero.
 *
 * @param num  Number of elements to allocate.
 * @param size Size of each element in bytes.
 * @return Pointer to the allocated zero-initialized memory, or NULL if allocation fails.
 */
void *bsp_calloc(size_t num ,size_t size);
/**
 * @brief Frees memory previously allocated by the board support package (BSP).
 *
 * This function releases the memory block pointed to by @p p, which must have been
 * returned by a previous BSP memory allocation function. After calling this function,
 * the pointer @p p must not be used unless it is assigned a new valid memory address.
 *
 * @param p Pointer to the memory block to be freed.
 */
void bsp_free(void *p);

// RTC

#include "time.h"

/**
 * @brief Retrieves the current system time.
 *
 * This function returns a pointer to a statically allocated `struct tm` 
 * containing the current date and time information.
 *
 * @return Pointer to a `struct tm` structure representing the current time.
 *         The pointer may be NULL if the time could not be retrieved.
 */
struct tm *bsp_get_time();
/**
 * @brief Sets the system time.
 *
 * This function updates the system's real-time clock with the provided time.
 *
 * @param time Pointer to a struct tm containing the new time to set.
 */
void bsp_set_time(struct timeval * tv, const struct timezone *tz);
uint32_t bsp_get_timestamp();
// Temperature Sensor

/**
 * @brief Retrieves the current temperature reading from the board's sensor.
 *
 * This function interfaces with the board support package (BSP) to obtain
 * the latest temperature measurement. The specific sensor and units (e.g., Celsius)
 * depend on the board implementation.
 *
 * @return int The current temperature reading.
 */
int bsp_get_temp();

#ifdef __cplusplus
}
#endif
#endif /* BOARD_H */