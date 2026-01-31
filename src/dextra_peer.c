#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <utstring.h>

#include "common.h"
#include "dextra_peer.h"
#include "kiss.h"

dextra_peer_t* dextra_peer_init( peer_key_t *lookup_key, struct sockaddr_in6 *client_addr, unsigned char *buffer, const char *kiss_addr, uint16_t kiss_port)
{
	dextra_peer_t *peer = (dextra_peer_t*) malloc( sizeof( dextra_peer_t));

	if( peer == NULL )
		return NULL;

	// Setup connection, abort if unable
	if((peer->child_fd = kiss_new_connection( kiss_addr, kiss_port)) < 1)
	{
		fprintf( 
			stderr, 
			"%s: unable to setup a KISS connection, %s.\n", 
			__FUNCTION__,
			strerror( errno)
		);
		free(peer);
		return NULL;
	}

	// Copy attributes
	memcpy( &peer->key,  lookup_key, sizeof( peer_key_t));
	memcpy( &peer->addr, client_addr, sizeof( struct sockaddr_in6));
	memcpy( peer->rpt1, buffer, 8);
	peer->ka_ttl  = DEXTRA_KA_INTVL-1; 
	peer->last_rx = 0; 
	peer->ifs_timer = 0; 
	peer->rpt1[9] = '\0';
	peer->rx_idle = 1;
	utstring_new( peer->reassembled_data);
	peer->rx_frame.simple_data_bytes = 0;
	peer->feat_flags = 0;

	return peer;
}

void dextra_peer_destroy( dextra_peer_t* peer)
{
	if( peer == NULL )
		return;
	
	utstring_free( peer->reassembled_data);
	close( peer->child_fd);
	free( peer);
}

