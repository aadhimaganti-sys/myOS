#ifndef _SHA256_H
#define _SHA256_H

#include "types.h"
#include "stddef.h"

struct sha256_ctx {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
};

void sha256_init(struct sha256_ctx *ctx);
void sha256_update(struct sha256_ctx *ctx, const void *data, size_t len);
void sha256_final(struct sha256_ctx *ctx, uint8_t hash[32]);

void sha256_hash(const void *data, size_t len, uint8_t hash[32]);
void sha256_double(const void *data, size_t len, uint8_t hash[32]);
void sha256_to_hex(const uint8_t hash[32], char hex[65]);

#endif /* _SHA256_H */
