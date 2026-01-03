
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "kiss.h"

// Set up a new KISS connection
//   When port is set to 0, an AF_UNIX socket is used.
int kiss_new_connection( char *addr, uint16_t port)
{
	int sock_fd;
	struct sockaddr_un  server_addr_un;
	struct sockaddr_in6 server_addr_in6;
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
		sock_fd = socket( AF_INET, SOCK_STREAM, 0);
		if( sock_fd < 0 )
		{
			fprintf( stderr, "%s: Unable to create TCP socket, %s.\n", __FUNCTION__, strerror( errno));
			return -1;
		}


		
		if( connect( sock_fd, (struct sockaddr*) &server_addr_in6, sizeof( struct sockaddr_in6)) < 0 )
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
