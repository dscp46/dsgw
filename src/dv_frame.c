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
	return ( read_csum != computed_csum );
}

int dv_last_frame( dv_trunk_hdr_t *hdr)
{
	return ( hdr->mgmt_info & 0xC0) == 0x40 ;
}

void dv_scramble_data( void *buffer, size_t len)
{
	/* Structure of DStar's strambler are explained in Annex 1 of the standard (https://www.jarl.com/d-star/STD7_0.pdf)
	   It is an additive scrambler which polynome is S_x = x^7 + x^4 + 1 (see Annex 1.1)
	   Bytes are presented to the scrambler from LSB to MSB (see Annex 1.4).
 	   When reset, the LFSR bits are all set to 1 (see Annex 1.1)
	   We use a fixed LUT, since we never need to scramble beyond 9 bytes of data without resetting the scrambler.
	*/
	const uint8_t scrambler_lut[] = { 0x70, 0x4f, 0x93, 0x40, 0x64, 0x74, 0x6d, 0x30, 0x2b};
	uint8_t *ptr = (uint8_t*) buffer;

	for( size_t i=0; i<len && i<sizeof(scrambler_lut); ++i)
		ptr[i] ^= scrambler_lut[i];

	return;
}
