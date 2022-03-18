#ifndef _BIGNUM_H
#define _BIGNUM_H

#include <stddef.h>

#define NUM_TYPE uint32_t
#define TMP_TYPE uint64_t
#define MAX_VAL (0xFFFFFFFF)
#define NUM_SZ (sizeof(NUM_TYPE))

/*
 * bignum data structure
 * number[0] contains least significant bits
 * number[size - 1] contains most significant bits
 * sign = 1 for negative number
 */
typedef struct _bignum {
    NUM_TYPE *num;
    size_t sz;
    int sign;
} bignum;

#define BIGNUM_SZ (500)

bignum *bignum_create(size_t sz);

void bignum_destroy(bignum *bn);

int bignum_cpy(bignum *dst, const bignum *src);

size_t bignum_to_string(bignum *bn, char *buf);

void bignum_add(bignum *s, const bignum *a, const bignum *b);

void bignum_sub(bignum *d, const bignum *a, const bignum *b);

void bignum_lshift(bignum *bn, size_t shift);

void bignum_mult(bignum *p, const bignum *a, const bignum *b);

#endif /* _BIGNUM_H */
