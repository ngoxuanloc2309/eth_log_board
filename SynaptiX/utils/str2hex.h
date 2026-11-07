#ifndef STR2HEX_H
#define STR2HEX_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

int hex_string_to_bytes(const char *hex, uint8_t *out, size_t *out_len);

#ifdef __cplusplus
}
#endif
#endif /* STR2HEX_H */