#include "coin.h"
#include "sha256.h"
#include "string.h"
#include "printf.h"
#include "timer.h"
#include "kernel.h"
#include "sched.h"
#include "p2p.h"
#include "virtio.h"
#include "ramfs.h"
#include "mm.h"

#define COIN_DISK_MAGIC        0x4B49544B4F494E31ULL /* "KITKOIN1" */
#define COIN_DISK_VERSION      2
#define COIN_DISK_START_SECTOR 16

struct coin_wallet_v1 {
    char address[COIN_ADDR_LEN];
    char label[32];
    uint64_t balance;
    uint32_t tx_count;
    int is_active;
};

struct coin_disk_state_v1 {
    uint64_t magic;
    uint32_t version;
    uint32_t wallet_count;
    uint32_t my_wallet_index;
    struct coin_wallet_v1 wallets[COIN_MAX_WALLETS];
    uint32_t chain_height;
    struct coin_block chain[COIN_MAX_CHAIN_LEN];
    uint32_t mempool_count;
    struct coin_tx mempool[COIN_MAX_BLOCK_TX];
    uint32_t checksum;
};

struct coin_disk_state {
    uint64_t magic;
    uint32_t version;
    uint32_t wallet_count;
    uint32_t my_wallet_index;
    struct coin_wallet wallets[COIN_MAX_WALLETS];
    uint32_t chain_height;
    struct coin_block chain[COIN_MAX_CHAIN_LEN];
    uint32_t mempool_count;
    struct coin_tx mempool[COIN_MAX_BLOCK_TX];
    uint32_t checksum;
} __attribute__((packed));

static struct coin_disk_state disk_io_buffer;

static struct coin_block **chain = NULL;
static uint32_t chain_capacity = 0;
static uint32_t chain_height = 0;
static uint8_t last_block_hash[32];

static int ensure_chain_capacity(uint32_t needed) {
    if (needed <= chain_capacity && chain != NULL) {
        return 0;
    }
    uint32_t new_cap = (chain_capacity == 0) ? 64 : chain_capacity * 2;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    struct coin_block **new_chain = (struct coin_block **)kmalloc(new_cap * sizeof(struct coin_block *));
    if (!new_chain) {
        kprintf("[KitKoin:ERR] Out of memory expanding blockchain table to %u capacity!\n", new_cap);
        return -1;
    }
    memset(new_chain, 0, new_cap * sizeof(struct coin_block *));
    if (chain && chain_capacity > 0) {
        memcpy(new_chain, chain, chain_capacity * sizeof(struct coin_block *));
        kfree(chain);
    }
    chain = new_chain;
    chain_capacity = new_cap;
    return 0;
}

static struct coin_block *allocate_new_block(uint32_t height) {
    if (ensure_chain_capacity(height + 1) != 0) {
        return NULL;
    }
    if (chain[height] == NULL) {
        chain[height] = (struct coin_block *)kmalloc(sizeof(struct coin_block));
        if (!chain[height]) {
            kprintf("[KitKoin:ERR] Failed to allocate memory for block #%u!\n", height);
            return NULL;
        }
    }
    memset(chain[height], 0, sizeof(struct coin_block));
    return chain[height];
}

static struct coin_tx mempool[COIN_MAX_BLOCK_TX];
static uint32_t mempool_count = 0;

static struct coin_wallet wallets[COIN_MAX_WALLETS];
static uint32_t wallet_count = 0;
static int my_wallet_index = 0;

/* Helper to compute block hash from header fields */
static void compute_block_hash(const struct coin_block *b, uint8_t hash[32]) {
    struct {
        uint32_t index;
        uint64_t timestamp;
        uint8_t prev_hash[32];
        uint8_t merkle_root[32];
        uint32_t nonce;
        uint32_t difficulty;
        uint64_t reward;
        uint64_t total_fees;
        uint32_t tx_count;
    } __attribute__((packed)) header;

    header.index = b->index;
    header.timestamp = b->timestamp;
    memcpy(header.prev_hash, b->prev_hash, 32);
    memcpy(header.merkle_root, b->merkle_root, 32);
    header.nonce = b->nonce;
    header.difficulty = b->difficulty;
    header.reward = b->reward;
    header.total_fees = b->total_fees;
    header.tx_count = b->tx_count;

    sha256_double(&header, sizeof(header), hash);
}

static void compute_merkle_root(const struct coin_tx *txs, uint32_t count, uint8_t root[32]) {
    if (count == 0) {
        memset(root, 0, 32);
        return;
    }

    uint8_t hashes[COIN_MAX_BLOCK_TX][32];
    for (uint32_t i = 0; i < count; i++) {
        memcpy(hashes[i], txs[i].tx_hash, 32);
    }

    /* Hash concatenation of transactions */
    struct sha256_ctx ctx;
    sha256_init(&ctx);
    for (uint32_t i = 0; i < count; i++) {
        sha256_update(&ctx, hashes[i], 32);
    }
    sha256_final(&ctx, root);
}

static int meets_difficulty(const uint8_t hash[32], uint32_t diff) {
    if (diff <= 1) {
        return (hash[0] & 0xF0) == 0;
    } else if (diff == 2) {
        return hash[0] == 0;
    } else if (diff == 3) {
        return (hash[0] == 0) && ((hash[1] & 0xF0) == 0);
    } else if (diff >= 4) {
        return (hash[0] == 0) && (hash[1] == 0);
    }
    return 1;
}

uint64_t coin_get_block_reward(uint32_t height) {
    uint32_t halvings = height / COIN_HALVING_INTERVAL;
    if (halvings >= 64) return 1ULL;
    uint64_t reward = COIN_INITIAL_REWARD >> halvings;
    return (reward > 0) ? reward : 1ULL;
}

uint32_t coin_get_blocks_until_halving(void) {
    uint32_t next = ((chain_height / COIN_HALVING_INTERVAL) + 1) * COIN_HALVING_INTERVAL;
    return (next > chain_height) ? (next - chain_height) : 0;
}

uint32_t coin_get_difficulty(void) {
    if (chain_height == 0) return 2;
    return COIN_DEFAULT_DIFFICULTY;
}

