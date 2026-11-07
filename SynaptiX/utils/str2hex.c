#include "str2hex.h"
#include "string.h"

static int hex_char_to_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int hex_string_to_bytes(const char *hex, uint8_t *out, size_t *out_len)
{
    size_t len = strlen(hex);
    if (len % 2 != 0) {
        *out_len = 0;
        return -1;
    }
    size_t bytes = len / 2;

    for (size_t i = 0; i < bytes; i++) {
        int hi = hex_char_to_val(hex[i * 2]);
        int lo = hex_char_to_val(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -3;
        out[i] = (hi << 4) | lo;
    }
    *out_len = bytes;
    return bytes;
}