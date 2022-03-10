#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include "bignum.h"


bignum *bignum_create(size_t sz)
{
    bignum *bn = (bignum *) kmalloc(sizeof(bignum), GFP_KERNEL);
    bn->num = (NUM_TYPE *) kmalloc(sz * sizeof(NUM_TYPE), GFP_KERNEL);
    memset(bn->num, 0, sz * sizeof(NUM_TYPE));
    bn->sz = sz;
    bn->sign = 0;

    return bn;
}

void bignum_destroy(bignum *bn)
{
    if (bn == NULL)
        return;
    kfree(bn->num);
    kfree(bn);
}

int bignum_cpy(bignum *dst, bignum *src)
{
    if (dst == NULL || src == NULL)
        return -1;
    dst->sign = src->sign;
    memcpy(dst->num, src->num, src->sz * sizeof(NUM_TYPE));
    return 0;
}

ssize_t bignum_to_string(bignum *bn, char *buf)
{
    ssize_t j = 0, blk_sz = NUM_SZ << 2;
    for (int i = bn->sz - 1; i >= 0; --i)
        j += snprintf(&buf[j], blk_sz, "%08X", bn->num[i]);
    return j;
}

void bignum_add(bignum *s, const bignum *a, const bignum *b)
{
    int carry = 0;
    for (int i = 0; i < s->sz; ++i) {
        TMP_TYPE tmp = (TMP_TYPE) a->num[i] + b->num[i] + carry;
        carry = tmp > MAX_VAL;
        s->num[i] = tmp & MAX_VAL;
    }

    return;
}

// d = a - b
void bignum_sub(bignum *d, const bignum *a, const bignum *b)
{
    // Calculate the two's complement for b.
    b->num[0] = ~b->num[0];
    int carry = ((TMP_TYPE) b->num[0] + 1) > MAX_VAL;
    b->num[0] += 1;
    for (int i = 1; i < b->sz; ++i) {
        b->num[i] = ~b->num[i];
        carry = ((TMP_TYPE) b->num[i] + carry) > MAX_VAL;
        b->num[i] += carry;
    }

    // d = a - b = a + b(two's complement)
    bignum_add(d, a, b);
}

void bignum_lshift(bignum *bn, size_t shift)
{
    shift %= 32;  // only handle shift within 32 bits atm
    if (!shift)
        return;

    // >> debug
    printk("1. %08X %08X %08X %08X", bn->num[3], bn->num[2], bn->num[1],
           bn->num[0]);
    // << debug

    for (int i = bn->sz - 1; i > 0; i--)
        bn->num[i] = bn->num[i] << shift | bn->num[i - 1] >> (32 - shift);
    bn->num[0] <<= shift;

    // >> debug
    printk("2. %08X %08X %08X %08X", bn->num[3], bn->num[2], bn->num[1],
           bn->num[0]);
    // << debug
}
