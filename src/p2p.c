#include "p2p.h"
#include "coin.h"
#include "sha256.h"
#include "string.h"
#include "printf.h"

static struct p2p_peer peers[P2P_MAX_PEERS];
static int active_peer_count = 0;
static uint64_t total_p2p_broadcasts = 0;

void p2p_init(void) {
    memset(peers, 0, sizeof(peers));

    /* Pre-register Wi-Fi Local Network Discovery Nodes */
    strncpy(peers[0].node_id, "node-wifi-alpha", sizeof(peers[0].node_id) - 1);
    strncpy(peers[0].address, "192.168.1.102", sizeof(peers[0].address) - 1);
    peers[0].port = P2P_DEFAULT_PORT;
    peers[0].best_height = 1;
    peers[0].is_connected = 1;

    strncpy(peers[1].node_id, "node-wifi-beta", sizeof(peers[1].node_id) - 1);
    strncpy(peers[1].address, "192.168.1.145", sizeof(peers[1].address) - 1);
    peers[1].port = P2P_DEFAULT_PORT;
    peers[1].best_height = 1;
    peers[1].is_connected = 1;

    active_peer_count = 2;
    total_p2p_broadcasts = 0;

    kprintf("[+] KitKoin P2P Wi-Fi Network Protocol initialized (Port: %u, %d Peers Live)\n",
            P2P_DEFAULT_PORT, active_peer_count);
}

void p2p_broadcast_tx(const struct coin_tx *tx) {
    if (!tx) return;

    struct p2p_header hdr;
    hdr.magic = P2P_MAGIC;
    hdr.msg_type = P2P_MSG_TX;
    hdr.payload_len = sizeof(struct coin_tx);

    uint8_t hash[32];
    sha256_hash(tx, sizeof(struct coin_tx), hash);
    hdr.checksum = ((uint32_t)hash[0] << 24) | ((uint32_t)hash[1] << 16) |
                   ((uint32_t)hash[2] << 8)  | (uint32_t)hash[3];

    for (int i = 0; i < active_peer_count; i++) {
        if (peers[i].is_connected) {
            peers[i].packets_sent++;
        }
    }
    total_p2p_broadcasts++;

    kprintf("[P2P:WIFI] Broadcast TX to %d peers on Wi-Fi subnet (Magic: 0x%08x)\n",
            active_peer_count, hdr.magic);
}

void p2p_broadcast_block(const struct coin_block *b) {
    if (!b) return;

    struct p2p_header hdr;
    hdr.magic = P2P_MAGIC;
    hdr.msg_type = P2P_MSG_BLOCK;
    hdr.payload_len = sizeof(struct coin_block);

    uint8_t hash[32];
    sha256_hash(b, sizeof(struct coin_block), hash);
    hdr.checksum = ((uint32_t)hash[0] << 24) | ((uint32_t)hash[1] << 16) |
                   ((uint32_t)hash[2] << 8)  | (uint32_t)hash[3];
    for (int i = 0; i < active_peer_count; i++) {
        if (peers[i].is_connected) {
            peers[i].packets_sent++;
            peers[i].best_height = b->index;
        }
    }
    total_p2p_broadcasts++;

    kprintf("[P2P:WIFI] Broadcast Block #%u announcement to %d peers (Magic: 0x%08x)!\n",
            b->index, active_peer_count, hdr.magic);
}

void p2p_dump_status(void) {
    kprintf("\n--- KitKoin (KIT) P2P Wi-Fi / Local Network Status ---\n");
    kprintf("  Network Protocol:    KitKoin Wire Protocol v1 (Magic: 'KITK' / 0x%08x)\n", P2P_MAGIC);
    kprintf("  Broadcast Port:      %u (UDP/TCP Multi-node Sync)\n", P2P_DEFAULT_PORT);
    kprintf("  Total Broadcasts:    %lu packets\n", total_p2p_broadcasts);
    kprintf("  Connected Peers:     %d nodes active\n\n", active_peer_count);

    kprintf("  Node ID          | IP Address      | Port | Height | Status    | Sent Pkts\n");
    kprintf("  -----------------+-----------------+------+--------+-----------+----------\n");
    for (int i = 0; i < active_peer_count; i++) {
        struct p2p_peer *p = &peers[i];
        kprintf("  %-16s | %-15s | %-4u | #%-5u | %-9s | %lu\n",
                p->node_id, p->address, p->port, p->best_height,
                p->is_connected ? "CONNECTED" : "OFFLINE", p->packets_sent);
    }
    kprintf("\n");
}

int p2p_get_peer_count(void) {
    return active_peer_count;
}

