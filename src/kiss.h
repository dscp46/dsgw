#ifndef __KISS_H
#define __KISS_H

#include <stdint.h>

#define KISS_DEFAULT_PORT	8100

#define KISS_CMD_MASK		0x0F

#define KISS_CMD_DATA		0x00
#define KISS_CMD_ACKMODE	0x0C

int kiss_new_connection( const char *addr, uint16_t port);
void* kiss_receiver( void* argv);

#endif	/* __KISS_H */
