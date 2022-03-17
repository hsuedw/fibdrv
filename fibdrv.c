#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include "bignum.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* 1: fast doubling, 0: iteration */
#define FIBONACCI_FAST_DOUBLING (1)

static DEFINE_MUTEX(fib_mutex);
static NUM_TYPE fib_input;
static ktime_t kt;

static ssize_t fib_time_cost(ssize_t (*fib_algo)(NUM_TYPE, char *),
                             NUM_TYPE k,
                             char *buf)
{
    kt = ktime_get();
    ssize_t buf_len = fib_algo(k, buf);
    kt = ktime_sub(ktime_get(), kt);

    return buf_len;
}

#if FIBONACCI_FAST_DOUBLING
static ssize_t fib_fast_doubling(NUM_TYPE k, char *buf)
{
    uint64_t h = 0;
    for (uint64_t i = k; i; ++h, i >>= 1)
        ;

    ssize_t len;
    bignum *a = bignum_create(BIGNUM_SZ);
    bignum *b = bignum_create(BIGNUM_SZ);
    bignum *c = bignum_create(BIGNUM_SZ);
    bignum *d = bignum_create(BIGNUM_SZ);
    bignum *tmp1 = bignum_create(BIGNUM_SZ);
    bignum *tmp2 = bignum_create(BIGNUM_SZ);

    a->num[0] = 0;  // base case, F(0) = 0
    b->num[0] = 1;  // base case, F(1) = 1

    bignum_mult(tmp1, a, a);
    bignum_mult(tmp1, b, b);

    for (int j = h - 1; j >= 0; --j) {
        bignum_cpy(tmp1, b);
        bignum_lshift(tmp1, 1);     // tmp1 = 2 * F(k+1)
        bignum_sub(tmp2, tmp1, a);  // tmp2 = 2 * F(k+1) - F(k)
        bignum_mult(c, a, tmp2);    // F(2k) = F(k) * [2 * F(k+1) - F(k)]

        bignum_mult(tmp1, a, a);    // tmp1 = a * a
        bignum_mult(tmp2, b, b);    // tmp2 = b * b
        bignum_add(d, tmp1, tmp2);  // F(2k+1) = F(k) * F(k) + F(k+1) * F(k+1)

        if ((k >> j) & 1) {
            bignum_cpy(a, d);
            bignum_add(b, c, d);
        } else {
            bignum_cpy(a, c);
            bignum_cpy(b, d);
        }
    }
    len = bignum_to_string(a, buf);

    bignum_destroy(a);
    bignum_destroy(b);
    bignum_destroy(c);
    bignum_destroy(d);
    bignum_destroy(tmp1);
    bignum_destroy(tmp2);

    return len;
}

#else
static ssize_t fib_sequence(NUM_TYPE k, char *buf)
{
    ssize_t len;
    if (k < 2) {
        bignum *bn = bignum_create(BIGNUM_SZ);
        bn->num[0] = (NUM_TYPE) k;
        len = bignum_to_string(bn, buf);
        return len;
    }

    bignum *fk = bignum_create(BIGNUM_SZ);
    bignum *fk1 = bignum_create(BIGNUM_SZ);
    bignum *fk2 = bignum_create(BIGNUM_SZ);

    // base cases
    fk1->num[0] = 1;
    fk2->num[0] = 0;

    for (int i = 2; i <= k; i++) {
        bignum_add(fk, fk1, fk2);
        bignum_cpy(fk2, fk1);
        bignum_cpy(fk1, fk);
    }
    len = bignum_to_string(fk, buf);

    bignum_destroy(fk);
    bignum_destroy(fk1);
    bignum_destroy(fk2);

    return len;
}
#endif

static ssize_t fib_input_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
    return snprintf(buf, sizeof(NUM_TYPE) * 2, "%X\n", fib_input);
}

static ssize_t fib_input_store(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               const char *buf,
                               size_t count)
{
    int ret;

    ret = kstrtoint(buf, 10, &fib_input);
    if (ret < 0)
        return ret;

    return count;
}

/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute fib_input_attribute =
    __ATTR(input,
           S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
           fib_input_show,
           fib_input_store);


static ssize_t fib_output_show(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               char *buf)
{
#if FIBONACCI_FAST_DOUBLING
    return fib_time_cost(fib_fast_doubling, fib_input, buf);
#else
    return fib_time_cost(fib_sequence, fib_input, buf);
#endif
}

/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute fib_output_attribute =
    __ATTR(output, S_IRUSR | S_IRGRP, fib_output_show, NULL);

static ssize_t fib_time_show(struct kobject *kobj,
                             struct kobj_attribute *attr,
                             char *buf)
{
    // return snprintf(buf, sizeof(u64) * 2 + 1, "%llu\n", ktime_to_ns(kt));
    return snprintf(buf, 100, "%llu\n", ktime_to_ns(kt));
}

/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute fib_time_attribute =
    __ATTR(time, S_IRUSR | S_IRGRP, fib_time_show, NULL);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *attrs[] = {
    &fib_input_attribute.attr, &fib_output_attribute.attr,
    &fib_time_attribute.attr,
    NULL, /* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
    .attrs = attrs,
};

static struct kobject *fib_kobj;

static int __init init_fib_dev(void)
{
    int retval;

    mutex_init(&fib_mutex);

    /*
     * Create a kobject with the name of "fibonacci",
     * located under /sys/kernel/
     */
    fib_kobj = kobject_create_and_add(DEV_FIBONACCI_NAME, kernel_kobj);
    if (!fib_kobj)
        return -ENOMEM;

    /* Create the files associated with this kobject */
    retval = sysfs_create_group(fib_kobj, &attr_group);
    if (retval)
        kobject_put(fib_kobj);

    return retval;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    kobject_put(fib_kobj);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
