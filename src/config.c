#include "config.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define NULL_COALESCE(x,y) ( ( x != NULL ) ? x : y )

int str_to_uint16(const char *str, uint16_t *out)
{
    char *endptr;
    errno = 0;

    unsigned long value = strtoul(str, &endptr, 10);

    if (errno != 0) return -1;
    if (endptr == str) return -1;
    if (*endptr != '\0') return -1;
    if (value > UINT16_MAX) return -1;

    *out = (uint16_t)value;
    return 0;
}

void print_usage( char *argv[])
{
	printf( "Usage: %s [-h] [-l bind_address] [-n XRF###] [-k kiss_addr] [-p 8100]\n", argv[0]);
	printf( "  -h: Display this help prompt.\n");
	printf( "  -l: Set which address to bind to. Default: ::1\n");
	printf( "  -k: Set the remote kiss bind address. Default values:\n");
	printf( "       - ::ffff:127.0.0.1 for TCP.\n");
	printf(	"	- /run/vpsed/vpsed.sock for UNIX.\n");
	printf( "  -n: Set the XRF reflector name.\n");
	printf( "  -p: Set remote kiss port.\n");
	printf( "      When setting this to '0', kiss_addr becomes a UNIX socket path.\n");
	return;
}
int parse_cmdline_args( int argc, char *argv[], appconf_t *cfg)
{
	static const char      dfl_xrf_name[] = "XRFSGW  ";
	static const char     dfl_bind_addr[] = "::1";
	static const char     dfl_kiss_addr[] = "::ffff:127.0.0.1";
	static const char dfl_kiss_sockname[] = "/run/vpsed/vpsed.sock";
	int c;
	char *bind_addr = NULL;
	char *kiss_addr = NULL;
	char *kiss_port = NULL;
	char *xrf_name = NULL;

	if( argc < 2 || argv == NULL || cfg == NULL )
		return 1;

	while((c = getopt( argc, argv, "hk:l:n:p:" )) != -1 )
	{
		switch(c)
		{
		case 'h':
			print_usage( argv);
			return -1;

		case 'k':
			cfg->kiss_addr = optarg;
			break;

		case 'l':
			bind_addr = optarg;
			break;

		case 'n':
			xrf_name = optarg;
			break;

		case 'p':
			kiss_port = optarg;

		case '?':
			if( optopt == 'k' || optopt == 'l' || optopt == 'n' || optopt == 'p' )
			{
				fprintf( stderr, "Option -%c requires an argument.\n", optopt);
				return 2;
			}
			
			print_usage( argv);
			return -1;

		default:
			fprintf( stderr, "Invalid argument name.\n");
			return -1;
		}
	}

	cfg->xrf_name  = NULL_COALESCE(  xrf_name, dfl_xrf_name);
	cfg->bind_addr = NULL_COALESCE( bind_addr, dfl_bind_addr);

	if( kiss_port != NULL && !str_to_uint16( kiss_port, &cfg->kiss_port) )
	{
		fprintf( stderr, "Invalid port number.\n");
		return 3;
	}
	else
		cfg->kiss_port = 8100;

	if( cfg->kiss_port == 0 )
		cfg->kiss_addr = NULL_COALESCE( kiss_addr, dfl_kiss_sockname);
	else
		cfg->kiss_addr = NULL_COALESCE( kiss_addr, dfl_kiss_addr);

	return 0;
}

