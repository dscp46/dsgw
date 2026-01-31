#include "dv_frame.h"

#include <arpa/inet.h>
#include <endian.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

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

void dv_insert_beep( int fd, struct sockaddr *addr, size_t addr_len)
{
	const uint8_t beep_frame[] = { 
		0x44, 0x53, 0x56, 0x54, 0x20, 0x00, 0x00, 0x00,
		0x20, 0x00, 0x02, 0x01, 0xf3, 0x37, 0x0d, 0xf3,
		0xcf, 0x38, 0xe2, 0xc7, 0x3c, 0x11, 0x45, 0x0c,
		0x16, 0x29, 0xf5
	};

	const uint8_t sil1_frame[] = { 
		0x44, 0x53, 0x56, 0x54, 0x20, 0x00, 0x00, 0x00,
		0x20, 0x00, 0x02, 0x01, 0xf3, 0x37, 0x0d, 0xae,
		0xcc, 0x2a, 0x78, 0xe1, 0x13, 0x3c, 0x67, 0xc0,
		0x16, 0x29, 0xf5
	};

	const uint8_t sil2_frame[] = { 
		0x44, 0x53, 0x56, 0x54, 0x20, 0x00, 0x00, 0x00,
		0x20, 0x00, 0x02, 0x01, 0xf3, 0x37, 0x0d, 0xce,
		0x8e, 0x2e, 0x39, 0x66, 0x12, 0x78, 0x31, 0xb0,
		0x16, 0x29, 0xf5
	};

	sendto( fd, beep_frame, 27, 0, addr, addr_len);
	usleep( 20000);
	sendto( fd, beep_frame, 27, 0, addr, addr_len);
	usleep( 20000);

	sendto( fd, sil1_frame, 27, 0, addr, addr_len);
	usleep( 20000);
	sendto( fd, sil2_frame, 27, 0, addr, addr_len);
	usleep( 20000);

	sendto( fd, beep_frame, 27, 0, addr, addr_len);
	usleep( 20000);
	sendto( fd, beep_frame, 27, 0, addr, addr_len);
	usleep( 20000);
}