struct coin_wallet *coin_find_wallet(const char *address) {
    if (!address) return NULL;
    for (uint32_t i = 0; i < wallet_count; i++) {
        if (wallets[i].is_active && strcmp(wallets[i].address, address) == 0) {
            return &wallets[i];
        }
    }
    return NULL;
}

struct coin_wallet *coin_create_wallet(const char *label) {
    if (wallet_count >= COIN_MAX_WALLETS) {
        return NULL;
    }

    struct coin_wallet *w = &wallets[wallet_count++];
    memset(w, 0, sizeof(struct coin_wallet));

    /* Generate address: kit1 + 16 hex chars derived from timestamp & index */
    uint8_t rand_bytes[32];
    uint64_t ticks = timer_get_ticks();
    uint64_t seed = ticks ^ ((uint64_t)wallet_count << 32) ^ read_midr();
    sha256_hash(&seed, sizeof(seed), rand_bytes);

    char hex[65];
    sha256_to_hex(rand_bytes, hex);

    strncpy(w->address, "kit1", 5);
    strncat(w->address, hex, 12);
    w->address[COIN_ADDR_LEN - 1] = '\0';

    if (label) {
        strncpy(w->label, label, sizeof(w->label) - 1);
    } else {
        strncpy(w->label, "PeerWallet", sizeof(w->label) - 1);
    }

    w->balance = 0;
    w->tx_count = 0;
    w->is_active = 1;

    if (chain_height > 0) {
        coin_save();
    }

    return w;
}

static int str_ieq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

static int edit_distance(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    int dp[33][33];
    if (len1 > 32) len1 = 32;
    if (len2 > 32) len2 = 32;
    for (int i = 0; i <= len1; i++) dp[i][0] = i;
    for (int j = 0; j <= len2; j++) dp[0][j] = j;
    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            char c1 = (s1[i-1] >= 'A' && s1[i-1] <= 'Z') ? (s1[i-1] + 32) : s1[i-1];
            char c2 = (s2[j-1] >= 'A' && s2[j-1] <= 'Z') ? (s2[j-1] + 32) : s2[j-1];
            int cost = (c1 == c2) ? 0 : 1;
            int del = dp[i-1][j] + 1;
            int ins = dp[i][j-1] + 1;
            int sub = dp[i-1][j-1] + cost;
            int min = del < ins ? del : ins;
            dp[i][j] = min < sub ? min : sub;
        }
    }
    return dp[len1][len2];
}

int coin_remove_wallet(const char *target) {
    if (!target) return -1;
    if (wallet_count == 0) return -1;

    int match_idx = -1;
    int best_dist = 99;

    /* 1. Try exact address or label match (case-insensitive) */
    for (uint32_t i = 0; i < wallet_count; i++) {
        if (!wallets[i].is_active) continue;
        if (strcmp(wallets[i].address, target) == 0 || str_ieq(wallets[i].label, target)) {
            match_idx = (int)i;
            break;
        }
    }

    /* 2. If no exact match, try typo-tolerant match (edit distance <= 2) */
    if (match_idx < 0) {
        for (uint32_t i = 0; i < wallet_count; i++) {
            if (!wallets[i].is_active) continue;
            int d = edit_distance(wallets[i].label, target);
            if (d <= 2 && d < best_dist) {
                best_dist = d;
                match_idx = (int)i;
            }
        }
    }

    if (match_idx < 0) {
        kprintf("[KitKoin:ERR] Wallet profile '%s' not found!\n", target);
        return -1;
    }

    /* Prevent deleting if it's the last remaining wallet */
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < wallet_count; i++) {
        if (wallets[i].is_active) active_count++;
    }
    if (active_count <= 1) {
        kprintf("[KitKoin:ERR] Cannot remove the sole remaining wallet profile (%s)!\n",
                wallets[match_idx].label);
        return -1;
    }

    char removed_label[32];
    char removed_addr[COIN_ADDR_LEN];
    uint64_t removed_bal = wallets[match_idx].balance;
    strncpy(removed_label, wallets[match_idx].label, sizeof(removed_label) - 1);
    strncpy(removed_addr, wallets[match_idx].address, sizeof(removed_addr) - 1);

    wallets[match_idx].is_active = 0;

    /* If removing currently active wallet, switch to first available active wallet */
    if ((uint32_t)match_idx == (uint32_t)my_wallet_index) {
        for (uint32_t i = 0; i < wallet_count; i++) {
            if (wallets[i].is_active) {
                my_wallet_index = (int)i;
                break;
            }
        }
    }

    coin_save();

    kprintf("[+] Successfully removed KitKoin wallet profile!\n");
    kprintf("    Label:     %s\n", removed_label);
    kprintf("    Address:   %s\n", removed_addr);
    if (removed_bal > 0) {
        kprintf("    Note:      Wallet contained %lu KIT.\n", removed_bal);
    }
    kprintf("    Active:    Switched to %s (%s)\n",
            wallets[my_wallet_index].address, wallets[my_wallet_index].label);
    return 0;
}

int coin_switch_wallet(const char *address) {
    if (!address) return -1;
    int match_idx = -1;
    int best_dist = 99;

    /* 1. Try exact address or label match (case-insensitive) */
    for (uint32_t i = 0; i < wallet_count; i++) {
        if (!wallets[i].is_active) continue;
        if (strcmp(wallets[i].address, address) == 0 || str_ieq(wallets[i].label, address)) {
            match_idx = (int)i;
            break;
        }
    }

    /* 2. Try typo-tolerant label match (edit distance <= 2) */
    if (match_idx < 0) {
        for (uint32_t i = 0; i < wallet_count; i++) {
            if (!wallets[i].is_active) continue;
            int d = edit_distance(wallets[i].label, address);
            if (d <= 2 && d < best_dist) {
                best_dist = d;
                match_idx = (int)i;
            }
        }
    }

    if (match_idx >= 0) {
        my_wallet_index = match_idx;
        coin_save();
        return 0;
    }
    return -1;
}

struct coin_wallet *coin_get_my_wallet(void) {
    return &wallets[my_wallet_index];
}

uint32_t coin_get_wallet_count(void) {
    return wallet_count;
}

