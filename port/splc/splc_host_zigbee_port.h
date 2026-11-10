#ifndef SPLC_HOST_ZIGBEE_PORT_H
#define SPLC_HOST_ZIGBEE_PORT_H
/*
 * splc_host_zigbee_port.h - product port (outside the simple_plc library)
 *
 * UART9 belongs to the host rf_app, which parses the DRF1609H text protocol. Until that
 * ownership is changed, this port only declares the available Master role; it does not provide
 * a Modbus byte pipe or apply Zigbee configuration to the radio.
 * Compiled empty unless SPLC_ZIGBEE_ENABLE = 1.
 */
#ifdef __cplusplus
extern "C" {
#endif

/* Register before plc_engine_init(), from the engine task. */
void splc_host_zigbee_port_init(void);

#ifdef __cplusplus
}
#endif
#endif /* SPLC_HOST_ZIGBEE_PORT_H */