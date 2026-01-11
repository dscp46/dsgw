#include "crc_ccitt.h"

uint16_t crc_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0x8408;
            else              crc >>= 1;
        }
    }
    return (uint16_t)~crc;
}
