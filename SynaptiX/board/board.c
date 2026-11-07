#include "board.h"
#include <assert.h>
#include "cqueue.h"
#include "cJSON.h"
#include "logger.h"
#include "dts.h"
#include <stdio.h>
#include "rng.h"
#include "rtc.h"
#include "w25q_mem.h"

// static const char *TAG = "BSP";

typedef struct BSP_UART
{
    int port;
    CQueue_t rx_queue;
    SemaphoreHandle_t mutex;
    size_t rx_count;
    uint8_t *rx_buffer;
} BSP_UART_t;

UART_HandleTypeDef *hal_uart[BSP_UART_PORT_NUM] = {&huart1, &huart7, &huart3, &huart10, &huart9}; // 0 : LOG, 1 : RS485-1, 2 : RS485-2, 3 : RS485-3, 4 : RF

BSP_UART_t bsp_uarts[BSP_UART_PORT_NUM] = {
    {.port = LOG_PORT, .mutex = NULL, .rx_count = 0, .rx_buffer = NULL},     // Debug
    {.port = RS485_0_PORT, .mutex = NULL, .rx_count = 0, .rx_buffer = NULL}, // RS485-1
    {.port = RS485_1_PORT, .mutex = NULL, .rx_count = 0, .rx_buffer = NULL}, // RS485-2
    {.port = RS485_2_PORT, .mutex = NULL, .rx_count = 0, .rx_buffer = NULL}, // RS485-3
    {.port = RF_PORT, .mutex = NULL, .rx_count = 0, .rx_buffer = NULL}       // RF
};

uint8_t uart_data[BSP_UART_PORT_NUM];
#if USB_ENABLE
#define USB_BUFF_SIZE 512
CQueue_t usb_queue[USB_MAX_CHANNEL];
uint8_t usb_buff[USB_MAX_CHANNEL][USB_BUFF_SIZE];
#endif

volatile cJSON_Hooks jsonHook;

void *bsp_malloc(size_t size)
{
    return pvPortMalloc(size);
}
void bsp_free(void *p)
{
    vPortFree(p);
}
void *bsp_calloc(size_t num, size_t size)
{
    return pvPortCalloc(num, size);
}

void board_init(void)
{
    HAL_DTS_Start(&hdts);
    W25Q_Init();
    jsonHook.free_fn = bsp_free;
    jsonHook.malloc_fn = bsp_malloc;
    cJSON_InitHooks((cJSON_Hooks *)&jsonHook);
    for (int i = 0; i < BSP_UART_PORT_NUM; i++)
    {
        uart_init(i, 115200, 4096);
        HAL_UART_Receive_IT(hal_uart[i], &uart_data[i], 1);
    }

#if USB_ENABLE
    for (int i = 0; i < USB_MAX_CHANNEL; i++)
    {
        cqueue_init_static(&usb_queue[i], usb_buff[i], USB_BUFF_SIZE, sizeof(uint8_t));
    }
#endif
}

void board_deinit(void)
{
}

void uart_init(int port, int baudrate, uint32_t rx_length)
{
    if (port < 0 || port > BSP_UART_PORT_NUM)
        return;
    bsp_uarts[port].rx_buffer = malloc(rx_length);
    assert(bsp_uarts[port].rx_buffer != NULL);
    bsp_uarts[port].rx_count = 0;
    cqueue_init_static(&bsp_uarts[port].rx_queue, bsp_uarts[port].rx_buffer, rx_length, sizeof(uint8_t));
    bsp_uarts[port].mutex = xSemaphoreCreateMutex();
    assert(bsp_uarts[port].mutex != NULL);
}

void usart_de_on(int port)
{

    switch (port)
    {
    case RS485_0_PORT:
        /* code */
        HAL_GPIO_WritePin(RS485_0_DE_GPIO_Port, RS485_0_DE_Pin, GPIO_PIN_SET);
        break;
    case RS485_1_PORT:
        /* code */
        HAL_GPIO_WritePin(RS485_1_DE_GPIO_Port, RS485_1_DE_Pin, GPIO_PIN_SET);
        break;
    case RS485_2_PORT:
        /* code */
        HAL_GPIO_WritePin(RS485_2_DE_GPIO_Port, RS485_2_DE_Pin, GPIO_PIN_SET);
        break;
    default:
        break;
    }
}

