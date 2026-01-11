#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "dextra_peer.h"
#include "kiss.h"

dextra_peer_t* dextra_peer_init( peer_key_t *lookup_key, struct sockaddr_in6 *client_addr, unsigned char *buffer)
{
	dextra_peer_t *peer = (dextra_peer_t*) malloc( sizeof( dextra_peer_t));

	if( peer == NULL )
		return NULL;

	// Setup connection, abort if unable
	if((peer->child_fd = kiss_new_connection( "::ffff:127.0.0.1", 8000)) < 1)
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
	peer->rpt1[9] = '\0';

	return peer;
}

void dextra_peer_destroy( dextra_peer_t* peer)
{
	if( peer == NULL )
		return;

	close( peer->child_fd);
	free( peer);
}