struct coin_wallet *coin_get_wallet_by_index(uint32_t idx) {
    if (idx < wallet_count && wallets[idx].is_active) {
        return &wallets[idx];
    }
    return NULL;
}

struct coin_block *coin_get_block(uint32_t height) {
    if (chain && height < chain_height && height < chain_capacity) {
        return chain[height];
    }
    return NULL;
}

uint64_t coin_get_balance(const char *address) {
    struct coin_wallet *w = coin_find_wallet(address);
    return w ? w->balance : 0;
}

uint32_t coin_get_height(void) {
    return chain_height;
}

uint32_t coin_get_mempool_count(void) {
    return mempool_count;
}

uint64_t coin_get_circulating_supply(void) {
    uint64_t total = 0;
    for (uint32_t i = 0; i < wallet_count; i++) {
        if (wallets[i].is_active) {
            total += wallets[i].balance;
        }
    }
    return total;
}

int coin_send_with_fee(const char *to_address, uint64_t amount, uint64_t fee) {
    if (!to_address || amount == 0) {
        kprintf("[KitKoin:ERR] Invalid recipient or transfer amount!\n");
        return -1;
    }

    uint64_t total_required = amount + fee;
    struct coin_wallet *my_w = coin_get_my_wallet();
    if (my_w->balance < total_required) {
        kprintf("[KitKoin:ERR] Insufficient funds! Balance: %lu KIT, Requested: %lu KIT (+ %lu fee)\n",
                my_w->balance, amount, fee);
        return -1;
    }

    if (mempool_count >= COIN_MAX_BLOCK_TX) {
        kprintf("[KitKoin:ERR] Transaction mempool is full! Mine a block first.\n");
        return -1;
    }

    /* Auto-register recipient wallet if brand new address */
    struct coin_wallet *dest_w = coin_find_wallet(to_address);
    if (!dest_w) {
        if (wallet_count < COIN_MAX_WALLETS) {
            dest_w = &wallets[wallet_count++];
            memset(dest_w, 0, sizeof(struct coin_wallet));
            strncpy(dest_w->address, to_address, COIN_ADDR_LEN - 1);
            strncpy(dest_w->label, "ExternalPeer", sizeof(dest_w->label) - 1);
            dest_w->balance = 0;
            dest_w->tx_count = 0;
            dest_w->is_active = 1;
        } else {
            kprintf("[KitKoin:ERR] Max wallets reached, cannot register address!\n");
            return -1;
        }
    }

    struct coin_tx *tx = &mempool[mempool_count++];
    memset(tx, 0, sizeof(struct coin_tx));

    strncpy(tx->sender, my_w->address, COIN_ADDR_LEN - 1);
    strncpy(tx->recipient, to_address, COIN_ADDR_LEN - 1);
    tx->amount = amount;
    tx->fee = fee;
    tx->timestamp = timer_get_ticks();
    tx->block_height = 0;

    /* Compute TX Hash */
    sha256_double(tx, sizeof(struct coin_tx) - 32, tx->tx_hash);

    /* Debit sender immediately to prevent mempool double-spend */
    my_w->balance -= total_required;

    char tx_hex[65];
    sha256_to_hex(tx->tx_hash, tx_hex);
    tx_hex[16] = '\0';

    kprintf("[+] Transaction queued to mempool! (Hash: %s...)\n", tx_hex);
    kprintf("    From:   %s (%s)\n", my_w->address, my_w->label);
    kprintf("    To:     %s (%s)\n", dest_w->address, dest_w->label);
    kprintf("    Amount: %lu KIT (Miner Fee: %lu KIT)\n", amount, fee);

    /* Broadcast to Wi-Fi P2P network */
    p2p_broadcast_tx(tx);

    coin_save();

    return 0;
}

int coin_send(const char *to_address, uint64_t amount) {
    return coin_send_with_fee(to_address, amount, 0);
}

