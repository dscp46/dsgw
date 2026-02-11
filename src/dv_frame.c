#include "dv_frame.h"

#include <arpa/inet.h>
#include <endian.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
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

void dv_send_frame( int fd, struct sockaddr *addr, size_t addr_len, void *buf, size_t len, int fast, char *src_callsign, char *mesg)
{
	if( addr == NULL || buf == NULL) return;
	const uint8_t superframe_sync_pattern[] = { 0x55, 0x2D, 0x16};
	const uint8_t slow_data_silence_a[] = { 0xce, 0x8e, 0x2e, 0x39, 0x66, 0x12, 0x78, 0x31, 0xb0 };
	const uint8_t slow_data_silence_b[] = { 0xae, 0xcc, 0x2a, 0x78, 0xe1, 0x91, 0x34, 0x67, 0xc0 };
	uint16_t stream_id = (uint16_t) (rand() & 0x0000FFFF); // random stream number
	uint16_t dv_frame_ctr = 0; // Used for fast data beep intertion
	uint8_t seq = 0; // Relative position in a superframe

	// Send 6 times a traffic header

	while( len > 0 )
	{
		
	}

}

void dv_insert_beep( int fd, struct sockaddr *addr, size_t addr_len, uint16_t stream_id, uint8_t *seq)
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

	uint8_t buffer[DV_STREAM_PKT_SZ];
	dv_stream_pkt_t *pkt = (dv_stream_pkt_t*) buffer;
	dv_trunk_hdr_t  *hdr = (dv_trunk_hdr_t*)  pkt->trunk_hdr;

	for( int i=0; i<6; ++i)
	{
		switch(i)
		{
		case 2:
			memcpy( buffer, sil1_frame, DV_STREAM_PKT_SZ);
			break;
		case 3:
			memcpy( buffer, sil2_frame, DV_STREAM_PKT_SZ);
			break;
		default:
			memcpy( buffer, beep_frame, DV_STREAM_PKT_SZ);
		}

		if( *seq == 0)
			memcpy( pkt->data_frame, "\x55\x2D\x16", 3);

		hdr->call_id = htons(stream_id);
		hdr->mgmt_info = ((*seq)++ & DV_TRUNK_SEQ_MASK);
		*seq %= 21;

		sendto( fd, buffer, DV_STREAM_PKT_SZ, 0, addr, addr_len);
		usleep( 20000);
	}
}

void dv_insert_eot( int fd, struct sockaddr *addr, size_t addr_len, uint16_t stream_id, uint8_t *seq)
{
	const uint8_t eot_frame[] = {
		0x44, 0x53, 0x56, 0x54, 0x20, 0x00, 0x00, 0x00,
		0x20, 0x00, 0x02, 0x01, 0xad, 0x3e, 0x4a, 0x55,
		0x55, 0x55, 0x55, 0xc8, 0x7a, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00
	};

	uint8_t buffer[DV_STREAM_PKT_SZ];
	dv_stream_pkt_t *pkt = (dv_stream_pkt_t*) buffer;
	dv_trunk_hdr_t  *hdr = (dv_trunk_hdr_t*)  pkt->trunk_hdr;

	memcpy( buffer, eot_frame, DV_STREAM_PKT_SZ);
	hdr->call_id = htons(stream_id);
	hdr->mgmt_info = DV_TRUNK_LAST_FRAME | ((*seq)++ & DV_TRUNK_SEQ_MASK);

	sendto( fd, buffer, DV_STREAM_PKT_SZ, 0, addr, addr_len);
}
