#ifndef _P2P_H
#define _P2P_H

#include "types.h"
#include "stddef.h"

#define P2P_MAGIC               0x4B49544BU /* "KITK" */
#define P2P_DEFAULT_PORT        8333
#define P2P_MAX_PEERS           8

#define P2P_MSG_HELLO           1
#define P2P_MSG_TX              2
#define P2P_MSG_BLOCK           3
#define P2P_MSG_GET_BLOCKS      4

struct p2p_header {
    uint32_t magic;
    uint16_t msg_type;
    uint16_t payload_len;
    uint32_t checksum;
} __attribute__((packed));

struct p2p_peer {
    char node_id[24];
    char address[16];
    uint32_t port;
    uint32_t best_height;
    int is_connected;
    uint64_t packets_sent;
    uint64_t packets_recv;
};

/* Forward declarations */
struct coin_tx;
struct coin_block;

void p2p_init(void);
void p2p_broadcast_tx(const struct coin_tx *tx);
void p2p_broadcast_block(const struct coin_block *b);
void p2p_broadcast_block_quiet(const struct coin_block *b);
void p2p_dump_status(void);
int  p2p_get_peer_count(void);
int  p2p_add_peer(const char *node_id, const char *address, uint32_t port);
void p2p_ping_peers(void);
void p2p_sync(void);
void p2p_simulate_traffic(void);
struct p2p_peer *p2p_get_peer_by_index(int idx);

#endif /* _P2P_H */