void usart_de_off(int port)
{

    switch (port)
    {
    case RS485_0_PORT:
        /* code */
        HAL_GPIO_WritePin(RS485_0_DE_GPIO_Port, RS485_0_DE_Pin, GPIO_PIN_RESET);
        break;
    case RS485_1_PORT:
        /* code */
        HAL_GPIO_WritePin(RS485_1_DE_GPIO_Port, RS485_1_DE_Pin, GPIO_PIN_RESET);
        break;
    case RS485_2_PORT:
        /* code */
        HAL_GPIO_WritePin(RS485_2_DE_GPIO_Port, RS485_2_DE_Pin, GPIO_PIN_RESET);
        break;
    default:
        break;
    }
}

void uart_write_bytes(int port, const uint8_t *data, uint32_t length)
{
    if (port < 0 || port > BSP_UART_PORT_NUM)
        return;
    xSemaphoreTake(bsp_uarts[port].mutex, portMAX_DELAY);
    usart_de_on(port);
    HAL_UART_Transmit(hal_uart[port], (uint8_t *)data, length, HAL_MAX_DELAY);
    usart_de_off(port);
    xSemaphoreGive(bsp_uarts[port].mutex);
}

int uart_read_bytes(int port, uint8_t *data, uint32_t length,
                    uint32_t ticks_to_wait)
{
    if (port < 0 || port > BSP_UART_PORT_NUM)
        return -1;
    if(data == NULL || length == 0) return -1;

    uint16_t i = 0;
    uint32_t tickstart = bsp_get_tick();

    while((bsp_get_tick() - tickstart) < ticks_to_wait)
    {
        if(cqueue_receive(&bsp_uarts[port].rx_queue, &data[i]))
        {
            i++;
            if(i >= length)
                break;
        }
        else{
            bsp_delay(1);
        }
    }

    return i;
}

int uart_get_buffered_data_len(int port, size_t *length)
{
    if (port < 0 || port > BSP_UART_PORT_NUM)
        return -1;

    *length = bsp_uarts[port].rx_queue.count;
    return 0;
}

void uart_flush(int port)
{
    if (port < 0 || port > BSP_UART_PORT_NUM)
        return;

    bsp_uarts[port].rx_queue.count = 0;
    bsp_uarts[port].rx_queue.head = 0;
    bsp_uarts[port].rx_queue.tail = 0;
    bsp_uarts[port].rx_count = 0;
}

float bsp_get_mcu_temperature()
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t adc_value = 0;
    float temperature = 0.0f;

    // Configure ADC channel to internal temperature sensor
    sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_24CYCLES_5; // ensure long enough sampling time
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;

    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    // Start ADC conversion
    HAL_ADC_Start(&hadc1);

    // Wait for conversion
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);

    // Read ADC value
    adc_value = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    // --- Convert ADC value to temperature ---
    // Get calibration values from system memory
    uint32_t *TS_CAL1 = (uint32_t *)0x08FFF810; // 30°C calibration
    uint32_t *TS_CAL2 = (uint32_t *)0x08FFF814; // 130°C calibration
    float temp30 = (float)(*TS_CAL1);
    float temp130 = (float)(*TS_CAL2);

    // Convert to Celsius
    temperature = ((float)(adc_value - temp30) * (130.0f - 30.0f) / (temp130 - temp30)) + 30.0f;

    return temperature;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    for (int i = 0; i < BSP_UART_PORT_NUM; i++)
    {
        if (huart->Instance == hal_uart[i]->Instance)
        {
            cqueue_send(&bsp_uarts[i].rx_queue, &uart_data[i]);
            bsp_uarts[i].rx_count++;
            HAL_UART_Receive_IT(hal_uart[i], &uart_data[i], 1); // RS485
            return;
        }
    }
}
#if USB_ENABLE

// #include "usbd_cdc_acm_if.h"
#include "tusb.h"

void bsp_restart()
{
    // for (int i = 0; i < BSP_UART_PORT_NUM; i++)
    // {
    //     HAL_UART_DeInit(hal_uart[i]);
    // }
    // HAL_ADC_DeInit(&hadc1);
    // HAL_RNG_DeInit(&hrng);
    // HAL_RTC_DeInit(&hrtc);
    // HAL_DTS_DeInit(&hdts);
    // HAL_RCC_DeInit();
    // HAL_DeInit();
    // SysTick->CTRL = 0;
    // SysTick->LOAD = 0;
    // SysTick->VAL = 0;
    // SCB->VTOR = FLASH_BASE;
    // __set_MSP(*(volatile uint32_t *)FLASH_BASE);
    // void (*app_reset_handler)(void) = (void *)(*((volatile uint32_t *)(FLASH_BASE + 4U)));
    NVIC_SystemReset();
}

