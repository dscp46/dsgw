#ifndef __DV_FRAME_H
#define __DV_FRAME_H

#include <assert.h>
#include <stdint.h>

// AMBE data: 21 * 9 bytes
#define DSVT_AMBE_SZ	20

// DV Message Size
#define DSVT_MSG_SZ	20

// 10 S-Frames of up to 5 bytes
#define	DSVT_SD_SZ	50


typedef struct dv_frame_s {
	uint16_t        stream_id;
	unsigned char   ambe_data[ DSVT_AMBE_SZ];
	unsigned char     message[ DSVT_MSG_SZ];
	unsigned char simple_data[ DSVT_SD_SZ];
	size_t  simple_data_bytes;
} dv_frame;

typedef struct __attribute__((packed)) dv_trunk_hdr {
	uint8_t type : 2;
	uint8_t rsvd : 6;
	uint8_t dst_rpt_id;
	uint8_t src_rpt_id;
	uint8_t src_term_id;
	uint16_t call_id;
	uint8_t mgmt_info;
} dv_trunk_hdr_t;

typedef struct __attribute__((packed)) dv_stream_conf_hdr {
	// WARNING: strings are not null-terminated.
	char signature[4];
	uint16_t flag;
	uint16_t rsvd;
	dv_trunk_hdr_t trunk_hdr;
	char flag1[3];
	char flag2[3];
	char flag3[3];
	char rpt1[8];
	char rpt2[8];
	char   ur[8];
	char   my[12];
	uint16_t checksum;
} dv_stream_conf_hdr_t;

_Static_assert( sizeof( dv_trunk_hdr_t) == 7, "struct dv_trunk_hdr_t isn't properly packed!" );
_Static_assert( sizeof( dv_stream_conf_hdr_t) == 56, "struct dv_stream_conf_hdr_t isn't properly packed!");

#endif	// __DV_FRAME_H
