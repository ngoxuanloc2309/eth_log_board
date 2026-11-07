#ifndef __SNTP_APP_H__
#define __SNTP_APP_H__
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>


#define SNTP_SERVER_ADDRESS "pool.ntp.org"
#define SNTP_SERVER_ADDRESS "pool.ntp.org"
#define SNTP_GOOGLE_NTP "time.google.com"
#define SNTP_GOOGLE_2_NTP "time2.google.com"
#define SNTP_GOOGLE_3_NTP "time3.google.com"
#define SNTP_SERVER_PORT 123

/**
 * @brief Sets the system time using the provided SNTP time value.
 *
 * This function updates the system time based on the SNTP (Simple Network Time Protocol)
 * time value passed as an argument.
 *
 * @param sntp_time The SNTP time value to set, typically in seconds since the epoch.
 */
void sntp_set_time(uint32_t sntp_time);
/**
 * @brief Initializes the SNTP application.
 *
 * This function sets up the necessary resources and configurations
 * required for the SNTP (Simple Network Time Protocol) application
 * to operate. It should be called before using any SNTP-related
 * functionality.
 */
void sntp_app_init(void);
/**
 * @brief Retrieves the current SNTP timestamp.
 *
 * This function returns the current timestamp obtained via SNTP (Simple Network Time Protocol).
 *
 * @return uint32_t The current SNTP timestamp in seconds since the epoch.
 */
uint32_t sntp_get_timestamp();
#ifdef __cplusplus
}
#endif
#endif /* __SNTP_H__ */