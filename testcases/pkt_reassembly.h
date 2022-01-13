#include <stdint.h>

typedef struct {
    uint32_t dPort_sPort;
    uint64_t dIP_sIP;
} tuple_t;

// typedef struct {
//     uint56_t last_7_bytes;
//     uint2_t pdu_flag;
//     uint3_t pkt_flags;
//     uint9_t tcp_flags;
//     uint9_t hdr_len;
//     uint5_t flits;
//     uint6_t empty;
//     uint10_t pktID;
//     uint16_t len;
//     uint32_t seq;
//     tuple_t tuple;
//     uint8_t prot;
// } metadata_t;

// typedef struct {
//     uint12_t addr3;
//     uint12_t addr2;
//     uint12_t addr1;
//     uint12_t addr0;
//     uint56_t last_7_bytes;
//     uint10_t slow_cnt;
//     uint1_t ll_valid;
//     uint9_t pointer;
//     uint32_t seq;
//     tuple_t tuple;
// } fce_t;

typedef struct {
    uint64_t pdu_flag_last_7_bytes;
    uint8_t pkt_flags;
    uint16_t tcp_flags;
    uint32_t pktID_empty_flits_hdr_len;
    uint16_t len;
    uint32_t seq;
    tuple_t tuple;
    uint8_t prot;
} metadata_t;

typedef struct {
    uint8_t ch0_bit_map;
    uint16_t pointer2;
    uint64_t addr0_addr2_addr2_addr3;
    uint64_t last_7_bytes;
    uint16_t slow_cnt;
    uint16_t pointer;
    uint32_t seq;
    tuple_t tuple;
} fce_t;

typedef struct {
    metadata_t meta;
    uint16_t next;
} dymem_t;