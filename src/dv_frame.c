#include "dv_frame.h"

#include <arpa/inet.h>
#include <endian.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
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

void dv_send_frame( int fd, struct sockaddr *addr, size_t addr_len, void *buf, size_t len, int fast, char *xrf_callsign, char *src_callsign, char *mesg)
{
	if( addr == NULL || buf == NULL) return;
	const uint8_t superframe_sync_pattern[] = { 0x55, 0x2D, 0x16};
	const uint8_t comfort_noise_a[] = { 0xce, 0x8e, 0x2e, 0x39, 0x66, 0x12, 0x78, 0x31, 0xb0 };
	const uint8_t comfort_noise_b[] = { 0xae, 0xcc, 0x2a, 0x78, 0xe1, 0x91, 0x34, 0x67, 0xc0 };
	uint16_t stream_id = (uint16_t) (rand() & 0x0000FFFF); // random stream number
	uint16_t dv_frame_ctr = 0; // Used to time fast data beep insertion
	uint8_t seq = 0; // Relative position in a superframe
	uint8_t segment_sz;
	uint8_t payload_sz, buffer[DV_STREAM_HDR_SZ], frames[(DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ)*3];
	uint8_t *data = (uint8_t*)buf;

	// Initialize the DSVT packet
	dv_stream_hdr_t *hdr_pkt = (dv_stream_hdr_t*) buffer;
	dv_radio_hdr_t *radio_hdr = (dv_radio_hdr_t*) hdr_pkt->radio_hdr;
	dv_stream_pkt_t *pkt = (dv_stream_pkt_t*) buffer;
	dv_trunk_hdr_t  *trk_hdr = (dv_trunk_hdr_t*)  pkt->trunk_hdr; // Reusable for the header and voice packets, same offsets

	// Initialize the packet headers
	memcpy( hdr_pkt->signature, "DSVT", 4);
	hdr_pkt->flag = htons( DSVT_TYPE_WIRELESS_HDR);
	trk_hdr->type = DV_TRUNK_TYPE_DV_COMM;
	trk_hdr->rsvd = 0;
	trk_hdr->dst_rpt_id = 0;
	trk_hdr->src_rpt_id = 2;
	trk_hdr->src_term_id = 1;
	trk_hdr->call_id = htons(stream_id);
	trk_hdr->mgmt_info = DV_TRUNK_HEADER;

	// Set the radio header
	memcpy( radio_hdr->rpt1 , xrf_callsign, 8);
	memcpy( radio_hdr->rpt2 , xrf_callsign, 7);
	radio_hdr->rpt2[7] = 'G';
	memcpy( radio_hdr->ur, "CQCQCQ  ", 8);
	memcpy( radio_hdr->my , src_callsign, 8);
	memcpy( radio_hdr->my+8 , "SGW ", 4);

	// Compute radio header CRC
	radio_hdr->p_fcs = htole16( crc_ccitt( (uint8_t*)radio_hdr, sizeof( dv_radio_hdr_t)-2));

	// Send 5 times the stream header
	for( int i=0; i<6; ++i)
	{

	}

	// Reconfigure headers
	hdr_pkt->flag = htons( DSVT_TYPE_DATA_PKT);

	if( mesg != NULL )
	{
		// Send first frame with the superframe sync pattern
		trk_hdr->mgmt_info = seq++ & DV_TRUNK_SEQ_MASK;
		memcpy( pkt->audio_frame, comfort_noise_a, DV_AUDIO_FRM_SZ);
		memcpy( pkt->data_frame , superframe_sync_pattern, DV_DATA_FRM_SZ);

		sendto( fd, buffer, DV_STREAM_PKT_SZ, 0, addr, addr_len);
		usleep( 20000);

		// Send TX message
		for( int i=0; i<4; ++i)
		{
			// First half of the S-Data frame
			memcpy( pkt->audio_frame, comfort_noise_b, DV_AUDIO_FRM_SZ);
			trk_hdr->mgmt_info = seq++ & DV_TRUNK_SEQ_MASK;
			pkt->data_frame[0] = i | DV_MINIHDR_MESSAGE;
			memcpy( pkt->data_frame+1, mesg+(i*5), DV_DATA_FRM_SZ-1);
			dv_scramble_data( pkt->data_frame, DV_DATA_FRM_SZ);

			sendto( fd, buffer, DV_STREAM_PKT_SZ, 0, addr, addr_len);
			usleep( 20000);

			// Second half of the S-Data frame
			memcpy( pkt->audio_frame, comfort_noise_a, DV_AUDIO_FRM_SZ);
			trk_hdr->mgmt_info = seq++ & DV_TRUNK_SEQ_MASK;
			memcpy( pkt->data_frame, mesg+(i*5)+2, DV_DATA_FRM_SZ);
			dv_scramble_data( pkt->data_frame, DV_DATA_FRM_SZ);

			sendto( fd, buffer, DV_STREAM_PKT_SZ, 0, addr, addr_len);
			usleep( 20000);
		}
	}

	while( len > 0 )
	{
		// Initialize the frame buffer, which contains 3 pairs of DV audio and data frames.
		// The order is [ even audio ][ even data ][ odd audio ][ odd data ][ sync audio ][ sync pattern ]
		// Frame parity is considered as described in the graphs in section 7.2, which is **the opposite** to `seq`'s parity
		// Initialization value is set to 0x66, as specified in section 6.1.3
		memset( frames, 0x66, (DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ)*3);
		memcpy( frames+(DV_AUDIO_FRM_SZ*3)+(DV_DATA_FRM_SZ*2), superframe_sync_pattern, DV_DATA_FRM_SZ);

		if( fast)
		{
			segment_sz = (seq == 0) ? 28 : 20;
			payload_sz = MIN( len, segment_sz);

			// Set mitigation bytes
			frames[                                   4] = DV_MITIGATION;
			frames[(DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ)  +4] = DV_MITIGATION;
			frames[(DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ)*2+4] = DV_MITIGATION;

			// Set the guard byte
			frames[(2*DV_AUDIO_FRM_SZ) + DV_DATA_FRM_SZ] = DV_GUARD;

			// Insert data
			do
			{
				// Fill first data frame
				memcpy( frames+ DV_AUDIO_FRM_SZ                      +1, data, MIN( payload_sz, DV_DATA_FRM_SZ-1));
				payload_sz -= MIN( payload_sz, DV_DATA_FRM_SZ-1);
				if( payload_sz == 0) break;

				// Fill second data frame, minus the guard byte
				memcpy( frames+(DV_AUDIO_FRM_SZ*2)+ DV_DATA_FRM_SZ   +1, data, MIN( payload_sz, DV_DATA_FRM_SZ-1));
				payload_sz -= MIN( payload_sz, DV_DATA_FRM_SZ-1);
				if( payload_sz == 0) break;

				// Fill first audio frame
				memcpy( frames                                         , data, MIN( payload_sz, 4));
				payload_sz -= MIN( payload_sz, 4);
				if( payload_sz == 0) break;

				memcpy( frames                                       +5, data, MIN( payload_sz, 4));
				payload_sz -= MIN( payload_sz, 4);
				if( payload_sz == 0) break;

				// Fill second audio frame
				memcpy( frames+(DV_AUDIO_FRM_SZ   + DV_DATA_FRM_SZ)    , data, MIN( payload_sz, 4));
				payload_sz -= MIN( payload_sz, 4);
				if( payload_sz == 0) break;

				memcpy( frames+(DV_AUDIO_FRM_SZ   + DV_DATA_FRM_SZ)  +5, data, MIN( payload_sz, 4));
				payload_sz -= MIN( payload_sz, 4);
				if( payload_sz == 0) break;

				// Fill the zeroth frame (only used at the beginning of a superframe)
				memcpy( frames+(DV_AUDIO_FRM_SZ   + DV_DATA_FRM_SZ)*2  , data, MIN( payload_sz, 4));
				payload_sz -= MIN( payload_sz, 4);
				if( payload_sz == 0) break;

				memcpy( frames+(DV_AUDIO_FRM_SZ   + DV_DATA_FRM_SZ)*2+5, data, MIN( payload_sz, 4));
				payload_sz -= MIN( payload_sz, 4);

			} while( 0) ;

			// Set the miniheader
			frames[DV_AUDIO_FRM_SZ] = MIN( len, segment_sz) | DV_MINIHDR_FAST_DATA;

			// Scramble data
			dv_scramble_data( frames                                       , DV_AUDIO_FRM_SZ);
			dv_scramble_data( frames+ DV_AUDIO_FRM_SZ                      , DV_DATA_FRM_SZ);
			dv_scramble_data( frames+ DV_AUDIO_FRM_SZ   + DV_DATA_FRM_SZ   , DV_AUDIO_FRM_SZ);
			dv_scramble_data( frames+(DV_AUDIO_FRM_SZ*2)+ DV_DATA_FRM_SZ   , DV_DATA_FRM_SZ);
			dv_scramble_data( frames+(DV_AUDIO_FRM_SZ*2)+(DV_DATA_FRM_SZ*2), DV_AUDIO_FRM_SZ);

			dv_frame_ctr += 2;
		}
		else
		{
			segment_sz = DV_DATA_FRM_SZ*2 - 1;
			payload_sz = MIN( len, segment_sz);

			// Pad the AMBE payload with our comfort noise AMBE data
			memcpy( frames                                   , comfort_noise_b, DV_AUDIO_FRM_SZ);
			memcpy( frames+DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ    , comfort_noise_a, DV_AUDIO_FRM_SZ);
			memcpy( frames+(DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ)*2, comfort_noise_a, DV_AUDIO_FRM_SZ);

			// Set the payload
			memcpy( frames+DV_AUDIO_FRM_SZ+1, data, MIN( payload_sz, DV_DATA_FRM_SZ-1));
			payload_sz -= MIN( payload_sz, DV_DATA_FRM_SZ-1);

			// Set the second payload, if applicable
			if( payload_sz > 0 )
				memcpy( frames+(DV_AUDIO_FRM_SZ*2)+DV_DATA_FRM_SZ, data, payload_sz);

			// Set Miniheader
			frames[DV_AUDIO_FRM_SZ] = MIN( len, segment_sz) | DV_MINIHDR_SLOW_DATA;

			// Decrement the amounts of bytes to send
			len -= MIN( len, segment_sz);

			// Scramble data
			dv_scramble_data( frames+ DV_AUDIO_FRM_SZ                  , DV_DATA_FRM_SZ);
			dv_scramble_data( frames+(DV_AUDIO_FRM_SZ*2)+DV_DATA_FRM_SZ, DV_DATA_FRM_SZ);
		}

		// Sync header
		if( seq == 0 )
		{
			memcpy( pkt->audio_frame, frames+(DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ)*2, DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ);
			trk_hdr->mgmt_info = seq & DV_TRUNK_SEQ_MASK;

			sendto( fd, buffer, DV_STREAM_PKT_SZ, 0, addr, addr_len);
			usleep( 20000);
			++seq;
		}

		// Send even frame
		memcpy( pkt->audio_frame, frames, DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ);
		trk_hdr->mgmt_info = seq & DV_TRUNK_SEQ_MASK;

		sendto( fd, buffer, DV_STREAM_PKT_SZ, 0, addr, addr_len);
		usleep( 20000);
		++seq; seq %= 21;

		// Send odd frame
		memcpy( pkt->audio_frame, frames+(DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ), DV_AUDIO_FRM_SZ+DV_DATA_FRM_SZ);
		trk_hdr->mgmt_info = seq & DV_TRUNK_SEQ_MASK;

		sendto( fd, buffer, DV_STREAM_PKT_SZ, 0, addr, addr_len);
		usleep( 20000);
		++seq; seq %= 21;

		// Insert fast data beep
		if( dv_frame_ctr >= 44 )
		{
			dv_insert_beep( fd, addr, addr_len, stream_id, &seq);
			dv_frame_ctr = 0;
		}
	}

	dv_insert_eot( fd, addr, addr_len, stream_id, &seq);
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
		hdr->mgmt_info = (*seq & DV_TRUNK_SEQ_MASK);

		sendto( fd, buffer, DV_STREAM_PKT_SZ, 0, addr, addr_len);
		usleep( 20000);
		++(*seq); *seq %= 21;
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
