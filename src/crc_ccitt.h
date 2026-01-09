#ifndef __CRC_CCITT_H
#define __CRC_CCITT_H

#include <stddef.h>
#include <stdint.h>

uint16_t crc_ccitt(const uint8_t *data, size_t len);

#endif	/* __CRC_CCITT_H */
