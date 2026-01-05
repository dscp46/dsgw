#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uthash.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "dextra.h"
#include "dextra_peer.h"
#include "kiss.h"

void map_key_from_claddr( struct sockaddr_in6 *addr, peer_key_t *key);
void* dextra_keepalive_thread( void* argv);

int dextra_setup_socket( const char *addr)
{
	int sock_fd, errnum, addr_size = 0;
	struct sockaddr_in6 *server_addr = NULL;
	struct addrinfo hints, *res = NULL;
	const int off = 0;

	if( addr != NULL )
	{
		/* Use the user-supplied address */
		memset( &hints, 0, sizeof( hints));
		hints.ai_family   = AF_INET6;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags    = AI_ADDRCONFIG;

		if(( errnum = getaddrinfo( addr, NULL, &hints, &res)) != 0)
		{
			fprintf( stderr, "%s: Server addr resolution failed, %s\n", __FUNCTION__, gai_strerror( errnum));
			return -1;
		}

		server_addr            = (struct sockaddr_in6 *)res->ai_addr;
		server_addr->sin6_port = htons( DEXTRA_PORT);
	}
	else
	{
		/* Default settings: listen on loopback */
		addr_size   = sizeof( struct sockaddr_in6);
		server_addr = (struct sockaddr_in6*) malloc( addr_size);

		memset( server_addr, 0, addr_size);
		server_addr->sin6_family = AF_INET6;
		server_addr->sin6_addr   = in6addr_loopback;
		server_addr->sin6_port   = htons( DEXTRA_PORT);
	}

	if(( sock_fd = socket( AF_INET6, SOCK_DGRAM, 0)) < 0 )
	{
		fprintf( stderr, "%s: Unable to create socket.\n", __FUNCTION__);
		return -1;
	}

	if( setsockopt( sock_fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof( off)) < 0 )
	{
		fprintf( stderr, "%s: Unable to set socket in dual stack mode.\n", __FUNCTION__);
	}

	if( bind( sock_fd, (struct sockaddr *)server_addr, sizeof( struct sockaddr_in6)) < 0 )
	{
		fprintf( stderr, "%s: Unable to bind to %s on port %u\n", __FUNCTION__, addr, DEXTRA_PORT);
		return -1;
	}
	
	if( addr_size )
		free( server_addr);
	
	if( res != NULL )
		freeaddrinfo( res);

	return sock_fd;
}