int dextra_peer_parse_pkt( dextra_peer_t *peer, dv_stream_pkt_t *pkt)
{
	if( peer == NULL || pkt == NULL ) return EINVAL;
	dv_trunk_hdr_t *trunk_hdr = (dv_trunk_hdr_t*) pkt->trunk_hdr;
	uint16_t sid = ntohs( trunk_hdr->call_id);
	uint8_t  seq = trunk_hdr->mgmt_info & DV_TRUNK_SEQ_MASK; // 00h~14h 
	uint32_t seq_mask = 1 << seq;
	unsigned int s_frame_offset = ((seq+1)>>1)-1;
	unsigned char *s_frame = NULL;

	if( peer->rx_frame.stream_id != sid) return EPROTO; // SID mismatch
	if( trunk_hdr->mgmt_info & DV_TRUNK_HEALTH_FLAG ) return EPROTO; // Corrupted frame
	if( seq > 20 ) return EPROTO; // Out of bounds sequence number

	// FIXME: Set up unordered delivery management
	
	// Accumulate Audio data
	memcpy( 
		peer->rx_frame.ambe_data + seq * DV_AUDIO_FRM_SZ,
		pkt->audio_frame,
		DV_AUDIO_FRM_SZ
	);

	// Descramble Audio data (for fast data)
	dv_scramble_data(
		peer->rx_frame.ambe_data + seq * DV_AUDIO_FRM_SZ,
		DV_AUDIO_FRM_SZ
	);

	if( seq == 0x00 )
	{
		// Sync 
		// Initialize rx frame
		memset( peer->dv_data_accumul, 0, sizeof(peer->dv_data_accumul)); 
		peer->dv_rx_mask = 0;
	}
	else 
	{
		// Descramble
		dv_scramble_data( pkt->data_frame, DV_DATA_FRM_SZ);

		// Accumulate data field
		memcpy( peer->dv_data_accumul + DV_DATA_FRM_SZ*(seq-1), pkt->data_frame, DV_DATA_FRM_SZ);
		peer->dv_rx_mask |= seq_mask;

		if( !(seq & 0x01) )
		{
			// Process S-Frame
			s_frame = (unsigned char *) peer->dv_data_accumul + (2*DV_DATA_FRM_SZ)*s_frame_offset;
			uint8_t s_type = s_frame[0] >> 4;
			size_t  s_arg = s_frame[0] & 0x0F;
			size_t  fastdata_block_sz = s_frame[0] - 0x80;

			fprintf( stderr, "S-Frame type %x, arg %lu\n", s_type, s_arg);

			switch( s_type )
			{
			case 3:	// Simple Data
				if( s_arg < 1 || s_arg > 5 ) break; // Inconsistent number of bytes
				utstring_bincpy( peer->reassembled_data, s_frame+1, s_arg);
				break;

			case 4:	// Message
				if( s_arg > 3 ) break; // Invalid message sequence
				memcpy(
					peer->rx_frame.message + 5*s_arg,
					s_frame+1,
					5 	// Size of a Message chunk
				);
				break;
			case 8:
			case 9: // Fast data
				if( s_frame_offset == 0 && fastdata_block_sz > 28 ) break;
				if( s_frame_offset != 0 && fastdata_block_sz > 20 ) break;
				peer->feat_flags |= DEXTRA_FEAT_FAST_DATA;
				int sz = (int) fastdata_block_sz;

				// Skip miniheader
				utstring_bincpy( peer->reassembled_data, s_frame+1, MIN( sz, 2));
				sz -= MIN( sz, 2); if ( sz <= 0 ) break;

				// Skip guard byte
				utstring_bincpy( peer->reassembled_data, s_frame+4, MIN( sz, 2));
				sz -= MIN( sz, 2); if ( sz <= 0 ) break;

				// First Audio frame
				utstring_bincpy(
					peer->reassembled_data,
					peer->rx_frame.ambe_data + (seq-1) * DV_AUDIO_FRM_SZ,
					MIN( sz, 4)
				);
				sz -= MIN( sz, 4); if ( sz <= 0 ) break;
				// Skip mitigation
				utstring_bincpy(
					peer->reassembled_data,
					peer->rx_frame.ambe_data + (seq-1) * DV_AUDIO_FRM_SZ + 5,
					MIN( sz, 4)
				);
				sz -= MIN( sz, 4); if ( sz <= 0 ) break;

				// Second Audio frame
				utstring_bincpy(
					peer->reassembled_data,
					peer->rx_frame.ambe_data +  seq * DV_AUDIO_FRM_SZ,
					MIN( sz, 4)
				);
				sz -= MIN( sz, 4); if ( sz <= 0 ) break;
				// Skip mitigation
				utstring_bincpy(
					peer->reassembled_data,
					peer->rx_frame.ambe_data +  seq * DV_AUDIO_FRM_SZ + 5,
					MIN( sz, 4)
				);
				sz -= MIN( sz, 4); if ( sz <= 0 ) break;

				// Third Audio frame
				utstring_bincpy(
					peer->reassembled_data,
					peer->rx_frame.ambe_data + (seq-2) * DV_AUDIO_FRM_SZ,
					MIN( sz, 4)
				);
				sz -= MIN( sz, 4); if ( sz <= 0 ) break;
				// Skip mitigation
				utstring_bincpy(
					peer->reassembled_data,
					peer->rx_frame.ambe_data + (seq-2) * DV_AUDIO_FRM_SZ + 5,
					MIN( sz, 4)
				);
				sz -= MIN( sz, 4); if ( sz <= 0 ) break;

				break;

			default:
				; // Drop Unsupported S-Frames
			}
		}
	}


	if( dv_last_frame( trunk_hdr) )
	{
		// Flush simple data buffer
		if( peer->rx_frame.simple_data_bytes)
		{
			utstring_bincpy(
				peer->reassembled_data,
				peer->rx_frame.simple_data,
				peer->rx_frame.simple_data_bytes
			);

		}

		// TODO: Parse and send packet.
		printf( "Message via %.7s%c: '%.20s'\n", peer->rpt1, peer->band, peer->rx_frame.message);
		printf( "Simple data payload: %lu byte(s)\n", utstring_len(peer->reassembled_data));
		fwrite( 
			utstring_body(peer->reassembled_data),
			utstring_len(peer->reassembled_data), 1, stdout
		);
		fflush(stdout);
		//parse( 
		//	...,
		//	utstring_body(peer->reassembled_data)),
		//	utstring_len(peer->reassembled_data))
		//);

		// Cleanup
		memset( peer->rx_frame.message, ' ', DSVT_MSG_SZ);
		utstring_clear( peer->reassembled_data);
		peer->rx_frame.simple_data_bytes = 0;

		// Medium Access Control
		peer->ifs_timer = DEXTRA_INTERFRAME_SPACE;
		peer->rx_idle = 1;
	}

	return 0;
}

int dextra_can_tx( dextra_peer_t *peer)
{
	return peer->rx_idle && peer->ifs_timer == 0;
}