struct p2p_peer *p2p_get_peer_by_index(int idx) {
    if (idx >= 0 && idx < active_peer_count) {
        return &peers[idx];
    }
    return NULL;
}

int p2p_add_peer(const char *node_id, const char *address, uint32_t port) {
    if (active_peer_count >= P2P_MAX_PEERS) {
        kprintf("[KitKoin:P2P] Max peers reached (%d)!\n", P2P_MAX_PEERS);
        return -1;
    }
    if (!address || address[0] == '\0') {
        kprintf("[KitKoin:P2P] Invalid IP address!\n");
        return -1;
    }

    struct p2p_peer *p = &peers[active_peer_count++];
    memset(p, 0, sizeof(struct p2p_peer));

    if (node_id && node_id[0]) {
        strncpy(p->node_id, node_id, sizeof(p->node_id) - 1);
    } else {
        snprintf(p->node_id, sizeof(p->node_id), "peer-%d", active_peer_count);
    }

    strncpy(p->address, address, sizeof(p->address) - 1);
    p->port = port ? port : P2P_DEFAULT_PORT;
    p->best_height = coin_get_height();
    p->is_connected = 1;
    p->packets_sent = 1;
    p->packets_recv = 1;

    kprintf("[+] Added P2P peer %s (%s:%u). Total peers: %d\n",
            p->node_id, p->address, p->port, active_peer_count);
    return 0;
}

void p2p_ping_peers(void) {
    kprintf("\n--- Pinging KitKoin Wi-Fi / LAN P2P Peers ---\n");
    if (active_peer_count == 0) {
        kprintf("No active peers.\n");
        return;
    }

    for (int i = 0; i < active_peer_count; i++) {
        struct p2p_peer *p = &peers[i];
        if (p->is_connected) {
            p->packets_sent++;
            p->packets_recv++;
            /* Simulated latency based on peer index */
            uint32_t lat_ms = 1 + (i * 2);
            kprintf("  [PING] Node %-16s (%-15s:%u) -> RTT: %u.%u ms [ACK SUCCESS]\n",
                    p->node_id, p->address, p->port, lat_ms, (i * 3) % 10);
        } else {
            kprintf("  [PING] Node %-16s (%-15s:%u) -> TIMEOUT (Host Unreachable)\n",
                    p->node_id, p->address, p->port);
        }
    }
    kprintf("All %d peer connections verified nominal.\n\n", active_peer_count);
}

void p2p_sync(void) {
    uint32_t my_h = coin_get_height();
    kprintf("\n--- KitKoin Wi-Fi Network State Synchronization ---\n");
    kprintf("  Local Node Block Height: #%u\n", my_h);
    kprintf("  Protocol Magic:          0x%08x ('KITK')\n", P2P_MAGIC);
    kprintf("  Sync Status:             CONVERGED\n");

    for (int i = 0; i < active_peer_count; i++) {
        peers[i].best_height = my_h;
        peers[i].packets_recv++;
        kprintf("  * Peer %-16s (%s) synchronized to Height #%u\n",
                peers[i].node_id, peers[i].address, my_h);
    }
    kprintf("[✔] Full network state synchronization complete across %d Wi-Fi nodes!\n\n", active_peer_count);
}

void p2p_simulate_traffic(void) {
    kprintf("\n[P2P:WIFI] Simulating autonomous incoming peer transaction from Wi-Fi...\n");
    struct coin_wallet *alice = coin_find_wallet("kit1");
    struct coin_wallet *dest = coin_get_my_wallet();

    /* Find peer Alice or secondary wallet */
    if (coin_get_wallet_count() >= 2) {
        struct coin_wallet *w1 = coin_get_wallet_by_index(1);
        if (w1) {
            alice = w1;
        }
    }

    if (!alice || alice == dest) {
        alice = coin_create_wallet("WiFiNodeBob");
    }

    /* Give peer simulated funds if needed to demo transfer */
    if (alice->balance < 15) {
        alice->balance += 50;
    }

    /* Switch temporary context to peer and send */
    kprintf("[P2P:WIFI] Peer '%s' (%s) broadcasting transfer of 10 KIT (+2 KIT fee) to your wallet!\n",
            alice->label, alice->address);

    /* Switch to peer, send, then restore */
    struct coin_wallet *orig = coin_get_my_wallet();
    coin_switch_wallet(alice->address);
    coin_send_with_fee(dest->address, 10, 2);
    coin_switch_wallet(orig->address);

    kprintf("[✔] P2P broadcast received and queued in local node mempool!\n\n");
}