void* dextra_server( void* argv)
{
	int err_bucket = 10;
	ssize_t len;
	unsigned char buffer[DEXTRA_MAX_PKT_SZ];
	pthread_t tid;

	struct sockaddr_in6 client_addr;
	socklen_t           client_len = sizeof( client_addr);

	const char ack[] = "ACK";

	dextra_peer_t *peer = NULL;
	peer_key_t lookup_key;

	if( argv == NULL )
		return NULL;

	dextra_server_args_t *args = (dextra_server_args_t*) argv;
	if((args->sock_fd = dextra_setup_socket( args->addr)) < 0 )
	{
		fprintf( stderr, "%s: Unable to run server.\n", __FUNCTION__);
		return NULL;
	}

	// Create keepalive thread
	if( pthread_create( &tid, NULL, dextra_keepalive_thread, args) < 0 )
	{
		fprintf( stderr, "%s: Failed to start keepalive thread.\n", __FUNCTION__);
		return NULL;
	}

	while( !args->shutdown)
	{
		len = recvfrom( args->sock_fd, buffer, DEXTRA_MAX_PKT_SZ, 0, (struct sockaddr *)&client_addr, &client_len);

		if( len < 0 )
		{
			if( len == EAGAIN || len == EWOULDBLOCK )
			{
				usleep(1);
				continue;
			}
			if( errno != EINTR )
				fprintf( stderr, "%s: recvfrom() failed (%d: %s).\n", __FUNCTION__, errno, strerror(errno));
			
			if( --err_bucket < 0 )
			{
				fprintf( stderr, "%s: Error bucket exhausted, giving up.\n", __FUNCTION__);
				break;
			}

			continue;
		}

		// Refill the error bucket
		err_bucket = 10;

		// Map the client addr to a searchable <ip,port> tuple
		map_key_from_claddr( &client_addr, &lookup_key);

		// Attempt to find an existing peer
		HASH_FIND( hh, args->peers, &lookup_key, sizeof( peer_key_t), peer);
		
		switch( len)
		{
		case DEXTRA_KEEPALIVE_SZ:
			// Keepalive packet
			if( peer == NULL ) break;
			peer->last_rx = 0;

			break;

		case DEXTRA_BIND_REQ_SZ:
			if( buffer[9] == ' ' || buffer[9] == 'Q' )
			{
				// Unbind request
				if( peer == NULL ) break;
				// TODO: Delete associated SIDs?
				
				// Delete peer
				HASH_DELETE( hh, args->peers, peer);
				dextra_peer_destroy( peer);
			}
			else
			{
				// Bind request
				if( peer == NULL)
				{
					// Add peer
					peer = dextra_peer_init( &lookup_key, &client_addr, buffer);
					
					// Peer initialization failure management
					if( peer == NULL )
					{
						fprintf( stderr, "%s: unable to allocate a peer.\n", __FUNCTION__);
						break;
					}

					peer->bound_module = buffer[9];
					HASH_ADD( hh, args->peers, key, sizeof( peer_key_t), peer);
				}

				// Send bind ACK regardless of registration status, to cover for packet loss.
				memcpy( buffer+10, ack, 4);
				sendto( args->sock_fd, buffer, DEXTRA_BIND_ANS_SZ, 0, (struct sockaddr *)&client_addr, client_len);
			}

			break;

		case DEXTRA_BIND_ANS_SZ:
			// Bind answer
			// Not implemented, since we don't support DExtra Adjuncts.
			break;

		case DEXTRA_STREAM_HDR_SZ:
			if( peer == NULL ) break;
			if( memcmp( buffer, "DSVT", 4) != 0 ) break; // Drop packets that don't match signature
			peer->last_rx = 0;
			break;

		case DEXTRA_STREAM_PKT_SZ:
			if( peer == NULL ) break;
			if( memcmp( buffer, "DSVT", 4) != 0 ) break; // Drop packets that don't match signature
			peer->last_rx = 0;
			break;

		default:
			// Ignore packet
			++(args->msg_errors);
		}
	}	

	fprintf( stderr, "%s: Shutting down...\n", __FUNCTION__);

	// Gracefully unbind all connected clients
	dextra_peer_t *cur_peer, *tmp;
	HASH_ITER( hh, args->peers, cur_peer, tmp)
	{
		HASH_DELETE( hh, args->peers, cur_peer);
		dextra_peer_destroy( cur_peer);
	}

	// TODO: Purge SIDs	

	close( args->sock_fd);
	return NULL;
}

void* dextra_keepalive_thread( void* argv)
{
	dextra_server_args_t *args = (dextra_server_args_t*) argv;
	dextra_peer_t *cur_peer, *tmp;

	while( !args->shutdown)
	{

		cur_peer = NULL;
		tmp = NULL;

		HASH_ITER( hh, args->peers, cur_peer, tmp)
		{
			if( ++(cur_peer->last_rx) > DEXTRA_DEAD_INTVL )
			{
				// Dead peer management
				HASH_DELETE( hh, args->peers, cur_peer);
				dextra_peer_destroy( cur_peer);
				continue;
			}

			if( --(cur_peer->ka_ttl) < 0 )
			{
				cur_peer->ka_ttl = DEXTRA_KA_INTVL;
				// Send keepalive
				sendto( 
					args->sock_fd, 
					args->xrf_name, 
					DEXTRA_KEEPALIVE_SZ, 
					0, 
					(struct sockaddr *)&cur_peer->addr, 
					sizeof( struct sockaddr_in6)
				);
			}
		}		
		sleep(1);
	}

	return NULL;
}

void dextra_server_args_init( dextra_server_args_t *args)
{
	if( args != NULL )
		return;

	args->addr = NULL;
	args->shutdown = 0;
	args->msg_errors = 0;
	args->peers = NULL;
	strncpy( args->xrf_name, "        ", sizeof args->xrf_name);
}

void map_key_from_claddr( struct sockaddr_in6 *addr, peer_key_t *key)
{
	if( addr == NULL || key == NULL )
		return;

	memcpy( &key->ip, &addr->sin6_addr, sizeof( struct in6_addr));
	key->port = addr->sin6_port;
}
