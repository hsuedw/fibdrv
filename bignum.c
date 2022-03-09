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
