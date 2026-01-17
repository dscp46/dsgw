#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

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

	return peer;
}

void dextra_peer_destroy( dextra_peer_t* peer)
{
	if( peer == NULL )
		return;

	close( peer->child_fd);
	free( peer);
}

int dextra_peer_parse_pkt( dextra_peer_t *peer, dv_stream_pkt_t *pkt)
{
	if( peer == NULL || pkt == NULL ) return EINVAL;
	dv_trunk_hdr_t *trunk_hdr = (dv_trunk_hdr_t*) pkt->trunk_hdr;
	uint16_t sid = ntohs( trunk_hdr->call_id);
	uint8_t  seq = trunk_hdr->mgmt_info & 0x1F; // 00h~14h 

	if( peer->rx_frame.stream_id != sid) return EPROTO; // SID mismatch


	if( seq == 0x00 )
	{
		// Sync 
	}
	else if( seq & 0x01 )
	{
		// Odd sequence number
		// Descramble and accumulate
	}
	else 
	{
		// Even sequence number
		// Descramble, accumulate, and parse
	}


	if( dv_last_frame( trunk_hdr) )
	{
		// TODO: Parse and send packet.

		// Medim Access Control
		peer->ifs_timer = DEXTRA_INTERFRAME_SPACE;
		peer->rx_idle = 1;
	}

	return 0;
}

int dextra_can_tx( dextra_peer_t *peer)
{
	return peer->rx_idle && peer->ifs_timer == 0;
}
