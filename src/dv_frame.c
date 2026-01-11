#include "dv_frame.h"

#include <arpa/inet.h>
#include <endian.h>
#include <stdio.h>
#include <stdint.h>

#include "crc_ccitt.h"

int dv_radio_invalid_csum ( dv_radio_hdr_t *hdr)
{
	const size_t hdr_sz = sizeof( dv_radio_hdr_t);
	uint16_t read_csum = le16toh( hdr->p_fcs);
	uint16_t computed_csum = crc_ccitt( (uint8_t*) hdr, hdr_sz-2);
	printf( "Expected: %04x, Actual: %04x\n", read_csum, computed_csum);
	return ( read_csum != computed_csum );
}
