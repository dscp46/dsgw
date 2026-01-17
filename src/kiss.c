
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "dextra.h"
#include "kiss.h"

/***** Internal functions  *****/
typedef struct kiss_queue_entry {
	struct kiss_queue_entry *prev;
	struct kiss_queue_entry *next;

	void         *packet;
	size_t        length;
	unsigned char ackmode_id[2];
	uint8_t       type;
} kiss_queue_entry_t;

kiss_queue_entry_t *kiss_queue_pop( kiss_queue_entry_t *self);
void kiss_queue_flush( kiss_queue_entry_t *self);
/*******************************/

// Set up a new KISS connection
//   When port is set to 0, an AF_UNIX socket is used.
int kiss_new_connection( const char *addr, uint16_t port)
{
	int sock_fd, errnum;
	struct sockaddr_un  server_addr_un;
	struct sockaddr_in6 *server_addr_in6 = NULL;
	struct addrinfo hints, *res = NULL;
	const char vpsed_path[] = "/run/vpsed/vpsed.sock";

	if( port == 0 )
	{
		sock_fd = socket( AF_UNIX, SOCK_STREAM, 0);
		if( sock_fd < 0 )
		{
			fprintf( stderr, "%s: Unable to create Unix socket, %s.\n", __FUNCTION__, strerror( errno));
			return -1;
		}

		memset( &server_addr_un, 0, sizeof(struct sockaddr_un));
		server_addr_un.sun_family = AF_UNIX;
		if( addr != NULL)
			strncpy( server_addr_un.sun_path,       addr, sizeof(server_addr_un.sun_path)-1);
		else
			strncpy( server_addr_un.sun_path, vpsed_path, sizeof(server_addr_un.sun_path)-1);

		if( connect( sock_fd, (struct sockaddr*) &server_addr_un, sizeof( struct sockaddr_un)) < 0 )
		{
			fprintf( 
				stderr, 
				"%s: Unable to connect to socket '%s', %s.\n", 
				__FUNCTION__, 
				( (addr!=NULL) ? addr : vpsed_path ),
				strerror( errno)
			);
			close( sock_fd);
			return -1;
		}
	}
	else
	{
		if( addr != NULL )
		{
			/* Use the user-supplied address */
			memset( &hints, 0, sizeof( hints));
			hints.ai_family   = AF_INET6;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags    = AI_ADDRCONFIG;

			if(( errnum = getaddrinfo( addr, NULL, &hints, &res)) != 0)
			{
				fprintf( stderr, "%s: KISS server addr resolution failed, %s\n", __FUNCTION__, gai_strerror( errnum));
				return -1;
			}

			server_addr_in6            = (struct sockaddr_in6 *)res->ai_addr;
			server_addr_in6->sin6_port = htons( port);
		}
		else
		{
			fprintf( stderr, "%s: KISS server not supplied.\n", __FUNCTION__);
			return -2;
		}
	
		sock_fd = socket( AF_INET6, SOCK_STREAM, 0);
		if( sock_fd < 0 )
		{
			fprintf( stderr, "%s: Unable to create TCP socket, %s.\n", __FUNCTION__, strerror( errno));
			return -1;
		}
		
		if( connect( sock_fd, (struct sockaddr*)server_addr_in6, sizeof( struct sockaddr_in6)) < 0 )
		{
			fprintf( 
				stderr, 
				"%s: Unable to connect to socket '%s', %s.\n", 
				__FUNCTION__, 
				( (addr!=NULL) ? addr : vpsed_path ),
				strerror( errno)
			);
			close( sock_fd);
			return -1;
		}
	}

	return sock_fd;
}

#define KISS_RXBUF_SZ	1360

void* kiss_receiver( void* argv)
{
	dextra_peer_t *peer = (dextra_peer_t *)argv;
	if( peer == NULL )
	{
		fprintf( stderr, "%s: Null pointer received.\n", __FUNCTION__);
		return NULL;
	}
	
	ssize_t ret_code;
	kiss_queue_entry_t *queue = NULL;
	char buffer[KISS_RXBUF_SZ];

	while( 1)
	{
		ret_code = read(
			peer->child_fd,
			buffer,
			KISS_RXBUF_SZ
		);

		if( ret_code < 0 )
		{
			if( ret_code == EAGAIN || ret_code == EWOULDBLOCK )
			{
				usleep(1);
				continue;
			}

			break;
		}

		if( ret_code < 3 ) continue; // Ignore runts

	}

	kiss_queue_flush( queue);

	return NULL;
}
kiss_queue_entry_t *kiss_queue_add( kiss_queue_entry_t *self, const void* buffer, size_t len)
{
	kiss_queue_entry_t *instance = (kiss_queue_entry_t*) malloc( sizeof( kiss_queue_entry_t));
	instance->packet = (unsigned char*) malloc( len);
	
	// TODO: Evaluate and decode frame
	
	memcpy( instance->packet, buffer, len);
	instance->length = len;

	instance->next = self;
	instance->prev = NULL;
	if( self != NULL )
		self->prev = instance;

	return instance;
}

kiss_queue_entry_t *kiss_queue_pop( kiss_queue_entry_t *self)
{
	kiss_queue_entry_t *entry = self;
	if( self == NULL ) return NULL;

	while( entry->next != NULL )
		entry = entry->next;
	
	entry->prev = NULL;
	return entry;
}

void kiss_queue_flush( kiss_queue_entry_t *self)
{
	kiss_queue_entry_t *entry = NULL;

	while((entry = kiss_queue_pop( self)) != NULL)
	{
		if( entry->packet != NULL )
			free( entry->packet);

		free( entry);
	}
}
