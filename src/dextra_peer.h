#ifndef __DEXTRA_PEER_H
#define __DEXTRA_PEER_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include "dextra.h"

dextra_peer_t* dextra_peer_init( peer_key_t *lookup_key, struct sockaddr_in6 *client_addr, unsigned char *buffer);
void dextra_peer_destroy( dextra_peer_t* peer);
int dextra_peer_parse_pkt( dextra_peer_t *peer, dv_stream_pkt_t *pkt);
int dextra_can_tx( dextra_peer_t *peer);

#endif	/* __DEXTRA_PEER_H */