int coin_mine_block(void) {
    struct coin_wallet *miner_w = coin_get_my_wallet();
    if (!miner_w) {
        kprintf("[KitKoin:ERR] No active mining wallet found!\n");
        return -1;
    }

    struct coin_block *new_b = allocate_new_block(chain_height);
    if (!new_b) {
        kprintf("[KitKoin:ERR] Cannot mine block: out of memory allocating block #%u!\n", chain_height);
        return -1;
    }

    new_b->index = chain_height;
    new_b->timestamp = timer_get_ticks();
    new_b->difficulty = coin_get_difficulty();
    new_b->reward = coin_get_block_reward(new_b->index);
    new_b->total_fees = 0;

    /* Previous block hash linkage */
    if (chain_height > 0) {
        memcpy(new_b->prev_hash, last_block_hash, 32);
    } else {
        memset(new_b->prev_hash, 0, 32);
    }

    /* 1. Pack pending transactions from Mempool (sort by fee priority) */
    for (uint32_t i = 0; i < mempool_count; i++) {
        for (uint32_t j = i + 1; j < mempool_count; j++) {
            if (mempool[j].fee > mempool[i].fee) {
                struct coin_tx tmp = mempool[i];
                mempool[i] = mempool[j];
                mempool[j] = tmp;
            }
        }
    }

    uint32_t packed_tx_count = 1; /* index 0 reserved for Coinbase */
    for (uint32_t i = 0; i < mempool_count && packed_tx_count < COIN_MAX_BLOCK_TX; i++) {
        mempool[i].block_height = new_b->index;
        memcpy(&new_b->txs[packed_tx_count++], &mempool[i], sizeof(struct coin_tx));
        new_b->total_fees += mempool[i].fee;
    }
    new_b->tx_count = packed_tx_count;

    /* 2. Add Coinbase Transaction (Subsidy + Collected Fees) */
    struct coin_tx *coinbase_tx = &new_b->txs[0];
    strncpy(coinbase_tx->sender, "COINBASE", COIN_ADDR_LEN - 1);
    strncpy(coinbase_tx->recipient, miner_w->address, COIN_ADDR_LEN - 1);
    coinbase_tx->amount = new_b->reward + new_b->total_fees;
    coinbase_tx->fee = 0;
    coinbase_tx->timestamp = new_b->timestamp;
    coinbase_tx->block_height = new_b->index;
    sha256_double(coinbase_tx, sizeof(struct coin_tx) - 32, coinbase_tx->tx_hash);

    /* Compute Merkle Root */
    compute_merkle_root(new_b->txs, new_b->tx_count, new_b->merkle_root);

    /* 3. Proof-of-Work Mining Loop */
    kprintf("\n[KitKoin:MINER] Commencing Proof-of-Work mining for Block #%u...\n", new_b->index);
    kprintf("[*] Difficulty Target: %u leading hex zeros\n", new_b->difficulty);
    kprintf("[*] Transactions in block: %u (Subsidy: %lu KIT, Fees: %lu KIT)\n",
            new_b->tx_count, new_b->reward, new_b->total_fees);

    uint32_t nonce = 0;
    uint8_t current_hash[32];
    uint64_t start_time = timer_get_ticks();

    while (1) {
        new_b->nonce = nonce;
        compute_block_hash(new_b, current_hash);

        if (meets_difficulty(current_hash, new_b->difficulty)) {
            memcpy(new_b->block_hash, current_hash, 32);
            break;
        }

        nonce++;
        if (nonce % 20000 == 0) {
            /* Yield CPU momentarily to maintain timer ticks and OS responsiveness */
            task_yield();
        }
    }

    uint64_t elapsed_ticks = timer_get_ticks() - start_time;
    char hash_str[65];
    sha256_to_hex(new_b->block_hash, hash_str);

    kprintf("[✔] BLOCK MINED SUCCESSFULLY!\n");
    kprintf("    Block Index:    #%u\n", new_b->index);
    kprintf("    Nonce Solved:   %u hashes tried (%lu ms)\n", nonce, elapsed_ticks * 10);
    kprintf("    Block Hash:     %s\n", hash_str);
    kprintf("    Reward:         +%lu KIT (+%lu fees) credited to miner (%s)\n",
            new_b->reward, new_b->total_fees, miner_w->address);

    /* 4. Update balances for included transactions */
    miner_w->balance += coinbase_tx->amount;
    miner_w->tx_count++;

    for (uint32_t i = 1; i < new_b->tx_count; i++) {
        struct coin_wallet *recip = coin_find_wallet(new_b->txs[i].recipient);
        if (recip) {
            recip->balance += new_b->txs[i].amount;
            recip->tx_count++;
        }
    }

    /* 5. Clear mined transactions from mempool */
    mempool_count = 0;
    memcpy(last_block_hash, new_b->block_hash, 32);
    chain_height++;

    /* 6. Broadcast newly mined block to local Wi-Fi P2P network */
    p2p_broadcast_block(new_b);

    coin_save();

    return 0;
}

int coin_mine_blocks(uint32_t count) {
    if (count == 0) count = 1;
    kprintf("[KitKoin:BATCH] Mining batch of %u blocks...\n", count);
    for (uint32_t i = 0; i < count; i++) {
        if (coin_mine_block() != 0) {
            kprintf("[KitKoin:BATCH] Aborting batch mining early at block #%u\n", i);
            return -1;
        }
    }
    kprintf("[KitKoin:BATCH] Successfully mined %u blocks! New Height: #%u\n", count, chain_height);
    return 0;
}

int coin_verify_chain(void) {
    kprintf("\n====================================================================\n");
    kprintf("        KITKOIN (KIT) CRYPTOGRAPHIC LEDGER INTEGRITY AUDIT          \n");
    kprintf("====================================================================\n");
    if (chain_height == 0) {
        kprintf("[!] Blockchain empty!\n");
        return 0;
    }

    /* Track simulated ledger balances to ensure no double spends or negative balances */
    uint64_t sim_balances[COIN_MAX_WALLETS];
    memset(sim_balances, 0, sizeof(sim_balances));
    uint64_t total_minted_rewards = 0;

    for (uint32_t i = 0; i < chain_height; i++) {
        struct coin_block *b = coin_get_block(i);
        if (!b) {
            kprintf("[FAIL] Block #%u cannot be loaded from memory!\n", i);
            return 0;
        }
        uint8_t recalculated_hash[32];
        compute_block_hash(b, recalculated_hash);

        if (memcmp(b->block_hash, recalculated_hash, 32) != 0) {
            kprintf("[FAIL] Block #%u hash mismatch!\n", i);
            return 0;
        }

        if (!meets_difficulty(b->block_hash, b->difficulty)) {
            kprintf("[FAIL] Block #%u fails Proof-of-Work difficulty target!\n", i);
            return 0;
        }

        if (i > 0) {
            struct coin_block *prev = coin_get_block(i - 1);
            if (!prev || memcmp(b->prev_hash, prev->block_hash, 32) != 0) {
                kprintf("[FAIL] Block #%u previous hash link broken!\n", i);
                return 0;
            }
        }

        /* Verify Merkle Root */
        uint8_t recalculated_merkle[32];
        compute_merkle_root(b->txs, b->tx_count, recalculated_merkle);
        if (memcmp(b->merkle_root, recalculated_merkle, 32) != 0) {
            kprintf("[FAIL] Block #%u Merkle root mismatch!\n", i);
            return 0;
        }

        /* Verify Coinbase Transaction */
        if (b->tx_count < 1) {
            kprintf("[FAIL] Block #%u missing Coinbase transaction!\n", i);
            return 0;
        }
        struct coin_tx *cb = &b->txs[0];
        if (strcmp(cb->sender, "GENESIS") != 0 && strcmp(cb->sender, "COINBASE") != 0) {
            kprintf("[FAIL] Block #%u TX 0 is not a valid Coinbase!\n", i);
            return 0;
        }

        uint64_t expected_subsidy = coin_get_block_reward(b->index);
        total_minted_rewards += expected_subsidy;

        /* Replay Coinbase */
        struct coin_wallet *mined_to = coin_find_wallet(cb->recipient);
        if (mined_to) {
            for (uint32_t w = 0; w < wallet_count; w++) {
                if (&wallets[w] == mined_to) {
                    sim_balances[w] += cb->amount;
                    break;
                }
            }
        }

        /* Replay Block Transactions */
        uint64_t fees_in_block = 0;
        for (uint32_t t = 1; t < b->tx_count; t++) {
            struct coin_tx *tx = &b->txs[t];
            fees_in_block += tx->fee;

            struct coin_wallet *snd_w = coin_find_wallet(tx->sender);
            struct coin_wallet *rec_w = coin_find_wallet(tx->recipient);
            if (snd_w) {
                for (uint32_t w = 0; w < wallet_count; w++) {
                    if (&wallets[w] == snd_w) {
                        uint64_t debit = tx->amount + tx->fee;
                        if (sim_balances[w] < debit) {
                            kprintf("[FAIL] Block #%u TX #%u: negative balance detected for %s!\n", i, t, tx->sender);
                            return 0;
                        }
                        sim_balances[w] -= debit;
                        break;
                    }
                }
            }
            if (rec_w) {
                for (uint32_t w = 0; w < wallet_count; w++) {
                    if (&wallets[w] == rec_w) {
                        sim_balances[w] += tx->amount;
                        break;
                    }
                }
            }
        }

        if (b->total_fees != fees_in_block) {
            kprintf("[FAIL] Block #%u fee accounting mismatch! Recorded: %lu, Calculated: %lu\n",
                    i, b->total_fees, fees_in_block);
            return 0;
        }
    }

    kprintf("[PASS] Proof-of-Work: All %u blocks meet cryptographic difficulty target.\n", chain_height);
    kprintf("[PASS] Chain Linkage: All SHA-256 parent block linkages verified.\n");
    kprintf("[PASS] Merkle Roots:  All transaction Merkle trees verified.\n");
    kprintf("[PASS] Ledger Audit:  Replayed all transactions from Genesis block.\n");
    kprintf("       Total Minted:  %lu KIT (Total Circulating: %lu KIT)\n",
            total_minted_rewards, coin_get_circulating_supply());
    kprintf("       Zero Double-Spends, Zero Deficits. Chain integrity is 100%% SOUND!\n");
    kprintf("====================================================================\n\n");
    return 1;
}

