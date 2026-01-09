#include "crc_ccitt.h"

uint16_t crc_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;  // initial value 0xFFFF
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else              crc <<= 1;
        }
    }
    return crc;
}
