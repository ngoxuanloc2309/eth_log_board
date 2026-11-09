#ifndef INDICATOR_SERVICE_IF_H
#define INDICATOR_SERVICE_IF_H

#include "indicator_app.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    INDICATOR_SERVICE_RESULT_OK = 0,
    INDICATOR_SERVICE_RESULT_INVALID_ARGUMENT,
    INDICATOR_SERVICE_RESULT_NOT_READY,
    INDICATOR_SERVICE_RESULT_QUEUE_FULL
} indicator_service_result_t;

/**
 * @brief Report or clear an active indicator fault from task context.
 *
 * The application-owned indicator task applies updates asynchronously.
 *
 * @param error Subsystem error to update; NONE is not reportable.
 * @param active true to mark the error active, false to clear it.
 * @return Result indicating whether the update was queued.
 */
indicator_service_result_t indicator_service_set_error(
    indicator_error_t error,
    bool active);

#ifdef __cplusplus
}
#endif

#endif
