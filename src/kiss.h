#ifndef __KISS_H
#define __KISS_H

#include <stdint.h>

#define KISS_DEFAULT_PORT	8100

int kiss_new_connection( const char *addr, uint16_t port);

#endif	/* __KISS_H */
