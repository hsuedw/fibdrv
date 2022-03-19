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
static bignum *answer;
static void (*fib_algo)(NUM_TYPE);

static void fib_fast_doubling(NUM_TYPE k)
{
    bignum *a = bignum_create(BIGNUM_SZ);
    bignum *b = bignum_create(BIGNUM_SZ);
    bignum *c = bignum_create(BIGNUM_SZ);
    bignum *d = bignum_create(BIGNUM_SZ);
    bignum *tmp1 = bignum_create(BIGNUM_SZ);
    bignum *tmp2 = bignum_create(BIGNUM_SZ);

    // Performanc measure strat.
    kt = ktime_get();

    uint64_t h = 0;
    for (uint64_t i = k; i; ++h, i >>= 1)
        ;

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
    // len = bignum_to_string(a, buf);
    answer = bignum_create(BIGNUM_SZ);
    bignum_cpy(answer, a);

    // Performanc measure stop.
    kt = ktime_sub(ktime_get(), kt);

    bignum_destroy(a);
    bignum_destroy(b);
    bignum_destroy(c);
    bignum_destroy(d);
    bignum_destroy(tmp1);
    bignum_destroy(tmp2);
}

static void fib_sequence(NUM_TYPE k)
{
    if (k < 2) {
        bignum *bn = bignum_create(BIGNUM_SZ);

        // Performanc measure strat.
        kt = ktime_get();

        bn->num[0] = (NUM_TYPE) k;
        // len = bignum_to_string(bn, buf);
        answer = bignum_create(BIGNUM_SZ);
        bignum_cpy(answer, bn);

        // Performanc measure stop.
        kt = ktime_sub(ktime_get(), kt);

        bignum_destroy(bn);
        return;
    }

    bignum *fk = bignum_create(BIGNUM_SZ);
    bignum *fk1 = bignum_create(BIGNUM_SZ);
    bignum *fk2 = bignum_create(BIGNUM_SZ);

    // Performanc measure strat.
    kt = ktime_get();

    // base cases
    fk1->num[0] = 1;
    fk2->num[0] = 0;

    for (int i = 2; i <= k; i++) {
        bignum_add(fk, fk1, fk2);
        bignum_cpy(fk2, fk1);
        bignum_cpy(fk1, fk);
    }
    // len = bignum_to_string(fk, buf);
    answer = bignum_create(BIGNUM_SZ);
    bignum_cpy(answer, fk);

    // Performanc measure stop.
    kt = ktime_sub(ktime_get(), kt);

    bignum_destroy(fk);
    bignum_destroy(fk1);
    bignum_destroy(fk2);
}

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

    fib_algo(fib_input);

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
    ssize_t len = bignum_to_string(answer, buf);
    bignum_destroy(answer);

    return len;
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

static ssize_t fib_algo_show(struct kobject *kobj,
                             struct kobj_attribute *attr,
                             char *buf)
{
    if (fib_algo == fib_sequence)
        return snprintf(buf, 11, "%s\n", "iteration");

    // fib_algo == fib_fast_doubling
    return snprintf(buf, 15, "%s\n", "fast-doubling");
}

static ssize_t fib_algo_store(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              const char *buf,
                              size_t count)
{
    if (strncmp(buf, "iteration", 9) == 0) {
        /* Set the algorithm to be iteration. */
        pr_info("%s: Set the algorithm to be iteration.\n", KBUILD_MODNAME);
        fib_algo = fib_sequence;
    } else if (strncmp(buf, "fast-doubling", 13) == 0) {
        /* Set the algorithm to be fast-doubling. */
        pr_info("%s: Set the algorithm to be fast-doubling.\n", KBUILD_MODNAME);
        fib_algo = fib_fast_doubling;
    } else {
        pr_info("%s: %s is not support.\n", KBUILD_MODNAME, buf);
        pr_info("%s: Set the algorithm to be fast-doubling.\n", KBUILD_MODNAME);
        fib_algo = fib_fast_doubling;
    }
    return strlen(buf);
}

/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute fib_algo_attribute =
    __ATTR(algo,
           S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
           fib_algo_show,
           fib_algo_store);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *attrs[] = {
    &fib_input_attribute.attr,
    &fib_output_attribute.attr,
    &fib_time_attribute.attr,
    &fib_algo_attribute.attr,
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

    /* The default algorithm for calculating a
     * Fibonacci number is fast doubling.
     */
    fib_algo = fib_fast_doubling;

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
