#ifndef __DV_FRAME_H
#define __DV_FRAME_H

#include <sys/socket.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

// AMBE data: 21 * 9 bytes
#define DSVT_AMBE_SZ	189

// DV Message Size
#define DSVT_MSG_SZ	20

// 10 S-Frames of up to 5 bytes
#define	DSVT_SD_SZ	50

// Sizes
#define DV_TRUNK_HDR_SZ		 7
#define DV_RADIO_HDR_SZ		41
#define DV_STREAM_HDR_SZ	56
#define DV_STREAM_PKT_SZ	27
#define DV_AUDIO_FRM_SZ		 9
#define DV_DATA_FRM_SZ		 3

// Bit masks
#define DV_TRUNK_LAST_FRAME	0x40
#define DV_TRUNK_HEALTH_FLAG	0x20
#define DV_TRUNK_SEQ_MASK	0x1F

typedef struct dv_frame {
	uint16_t        stream_id;
	unsigned char   ambe_data[ DSVT_AMBE_SZ];
	char            message[ DSVT_MSG_SZ];
	unsigned char simple_data[ DSVT_SD_SZ];
	size_t  simple_data_bytes;
} dv_frame_t;

typedef struct __attribute__((packed)) dv_trunk_hdr {
	uint8_t type : 2;
	uint8_t rsvd : 6;
	uint8_t dst_rpt_id;
	uint8_t src_rpt_id;
	uint8_t src_term_id;
	uint16_t call_id;
	uint8_t mgmt_info;
} dv_trunk_hdr_t;

typedef struct __attribute__((packed)) dv_radio_hdr {
	uint8_t flag1; // フラグ１
	uint8_t flag2; // フラグ２
	uint8_t flag3; // フラグ３
	char rpt1[8];
	char rpt2[8];
	char ur[8];   // UR callsign
	char my[12];  // MY callsign, with 4 byte identifier
	uint16_t p_fcs;
} dv_radio_hdr_t;

typedef struct __attribute__((packed)) dv_stream_hdr {
	// WARNING: strings are not null-terminated.
	char signature[4];
	uint16_t flag;
	uint16_t rsvd;
	char trunk_hdr[DV_TRUNK_HDR_SZ];
	char radio_hdr[DV_RADIO_HDR_SZ];
} dv_stream_hdr_t;

typedef struct __attribute__((packed)) dv_stream_pkt {
	char signature[4];
	uint16_t flag;
	uint16_t rsvd;
	char trunk_hdr[DV_TRUNK_HDR_SZ];
	char audio_frame[DV_AUDIO_FRM_SZ];
	char data_frame[DV_DATA_FRM_SZ];
} dv_stream_pkt_t;

_Static_assert( sizeof( dv_radio_hdr_t)  == DV_RADIO_HDR_SZ,  "struct dv_radio_hdr_t isn't properly packed!" );
_Static_assert( sizeof( dv_trunk_hdr_t)  == DV_TRUNK_HDR_SZ,  "struct dv_trunk_hdr_t isn't properly packed!" );
_Static_assert( sizeof( dv_stream_hdr_t) == DV_STREAM_HDR_SZ, "struct dv_stream_hdr_t isn't properly packed!");
_Static_assert( sizeof( dv_stream_pkt_t) == DV_STREAM_PKT_SZ, "struct dv_stream_pkt_t isn't properly packed!");

int dv_radio_invalid_csum ( dv_radio_hdr_t *hdr);
int dv_last_frame( dv_trunk_hdr_t *hdr);
void dv_scramble_data( void *buffer, size_t len);
void dv_insert_beep( int fd, struct sockaddr *addr, size_t addr_len, uint16_t stream_id, uint8_t *seq);
void dv_insert_eot( int fd, struct sockaddr *addr, size_t addr_len, uint16_t stream_id, uint8_t *seq);

#endif	// __DV_FRAME_H
