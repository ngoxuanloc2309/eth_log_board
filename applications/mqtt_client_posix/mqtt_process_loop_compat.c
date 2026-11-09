#include "core_mqtt.h"

MQTTStatus_t __real_MQTT_ProcessLoop(MQTTContext_t *context);

MQTTStatus_t __wrap_MQTT_ProcessLoop(MQTTContext_t *context)
{
    MQTTStatus_t status = __real_MQTT_ProcessLoop(context);

    /* coreMQTT-Agent treats this normal poll result as a fatal loop error. */
    return status == MQTTNoDataAvailable ? MQTTNeedMoreBytes : status;
}