void coin_dump_chain(void) {
    kprintf("\n====================================================================\n");
    kprintf("                 KITKOIN (KIT) BLOCKCHAIN LEDGER                    \n");
    kprintf("====================================================================\n");
    kprintf("Height: #%u | Circulating Supply: %lu KIT | Mempool: %u TXs | Halving in: %u blks\n\n",
            chain_height, coin_get_circulating_supply(), mempool_count, coin_get_blocks_until_halving());

    uint32_t start_skip = (chain_height > 12) ? 4 : chain_height;
    uint32_t resume_idx = (chain_height > 12) ? chain_height - 6 : chain_height;

    for (uint32_t i = 0; i < chain_height; i++) {
        if (chain_height > 12 && i == start_skip) {
            kprintf("\n  ... [%u intermediate blocks omitted for brevity] ...\n\n", resume_idx - start_skip);
            i = resume_idx;
        }
        struct coin_block *b = coin_get_block(i);
        if (!b) continue;
        char b_hash[65], p_hash[65];
        sha256_to_hex(b->block_hash, b_hash);
        sha256_to_hex(b->prev_hash, p_hash);

        b_hash[16] = '\0';
        p_hash[16] = '\0';

        kprintf("[Block #%02u] Nonce: %-8u | TXs: %u | Subsidy: %-2lu KIT | Fees: %-2lu KIT | Hash: %s...\n",
                b->index, b->nonce, b->tx_count, b->reward, b->total_fees, b_hash);

        for (uint32_t t = 0; t < b->tx_count; t++) {
            kprintf("    └─ TX #%u: %-16s ──[%lu KIT (+%lu fee)]──> %s\n",
                    t, b->txs[t].sender, b->txs[t].amount, b->txs[t].fee, b->txs[t].recipient);
        }
    }
    kprintf("====================================================================\n\n");
}

void coin_dump_wallets(void) {
    kprintf("\n--- KitKoin Wallet Registry ---\n");
    kprintf("  Idx | Status | Label            | Address                 | Balance (KIT)\n");
    kprintf("  ----+--------+------------------+-------------------------+--------------\n");
    for (uint32_t i = 0; i < wallet_count; i++) {
        struct coin_wallet *w = &wallets[i];
        if (w->is_active) {
            const char *status = (i == (uint32_t)my_wallet_index) ? "* ACTIVE" : "  PEER  ";
            kprintf("  %-3u | %s | %-16s | %-23s | %lu KIT\n",
                    i, status, w->label, w->address, w->balance);
        }
    }
    kprintf("\n");
}

void coin_dump_block(uint32_t height) {
    if (height >= chain_height) {
        kprintf("[KitKoin:ERR] Block height #%u exceeds current chain height (#%u)!\n",
                height, chain_height > 0 ? chain_height - 1 : 0);
        return;
    }

    struct coin_block *b = coin_get_block(height);
    if (!b) {
        kprintf("[KitKoin:ERR] Unable to load block #%u from memory!\n", height);
        return;
    }
    char b_hash[65], p_hash[65], m_root[65];
    sha256_to_hex(b->block_hash, b_hash);
    sha256_to_hex(b->prev_hash, p_hash);
    sha256_to_hex(b->merkle_root, m_root);

    kprintf("\n====================================================================\n");
    kprintf("                     KITKOIN BLOCK INSPECTOR: #%u                    \n", b->index);
    kprintf("====================================================================\n");
    kprintf("  Block Index:         #%u %s\n", b->index, b->index == 0 ? "(Genesis Block)" : "");
    kprintf("  Block Hash:          %s\n", b_hash);
    kprintf("  Previous Block Hash: %s\n", p_hash);
    kprintf("  Merkle Root:         %s\n", m_root);
    kprintf("  Timestamp (Ticks):   %lu (%lu ms)\n", b->timestamp, b->timestamp * 10);
    kprintf("  Difficulty Target:   %u leading hex zeros\n", b->difficulty);
    kprintf("  Nonce:               %u\n", b->nonce);
    kprintf("  Coinbase Subsidy:    %lu KIT (Halving Interval: %u)\n", b->reward, COIN_HALVING_INTERVAL);
    kprintf("  Total Fees in Block: %lu KIT\n", b->total_fees);
    kprintf("  Transaction Count:   %u transactions\n", b->tx_count);
    kprintf("  ------------------------------------------------------------------\n");
    kprintf("  Transactions in Block:\n");
    for (uint32_t t = 0; t < b->tx_count; t++) {
        struct coin_tx *tx = &b->txs[t];
        char tx_h[65];
        sha256_to_hex(tx->tx_hash, tx_h);
        tx_h[16] = '\0';
        kprintf("   [%u] TxID: %s... | %-16s ──[%lu KIT + %lu fee]──> %s\n",
                t, tx_h, tx->sender, tx->amount, tx->fee, tx->recipient);
    }
    kprintf("====================================================================\n\n");
}

