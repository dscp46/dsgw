#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdint.h>

typedef struct appconf {
	const char  *xrf_name;
	const char *bind_addr;
	const char *kiss_addr;
	uint16_t    kiss_port;	
} appconf_t;

int parse_cmdline_args( int argc, char *argv[], appconf_t *cfg);

#endif	/* __CONFIG_H */