int bsp_usb_cdc_transmit(int ch, uint8_t *buff, size_t len)
{
    if (0 > ch && ch >= USB_MAX_CHANNEL)
        return -1;
    return tud_cdc_n_write(ch,buff,len);
}

int bsp_usb_cdc_read(int ch, uint8_t *buff, size_t len)
{
    // if (0 > ch && ch >= USB_MAX_CHANNEL)
    //     return -1;
    // size_t i = 0;
    // for (i = 0; i < len; i++)
    //     if (!cqueue_receive(&usb_queue[ch], &buff[i]))
    //         break;
    // return i;
    return tud_cdc_n_read(ch,buff,len);
}

int bsp_usb_cdc_rx_callback(int ch, uint8_t *buff, size_t len)
{
    if (0 > ch && ch >= USB_MAX_CHANNEL)
        return -1;
    // log_debug(TAG,"USB CH %d Receive %lu bytes",ch,len);
    size_t i = 0;
    for (i = 0; i < len; i++)
        if (!cqueue_send(&usb_queue[ch], &buff[i]))
            break;
    return (int)i;
}
uint32_t bsp_usb_cdc_available(int ch)
{
    if (0 > ch && ch >= USB_MAX_CHANNEL)
        return 0;
    return tud_cdc_n_available(ch);
}
void bsp_usb_cdc_flush(int ch)
{
    if (0 > ch && ch >= USB_MAX_CHANNEL)
        return;
    tud_cdc_n_read_flush(ch);
}
#endif

#include "rtc.h"

struct tm sTime;

struct tm *bsp_get_time()
{
    RTC_TimeTypeDef gTime;
    RTC_DateTypeDef gDate;

    HAL_RTC_GetTime(&hrtc, &gTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &gDate, RTC_FORMAT_BIN);

    sTime.tm_hour = gTime.Hours;
    sTime.tm_min = gTime.Minutes;
    sTime.tm_sec = gTime.Seconds;
    sTime.tm_wday = gDate.Date;
    sTime.tm_mon = gDate.Month;
    sTime.tm_year = gDate.Year + 100;
    sTime.tm_yday = gDate.Date;
    return &sTime;
}

/** Convert a BCD byte (0x00-0x99) to an integer. */
static inline uint8_t bcd2bin(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10U) + (bcd & 0x0FU));
}

/** Convert an integer (0-99) to a BCD byte. */
static inline uint8_t bin2bcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10U) << 4) | (bin % 10U));
}

/**
 * @brief  Convert a broken-down UTC time (struct tm) to a Unix timestamp.
 *
 * A small mktime() replacement that avoids DST / timezone issues that can
 * affect the C-library version when running on bare-metal targets.
 *
 * Valid range: 1970-01-01 00:00:00 UTC … 2106 (uint32_t seconds).
 */
static time_t tm_to_unix(const struct tm *t)
{
    /* Days in each month for a non-leap year */
    static const uint16_t days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    int year  = t->tm_year + 1900; /* tm_year is years since 1900 */
    int month = t->tm_mon;         /* 0-based */
    int day   = t->tm_mday - 1;    /* 0-based */

    /* Total days from epoch to start of this year */
    int y = year - 1970;
    time_t days = (time_t)y * 365
                + (time_t)((y + 1) / 4)    /* leap years since 1972 */
                - (time_t)((y + 69) / 100)  /* century corrections      */
                + (time_t)((y + 369) / 400);

    /* Add days for months elapsed this year */
    days += days_before_month[month];

    /* Leap-day correction: add 1 if we are past February in a leap year */
    if (month > 1) {
        int is_leap = ((year % 4 == 0) && (year % 100 != 0)) ||
                      (year % 400 == 0);
        days += is_leap ? 1 : 0;
    }

    days += day;

    return days * 86400L
         + (time_t)t->tm_hour * 3600L
         + (time_t)t->tm_min  * 60L
         + (time_t)t->tm_sec;
}