void coin_dump_tx(const char *hex_prefix) {
    if (!hex_prefix) return;
    int found = 0;

    /* 1. Check Mempool */
    for (uint32_t i = 0; i < mempool_count; i++) {
        char tx_h[65];
        sha256_to_hex(mempool[i].tx_hash, tx_h);
        if (strncmp(tx_h, hex_prefix, strlen(hex_prefix)) == 0) {
            kprintf("\n[KitKoin:TX Found in Mempool - PENDING CONFIRMATION]\n");
            kprintf("  TxID:       %s\n", tx_h);
            kprintf("  Status:     Pending in Mempool (0 confirmations)\n");
            kprintf("  Sender:     %s\n", mempool[i].sender);
            kprintf("  Recipient:  %s\n", mempool[i].recipient);
            kprintf("  Amount:     %lu KIT\n", mempool[i].amount);
            kprintf("  Fee:        %lu KIT\n", mempool[i].fee);
            kprintf("  Timestamp:  %lu ticks\n\n", mempool[i].timestamp);
            found = 1;
        }
    }

    /* 2. Check Blocks */
    for (uint32_t b = 0; b < chain_height; b++) {
        struct coin_block *blk = coin_get_block(b);
        if (!blk) continue;
        for (uint32_t t = 0; t < blk->tx_count; t++) {
            char tx_h[65];
            sha256_to_hex(blk->txs[t].tx_hash, tx_h);
            if (strncmp(tx_h, hex_prefix, strlen(hex_prefix)) == 0) {
                uint32_t confs = chain_height - b;
                kprintf("\n[KitKoin:TX Found in Block #%u - CONFIRMED]\n", b);
                kprintf("  TxID:          %s\n", tx_h);
                kprintf("  Block Height:  #%u (%u confirmations)\n", b, confs);
                kprintf("  Sender:        %s\n", blk->txs[t].sender);
                kprintf("  Recipient:     %s\n", blk->txs[t].recipient);
                kprintf("  Amount:        %lu KIT\n", blk->txs[t].amount);
                kprintf("  Fee:           %lu KIT\n", blk->txs[t].fee);
                kprintf("  Timestamp:     %lu ticks\n\n", blk->txs[t].timestamp);
                found = 1;
            }
        }
    }

    if (!found) {
        kprintf("[KitKoin:ERR] Transaction with prefix '%s' not found on blockchain or mempool!\n", hex_prefix);
    }
}

void coin_dump_history(const char *address) {
    if (!address) {
        struct coin_wallet *my_w = coin_get_my_wallet();
        address = my_w->address;
    }

    kprintf("\n--- KitKoin Transaction History for %s ---\n", address);
    kprintf("  Type | Block  | Amount   | Fee   | Counterparty Address    | TxID\n");
    kprintf("  -----+--------+----------+-------+-------------------------+-------------\n");

    int count = 0;
    for (uint32_t b = 0; b < chain_height; b++) {
        struct coin_block *blk = coin_get_block(b);
        if (!blk) continue;
        for (uint32_t t = 0; t < blk->tx_count; t++) {
            struct coin_tx *tx = &blk->txs[t];
            int is_recip = (strcmp(tx->recipient, address) == 0);
            int is_sender = (strcmp(tx->sender, address) == 0);

            if (is_recip || is_sender) {
                char tx_h[65];
                sha256_to_hex(tx->tx_hash, tx_h);
                tx_h[12] = '\0';

                const char *type = is_recip ? "[+IN] " : "[-OUT]";
                const char *other = is_recip ? tx->sender : tx->recipient;

                kprintf("  %s | #%-5u | %-8lu | %-5lu | %-23s | %s...\n",
                        type, b, tx->amount, tx->fee, other, tx_h);
                count++;
            }
        }
    }

    /* Also check mempool */
    for (uint32_t m = 0; m < mempool_count; m++) {
        struct coin_tx *tx = &mempool[m];
        int is_recip = (strcmp(tx->recipient, address) == 0);
        int is_sender = (strcmp(tx->sender, address) == 0);

        if (is_recip || is_sender) {
            char tx_h[65];
            sha256_to_hex(tx->tx_hash, tx_h);
            tx_h[12] = '\0';

            const char *type = is_recip ? "[MEM+]" : "[MEM-]";
            const char *other = is_recip ? tx->sender : tx->recipient;

            kprintf("  %s | PEND   | %-8lu | %-5lu | %-23s | %s...\n",
                    type, tx->amount, tx->fee, other, tx_h);
            count++;
        }
    }

    if (count == 0) {
        kprintf("  (No transactions found for address %s)\n", address);
    }
    kprintf("--------------------------------------------------------------------\n\n");
}

static volatile int automine_active = 0;
static int automine_task_pid = -1;

static void automine_worker(void) {
    kprintf("\n[KitKoin:AutoMiner] Background miner thread started (PID: %d)!\n", automine_task_pid);
    while (automine_active) {
        coin_mine_block();

        /* Sleep 4 seconds between blocks or until interrupted */
        for (int i = 0; i < 40 && automine_active; i++) {
            timer_sleep_ms(100);
            task_yield();
        }
    }
    automine_active = 0;
    kprintf("\n[KitKoin:AutoMiner] Background miner thread terminated.\n");
}

int coin_automine_start(void) {
    if (automine_active) {
        kprintf("[KitKoin:AutoMiner] Autominer is already active.\n");
        return 0;
    }
    automine_active = 1;
    automine_task_pid = task_create("kit_miner", automine_worker, 3);
    if (automine_task_pid < 0) {
        automine_active = 0;
        kprintf("[KitKoin:ERR] Failed to spawn background miner task!\n");
        return -1;
    }
    kprintf("[+] Background autominer started (PID: %d). Mining blocks asynchronously in background!\n",
            automine_task_pid);
    return 0;
}

