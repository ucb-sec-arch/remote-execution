/*
 * sha1.h: Modified for Flicker.
 */

#ifndef _SHA1_H
#define _SHA1_H

#include <sys/types.h>
#include <stdarg.h>
#include <stddef.h>

#define SHA_DIGEST_LENGTH 20
/* The SHA block size and message digest sizes, in bytes */

#define SHA_DATASIZE    64
#define SHA_DATALEN     16
#define SHA_DIGESTSIZE  20
#define SHA_DIGESTLEN    5
/* The structure for storing SHA info */

typedef struct sha1_ctx {
  unsigned int digest[SHA_DIGESTLEN];  /* Message digest */
  unsigned int count_l, count_h;       /* 64-bit block count */
  unsigned char block[SHA_DATASIZE];     /* SHA data buffer */
  int index;                             /* index into buffer */
} SHA1_CTX;

typedef struct _sha1_hash {
    unsigned char h[SHA_DIGEST_LENGTH];
} SHA1_HASH;

void SHA1_init(SHA1_CTX *ctx) __attribute__ ((section (".text.slb")));
void SHA1_update(SHA1_CTX *ctx, const unsigned char *buffer, unsigned int len) __attribute__ ((section (".text.slb")));
void SHA1_final(SHA1_CTX *ctx) __attribute__ ((section (".text.slb")));
void SHA1_digest(SHA1_CTX *ctx, unsigned char *s) __attribute__ ((section (".text.slb")));
void SHA1_copy(SHA1_CTX *dest, SHA1_CTX *src) __attribute__ ((section (".text.slb")));

int sha1_buffer(const unsigned char *buffer, size_t len,
                unsigned char md[SHA_DIGEST_LENGTH]);


#endif
