#ifndef __DEXTRA_H
#define __DEXTRA_H

#include <stddef.h>
#include <uthash.h>
#include <arpa/inet.h>

#define DEXTRA_PORT		30001

#define DEXTRA_KEEPALIVE_SZ	9
#define DEXTRA_BIND_REQ_SZ	11
#define DEXTRA_BIND_ANS_SZ	14
#define DEXTRA_STREAM_PKT_SZ	27
#define DEXTRA_STREAM_HDR_SZ	56
#define DEXTRA_MAX_PKT_SZ	DEXTRA_STREAM_HDR_SZ+1

#define DEXTRA_KA_INTVL		3
#define DEXTRA_DEAD_INTVL	10

typedef struct peer_key {
    struct in6_addr ip;
    uint16_t port;
} peer_key_t;

typedef struct dextra_peer {
	peer_key_t key;
	struct sockaddr_in6 addr;
	char   rpt1[9];
	int child_fd;
	int last_rx;
	int ka_ttl;
	char bound_module;
	UT_hash_handle hh;
} dextra_peer_t;

typedef struct dextra_server_args {
	char *addr;
	int shutdown;
	int sock_fd;
	size_t msg_errors;
	char xrf_name[9];
	dextra_peer_t *peers;
} dextra_server_args_t;

void dextra_server_args_init( dextra_server_args_t *args);
void* dextra_server( void* argv);

#endif	/* __DEXTRA_H */