void coin_automine_stop(void) {
    if (!automine_active) {
        kprintf("[KitKoin:AutoMiner] Autominer is not currently running.\n");
        return;
    }
    automine_active = 0;
    kprintf("[*] Background autominer stopped.\n");
}

int coin_automine_is_running(void) {
    return automine_active;
}

void coin_benchmark(uint32_t test_ms) {
    if (test_ms == 0) test_ms = 1000;
    kprintf("\n[KitKoin:BENCHMARK] Commencing CPU SHA-256 Mining Benchmark (%u ms)...\n", test_ms);
    uint8_t test_data[80];
    memset(test_data, 0x42, sizeof(test_data));
    uint8_t out[32];

    uint64_t start_ticks = timer_get_ticks();
    uint64_t target_ticks = start_ticks + (test_ms / 10);
    uint64_t hashes = 0;

    while (timer_get_ticks() < target_ticks) {
        sha256_double(test_data, sizeof(test_data), out);
        test_data[0] = out[0];
        hashes++;
        if (hashes % 5000 == 0) {
            task_yield();
        }
    }

    uint64_t end_ticks = timer_get_ticks();
    uint64_t actual_ms = (end_ticks - start_ticks) * 10;
    if (actual_ms == 0) actual_ms = 1;

    uint64_t h_per_sec = (hashes * 1000) / actual_ms;
    kprintf("[KitKoin:BENCHMARK] Benchmark Complete!\n");
    kprintf("  Hashes Computed: %lu dual-SHA-256 iterations\n", hashes);
    kprintf("  Elapsed Time:    %lu ms\n", actual_ms);
    kprintf("  CPU Hashrate:    %lu.%02lu kH/s (%lu H/s)\n",
            h_per_sec / 1000, (h_per_sec % 1000) / 10, h_per_sec);
    kprintf("  Est. Diff 3 Hit: ~%lu ms average solve time\n\n",
            (4096 * 1000) / (h_per_sec ? h_per_sec : 1));
}

