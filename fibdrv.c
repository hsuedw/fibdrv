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

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static DEFINE_MUTEX(fib_mutex);

static ssize_t fib_sequence(long long k, char *buf)
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

    // FIXME: The following code are just to pass static analysis.
    //        Delete them once they are used in fast doubling
    bignum_lshift(fk, 2);

    fk->num[0] = 0;
    fk->num[1] = 0;
    fk->num[2] = 0;
    fk->num[3] = 0;
    fk1->num[0] = 0xfa9946c1;
    fk1->num[1] = 0x55b0bdd8;
    fk1->num[2] = 0x00000007;
    fk1->num[3] = 0x00000000;
    fk2->num[0] = 0xcafb7902;
    fk2->num[1] = 0xde2ab8ce;
    fk2->num[2] = 0x0000000b;
    fk2->num[3] = 0x00000000;
    bignum_sub(fk, fk1, fk2);

    fk->num[0] = 0;
    fk->num[1] = 0;
    fk->num[2] = 0;
    fk->num[3] = 0;
    fk->num[4] = 0;
    fk->num[5] = 0;
    fk->num[6] = 0;
    fk->num[7] = 0;
    fk1->num[0] = 0xcafb7902;
    fk1->num[1] = 0xde2ab8ce;
    fk1->num[2] = 0xb;
    fk1->num[3] = 0x0;
    fk1->num[4] = 0x0;
    fk1->num[5] = 0x0;
    fk1->num[6] = 0x0;
    fk1->num[7] = 0x0;

    fk2->num[0] = 0xc594bfc3;
    fk2->num[1] = 0x33db76a7;
    fk2->num[2] = 0x13;
    fk2->num[3] = 0x0;
    fk2->num[4] = 0x0;
    fk2->num[5] = 0x0;
    fk2->num[6] = 0x0;
    fk2->num[7] = 0x0;
    bignum_mult(fk, fk1, fk2);
    // FIXME, end

    bignum_destroy(fk);
    bignum_destroy(fk1);
    bignum_destroy(fk2);

    return len;
}

static int fib_input;

static ssize_t fib_input_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
    return snprintf(buf, 4, "%d\n", fib_input);
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
    return fib_sequence(fib_input, buf);
}

/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute fib_output_attribute =
    __ATTR(output, S_IRUSR | S_IRGRP, fib_output_show, NULL);

static ssize_t fib_time_show(struct kobject *kobj,
                             struct kobj_attribute *attr,
                             char *buf)
{
    return snprintf(buf, 10, "%d\n", 0);
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
