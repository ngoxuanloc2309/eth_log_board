/*
 * splc_adc_port.c - ADC stub for the DTU Zigbee product.
 *
 * SPLC_ADC_BUILTIN = 0 means the host supplies sx_adc_*(). plc_io.c always references
 * sx_adc_read(), so the symbol must exist at link time, but this board has NO analog
 * input: the tag layout has 0 AI and no code calls plc_io_register_ai(), so the read
 * loop in input_scan() never runs. Returning 0 keeps the contract honest if it ever does.
 */
#include "sx_adc.h"

uint32_t sx_adc_read(sx_adc_config_t *config)
{
    (void)config;
    return 0U;
}