int _gettimeofday(struct timeval *tv, void *tzvp)
{
    if (!tv) {
        return -1;
    }
    tv->tv_sec = 0;
    tv->tv_usec = 0;

    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK ||
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
        return -1;
    }
    //printf("tm : %lu\r\n",(uint32_t)tv->tv_sec);
    
    //printf("RTC read: %04d-%02d-%02d %02d:%02d:%02d\r\n",
          //  sDate.Year + 2000, sDate.Month, sDate.Date,
          //  sTime.Hours, sTime.Minutes, sTime.Seconds);
    
    struct tm utc = { 0 };
    
    utc.tm_year = sDate.Year + 100; /* tm_year is years since 1900 */
    utc.tm_mon  = sDate.Month - 1;  /* tm_mon is 0-based */
    utc.tm_mday = sDate.Date;
    utc.tm_hour = sTime.Hours;
    utc.tm_min  = sTime.Minutes;
    utc.tm_sec  = sTime.Seconds;
    time_t t = tm_to_unix(&utc); /* mktime: converts broken-down time to Unix timestamp */
    if (t == (time_t)-1) {
        return -1;
    }

    //printf("Unix timestamp: %lu\r\n", (uint32_t)t);
    tv->tv_sec = t;
    return 0;
}

void bsp_set_time(struct timeval * tv, const struct timezone *tz)
{
    if(tv == NULL){
        return;
    }

    /* Convert Unix timestamp → broken-down UTC */
    time_t t = tv->tv_sec;
    struct tm utc;
    gmtime_r(&t, &utc);

    RTC_TimeTypeDef sTime = {0};
    sTime.Hours   = (uint8_t)utc.tm_hour;
    sTime.Minutes = (uint8_t)utc.tm_min;
    sTime.Seconds = (uint8_t)utc.tm_sec;
    sTime.TimeFormat      = RTC_HOURFORMAT_24; /* ignored in 24-h mode */
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_SUB1H;
    sTime.StoreOperation = RTC_STOREOPERATION_SET;
    /* Sub-second: scale tv_usec (0-999999) to the RTC prescaler range.
     * STM32H7 default: PREDIV_S = 255 → 1 tick ≈ 3.9 ms.
     * SubSeconds counts DOWN from PREDIV_S, so invert: */
    sTime.SubSeconds      = 0; /* set below if tv_usec provided */
    sTime.SecondFraction  = 255U; /* must match PREDIV_S – 1 */

    if (tv->tv_usec != 0) {
        /* Map 0..999999 µs → 255..0 ticks */
        uint32_t ticks = (uint32_t)(255U - ((uint32_t)tv->tv_usec * 255U / 1000000U));
        sTime.SubSeconds = ticks;
    }
    /* ---- Populate HAL RTC_DateTypeDef --------------------------------- */
    RTC_DateTypeDef sDate = {0};
    sDate.Year  = (uint8_t)(utc.tm_year - 100); /* HAL: years since 2000 */
    sDate.Month = (uint8_t)(utc.tm_mon + 1);    /* HAL: 1-based          */
    sDate.Date  = (uint8_t)utc.tm_mday;
    /* tm_wday: 0=Sun; HAL: RTC_WEEKDAY_MONDAY=1 … RTC_WEEKDAY_SUNDAY=7 */
    static const uint8_t wday_map[7] = {
        RTC_WEEKDAY_SUNDAY,
        RTC_WEEKDAY_MONDAY,
        RTC_WEEKDAY_TUESDAY,
        RTC_WEEKDAY_WEDNESDAY,
        RTC_WEEKDAY_THURSDAY,
        RTC_WEEKDAY_FRIDAY,
        RTC_WEEKDAY_SATURDAY
    };
    sDate.WeekDay = wday_map[utc.tm_wday];

    /* ---- Write to hardware -------------------------------------------- */
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
        return ;
    }
    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
        return ;
    }
    //printf("RTC set to: %04d-%02d-%02d %02d:%02d:%02d\r\n", sDate.Year + 2000,
          //  sDate.Month, sDate.Date, sTime.Hours, sTime.Minutes, sTime.Seconds);
    struct timeval tv_now;
    _gettimeofday(&tv_now, NULL);
}
uint32_t bsp_get_timestamp(){
    struct timeval tv_now;
    if(0 == _gettimeofday(&tv_now,NULL))
        return tv_now.tv_sec;
    return 0;
}
int bsp_get_temp()
{
    int32_t Temperature;
    HAL_DTS_GetTemperature(&hdts, &Temperature);
    return Temperature;
}

void assert_failed(uint8_t *file, uint32_t line)
{
    printf("assert failed file : %s , line : %lu \r\n", file, line);
}