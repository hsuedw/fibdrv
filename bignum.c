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

// s = a + b
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

// bn = bn << shift
void bignum_lshift(bignum *bn, size_t shift)
{
    shift %= 32;  // only handle shift within 32 bits atm
    if (!shift)
        return;

    for (int i = bn->sz - 1; i > 0; i--)
        bn->num[i] = bn->num[i] << shift | bn->num[i - 1] >> (32 - shift);
    bn->num[0] <<= shift;
}

static void bn_mult_add(bignum *p, int offset, TMP_TYPE x)
{
    TMP_TYPE carry = 0;
    for (int i = offset; i < p->sz; ++i) {
        carry += p->num[i] + (x & MAX_VAL);
        p->num[i] = carry;
        carry >>= 32;
        x >>= 32;
        if (!x && !carry)
            return;
    }
}

// p = a * b
void bignum_mult(bignum *p, const bignum *a, const bignum *b)
{
    // >> debug
    printk("1. a: %08x %08x %08x %08x %08x %08x %08x %08x", a->num[7],
           a->num[6], a->num[5], a->num[4], a->num[3], a->num[2], a->num[1],
           a->num[0]);
    printk("1. b: %08x %08x %08x %08x %08x %08x %08x %08x", b->num[7],
           b->num[6], b->num[5], b->num[4], b->num[3], b->num[2], b->num[1],
           b->num[0]);
    printk("1. p %08x %08x %08x %08x %08x %08x %08x %08x", p->num[7], p->num[6],
           p->num[5], p->num[4], p->num[3], p->num[2], p->num[1], p->num[0]);
    // << debug

    for (int i = 0; i < a->sz; ++i)
        for (int j = 0; j < b->sz; ++j) {
            TMP_TYPE carry = (TMP_TYPE) a->num[i] * b->num[j];
            bn_mult_add(p, i + j, carry);
        }

    // >> debug
    printk("2. a: %08x %08x %08x %08x %08x %08x %08x %08x", a->num[7],
           a->num[6], a->num[5], a->num[4], a->num[3], a->num[2], a->num[1],
           a->num[0]);
    printk("2. b: %08x %08x %08x %08x %08x %08x %08x %08x", b->num[7],
           b->num[6], b->num[5], b->num[4], b->num[3], b->num[2], b->num[1],
           b->num[0]);
    printk("2. p %08x %08x %08x %08x %08x %08x %08x %08x", p->num[7], p->num[6],
           p->num[5], p->num[4], p->num[3], p->num[2], p->num[1], p->num[0]);
    // << debug
}