int coin_save(void) {
    disk_io_buffer.magic = COIN_DISK_MAGIC;
    disk_io_buffer.version = COIN_DISK_VERSION;
    disk_io_buffer.wallet_count = wallet_count;
    disk_io_buffer.my_wallet_index = my_wallet_index;
    memcpy(disk_io_buffer.wallets, wallets, sizeof(wallets));
    disk_io_buffer.chain_height = chain_height;
    memset(disk_io_buffer.chain, 0, sizeof(disk_io_buffer.chain));
    uint32_t save_count = (chain_height <= COIN_DISK_SAVED_BLOCKS) ? chain_height : COIN_DISK_SAVED_BLOCKS;
    for (uint32_t i = 0; i < save_count; i++) {
        struct coin_block *blk = coin_get_block(i);
        if (blk) {
            memcpy(&disk_io_buffer.chain[i], blk, sizeof(struct coin_block));
        }
    }
    disk_io_buffer.mempool_count = mempool_count;
    memcpy(disk_io_buffer.mempool, mempool, sizeof(mempool));

    uint32_t sum = 0x5A5A5A5A;
    uint32_t *p = (uint32_t *)&disk_io_buffer;
    uint32_t words = (sizeof(struct coin_disk_state) - sizeof(uint32_t)) / sizeof(uint32_t);
    for (uint32_t i = 0; i < words; i++) {
        sum ^= p[i];
    }
    disk_io_buffer.checksum = sum;

    uint32_t total_sectors = (sizeof(struct coin_disk_state) + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int res = virtio_blk_write(COIN_DISK_START_SECTOR, &disk_io_buffer, total_sectors);

    /* Update human-readable wallet.txt in ramfs */
    struct coin_wallet *my_w = coin_get_my_wallet();
    char txt_buf[256];
    snprintf(txt_buf, sizeof(txt_buf),
             "KitKoin Wallet Record\nAddress: %s\nLabel: %s\nBalance: %lu KIT\nHeight: #%u\nStatus: PERSISTED_TO_DISK\n",
             my_w ? my_w->address : "N/A", my_w ? my_w->label : "N/A",
             my_w ? my_w->balance : 0, chain_height);
    ramfs_create("wallet.txt");
    ramfs_write("wallet.txt", txt_buf, strlen(txt_buf));

    return res;
}

int coin_load(void) {
    uint32_t total_sectors = (sizeof(struct coin_disk_state) + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int res = virtio_blk_read(COIN_DISK_START_SECTOR, &disk_io_buffer, total_sectors);
    if (res != 0) {
        return -1;
    }

    if (disk_io_buffer.magic != COIN_DISK_MAGIC) {
        return -1;
    }

    if (disk_io_buffer.version != COIN_DISK_VERSION) {
        return -1;
    }

    uint32_t sum = 0x5A5A5A5A;
    uint32_t *p = (uint32_t *)&disk_io_buffer;
    uint32_t words = (sizeof(struct coin_disk_state) - sizeof(uint32_t)) / sizeof(uint32_t);
    for (uint32_t i = 0; i < words; i++) {
        sum ^= p[i];
    }
    if (disk_io_buffer.checksum != sum) {
        return -1;
    }

    if (disk_io_buffer.wallet_count == 0 || disk_io_buffer.chain_height == 0) {
        return -1;
    }

    wallet_count = (disk_io_buffer.wallet_count <= COIN_MAX_WALLETS) ? disk_io_buffer.wallet_count : COIN_MAX_WALLETS;
    my_wallet_index = (disk_io_buffer.my_wallet_index < wallet_count) ? disk_io_buffer.my_wallet_index : 0;
    memcpy(wallets, disk_io_buffer.wallets, sizeof(wallets));
    uint32_t load_count = (disk_io_buffer.chain_height <= COIN_DISK_SAVED_BLOCKS) ? disk_io_buffer.chain_height : COIN_DISK_SAVED_BLOCKS;
    ensure_chain_capacity(load_count > 64 ? load_count : 64);
    chain_height = load_count;
    for (uint32_t i = 0; i < load_count; i++) {
        struct coin_block *blk = allocate_new_block(i);
        if (blk) {
            memcpy(blk, &disk_io_buffer.chain[i], sizeof(struct coin_block));
            if (i == load_count - 1) {
                memcpy(last_block_hash, blk->block_hash, 32);
            }
        }
    }
    mempool_count = (disk_io_buffer.mempool_count <= COIN_MAX_BLOCK_TX) ? disk_io_buffer.mempool_count : 0;
    memcpy(mempool, disk_io_buffer.mempool, sizeof(mempool));

    /* Update human-readable wallet.txt in ramfs */
    struct coin_wallet *my_w = coin_get_my_wallet();
    char txt_buf[256];
    snprintf(txt_buf, sizeof(txt_buf),
             "KitKoin Wallet Record\nAddress: %s\nLabel: %s\nBalance: %lu KIT\nHeight: #%u\nStatus: LOADED_FROM_DISK\n",
             my_w ? my_w->address : "N/A", my_w ? my_w->label : "N/A",
             my_w ? my_w->balance : 0, chain_height);
    ramfs_create("wallet.txt");
    ramfs_write("wallet.txt", txt_buf, strlen(txt_buf));

    return 0;
}



int coin_faucet(uint64_t amount) {
    if (amount == 0) amount = 1000;
    struct coin_wallet *my_w = coin_get_my_wallet();
    if (!my_w) return -1;
    my_w->balance += amount;
    my_w->tx_count++;
    coin_save();
    kprintf("[+] Faucet credited +%lu KIT to wallet %s (%s)!\n", amount, my_w->address, my_w->label);
    kprintf("    New Balance: %lu KIT\n", my_w->balance);
    kprintf("    [✔] Persisted state to storage disk (disk.img)! Will survive reboots.\n");
    return 0;
}

void coin_init(void) {
    if (chain && chain_capacity > 0) {
        for (uint32_t i = 0; i < chain_capacity; i++) {
            if (chain[i]) {
                kfree(chain[i]);
                chain[i] = NULL;
            }
        }
        kfree(chain);
        chain = NULL;
        chain_capacity = 0;
    }
    memset(last_block_hash, 0, 32);
    memset(mempool, 0, sizeof(mempool));
    memset(wallets, 0, sizeof(wallets));
    chain_height = 0;
    mempool_count = 0;
    wallet_count = 0;

    ensure_chain_capacity(64);

    /* Try to restore persistent state from disk if valid */
    if (coin_load() == 0) {
        kprintf("[+] KitKoin (KIT) Restored from persistent disk storage!\n");
        kprintf("    Chain Height:          #%u blocks\n", chain_height);
        struct coin_wallet *my_w = coin_get_my_wallet();
        if (my_w) {
            kprintf("    Active Account:        %s (%s, %lu KIT)\n", my_w->address, my_w->label, my_w->balance);
        }
        return;
    }

    /* 1. Initialize Primary Node Wallet */
    struct coin_wallet *w0 = coin_create_wallet("PrimaryNode");
    my_wallet_index = 0;

    /* 2. Initialize Second Demo Peer Wallet */
    coin_create_wallet("WiFiPeerAlice");

    /* 3. Build & Hardcode Genesis Block #0 */
    struct coin_block *genesis = allocate_new_block(0);
    if (!genesis) {
        kprintf("[KitKoin:ERR] Failed to allocate Genesis Block #0!\n");
        return;
    }
    genesis->index = 0;
    genesis->timestamp = 0;
    memset(genesis->prev_hash, 0, 32);
    genesis->difficulty = 2; /* Easy difficulty for Genesis */
    genesis->nonce = 0;
    genesis->reward = COIN_INITIAL_REWARD;
    genesis->total_fees = 0;

    /* Genesis Coinbase transaction */
    struct coin_tx *gtx = &genesis->txs[0];
    strncpy(gtx->sender, "GENESIS", COIN_ADDR_LEN - 1);
    strncpy(gtx->recipient, w0->address, COIN_ADDR_LEN - 1);
    gtx->amount = COIN_INITIAL_REWARD;
    gtx->fee = 0;
    gtx->timestamp = 0;
    gtx->block_height = 0;
    sha256_double(gtx, sizeof(struct coin_tx) - 32, gtx->tx_hash);
    genesis->tx_count = 1;

    compute_merkle_root(genesis->txs, 1, genesis->merkle_root);
    while (1) {
        compute_block_hash(genesis, genesis->block_hash);
        if (meets_difficulty(genesis->block_hash, genesis->difficulty)) {
            break;
        }
        genesis->nonce++;
    }

    memcpy(last_block_hash, genesis->block_hash, 32);

    /* Credit initial reward to primary wallet */
    w0->balance = COIN_INITIAL_REWARD;
    w0->tx_count = 1;
    chain_height = 1;

    /* Save fresh initial state to disk */
    coin_save();

    char gen_hash[65];
    sha256_to_hex(genesis->block_hash, gen_hash);

    kprintf("[+] KitKoin (KIT) Infinite Blockchain initialized!\n");
    kprintf("    Genesis Block #0 Hash: %s\n", gen_hash);
    kprintf("    Primary Node Wallet:   %s (%lu KIT)\n", w0->address, w0->balance);
    kprintf("    Consensus Parameters:  Halving every %d blocks, %lu Max Supply (Infinite)\n",
            COIN_HALVING_INTERVAL, COIN_MAX_SUPPLY);

    /* Background autominer off by default (user can start via kitkoin automine on) */
}

void coin_restart(void) {
    kprintf("\n[KitKoin:RESTART] Resetting blockchain to fresh Genesis Block #0...\n");
    coin_automine_stop();
    timer_sleep_ms(200);

    /* Wipe disk persistence record so reboot/reload doesn't load old blocks */
    memset(&disk_io_buffer, 0, sizeof(disk_io_buffer));
    uint32_t total_sectors = (sizeof(struct coin_disk_state) + SECTOR_SIZE - 1) / SECTOR_SIZE;
    virtio_blk_write(COIN_DISK_START_SECTOR, &disk_io_buffer, total_sectors);

    /* Re-initialize fresh blockchain */
    coin_init();
    kprintf("[✔] Blockchain restart complete! Fresh Genesis Block #0 active, autominer running.\n");
}
