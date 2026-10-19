#ifndef _COIN_H
#define _COIN_H

#include "types.h"
#include "stddef.h"

#define COIN_NAME               "KitKoin"
#define COIN_SYMBOL             "KIT"
#define COIN_INITIAL_REWARD     50ULL
#define COIN_BLOCK_REWARD       50ULL
#define COIN_HALVING_INTERVAL   16
#define COIN_MAX_SUPPLY         0xFFFFFFFFFFFFFFFFULL
#define COIN_MAX_BLOCK_TX       8
#define COIN_DISK_SAVED_BLOCKS  64
#define COIN_MAX_CHAIN_LEN      64
#define COIN_MAX_WALLETS        32
#define COIN_ADDR_LEN           24
#define COIN_DEFAULT_DIFFICULTY 3   /* Number of leading hex zeros required */

struct coin_tx {
    char sender[COIN_ADDR_LEN];
    char recipient[COIN_ADDR_LEN];
    uint64_t amount;
    uint64_t fee;
    uint64_t timestamp;
    uint32_t block_height;
    uint8_t tx_hash[32];
};

struct coin_block {
    uint32_t index;
    uint64_t timestamp;
    uint8_t prev_hash[32];
    uint8_t merkle_root[32];
    uint32_t nonce;
    uint32_t difficulty;
    uint64_t reward;
    uint64_t total_fees;
    uint32_t tx_count;
    struct coin_tx txs[COIN_MAX_BLOCK_TX];
    uint8_t block_hash[32];
};

struct coin_wallet {
    char address[COIN_ADDR_LEN];
    char label[32];
    uint64_t balance;
    uint32_t tx_count;
    int is_active;
};

/* Core Blockchain & Consensus Engine */
void coin_init(void);
void coin_restart(void);
struct coin_wallet *coin_get_my_wallet(void);
struct coin_wallet *coin_find_wallet(const char *address);
struct coin_wallet *coin_create_wallet(const char *label);
int  coin_remove_wallet(const char *target);
int  coin_switch_wallet(const char *address);
uint64_t coin_get_balance(const char *address);
int  coin_send(const char *to_address, uint64_t amount);
int  coin_send_with_fee(const char *to_address, uint64_t amount, uint64_t fee);
int  coin_mine_block(void);
int  coin_mine_block_silent(void);
int  coin_mine_blocks(uint32_t count);
int  coin_verify_chain(void);

/* Inspectors & Queries */
void coin_dump_chain(void);
void coin_dump_wallets(void);
void coin_dump_block(uint32_t height);
void coin_dump_tx(const char *hex_prefix);
void coin_dump_history(const char *address);
uint32_t coin_get_height(void);
uint64_t coin_get_circulating_supply(void);
uint32_t coin_get_mempool_count(void);
uint64_t coin_get_block_reward(uint32_t height);
uint32_t coin_get_blocks_until_halving(void);
uint32_t coin_get_difficulty(void);
struct coin_block *coin_get_block(uint32_t height);
uint32_t coin_get_wallet_count(void);
struct coin_wallet *coin_get_wallet_by_index(uint32_t idx);

/* Performance & Background Multitasking */
void coin_benchmark(uint32_t test_ms);
int  coin_automine_start(void);
void coin_automine_stop(void);
int  coin_automine_is_running(void);

/* Disk Persistence & Recovery */
int  coin_save(void);
int  coin_load(void);
int  coin_faucet(uint64_t amount);

#endif /* _COIN_H */
