#ifndef __DEXTRA_H
#define __DEXTRA_H

#include <stddef.h>
#include <uthash.h>
#include <arpa/inet.h>

#include "dv_frame.h"

#define DEXTRA_PORT		30001

#define DEXTRA_KEEPALIVE_SZ	9
#define DEXTRA_BIND_REQ_SZ	11
#define DEXTRA_BIND_ANS_SZ	14
#define DEXTRA_STREAM_PKT_SZ	DV_STREAM_PKT_SZ
#define DEXTRA_STREAM_HDR_SZ	DV_STREAM_HDR_SZ
#define DEXTRA_MAX_PKT_SZ	DEXTRA_STREAM_HDR_SZ+1

#define DEXTRA_KA_INTVL		10
#define DEXTRA_DEAD_INTVL	35
#define DEXTRA_INTERFRAME_SPACE	2

typedef struct peer_key {
    struct in6_addr ip;
    uint16_t port;
} peer_key_t;

typedef struct dextra_peer {
	peer_key_t key;
	struct sockaddr_in6 addr;
	char   rpt1[9];
	int child_fd;	// KISS socket fd
	int last_rx;	// Seconds since last frame rx, expires by exceeding threshold
	int ka_ttl;	// Keep alive timer, expires at 0
	int ifs_timer;  // Interframe Space, to wait for the repeater sendin ACK 
			// and for remote radio to turn around.
	int rx_idle;	// Used to enforce half duplex.
	char band; 
	char bound_module; 
	dv_frame_t rx_frame;
	char dv_data_accumul[3];
	UT_hash_handle hh;
} dextra_peer_t;

typedef struct dextra_server_args {
	const char *addr;
	const char *kiss_addr;
	uint16_t kiss_port;
	int shutdown;
	int sock_fd;
	size_t msg_errors;
	char xrf_name[9];
	dextra_peer_t *peers;
} dextra_server_args_t;

void dextra_server_args_init( dextra_server_args_t *args);
void* dextra_server( void* argv);

#endif	/* __DEXTRA_H */
