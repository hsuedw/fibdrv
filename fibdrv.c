#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

static long long answer;
static ktime_t kt;
static long long (*fib_algo)(long long);

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static DEFINE_MUTEX(fib_mutex);

static long long fib_fast_doubling(long long n)
{
    // reference:
    // https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling

    // Performanc measure strat.
    kt = ktime_get();

    unsigned int h = 0;
    for (unsigned int i = n; i; ++h, i >>= 1)
        ;

    long long a = 0;  // F(0) = 0
    long long b = 1;  // F(1) = 1

    // There is only one `1` in the bits of `mask`.
    // The `1`'s position is same as the highest bit of
    // n(mask = 2^(h-1) at first), and it will be shifted
    // right iteratively to do `AND` operation with `n`
    // to check `n_j` is odd or even, where n_j is defined below.
    for (unsigned int mask = 1 << (h - 1); mask; mask >>= 1) {  // Run h times!
        // Let j = h-i (looping from i = 1 to i = h), n_j = floor(n / 2^j) = n
        // >> j (n_j = n when j = 0), k = floor(n_j / 2), then a = F(k), b =
        // F(k+1) now.
        long long c = a * (2 * b - a);  // F(2k) = F(k) * [ 2 * F(k+1) – F(k) ]
        long long d = a * a + b * b;    // F(2k+1) = F(k)^2 + F(k+1)^2

        if (mask & n) {
            // n_j is odd: k = (n_j-1)/2 => n_j = 2k + 1
            a = d;      //   F(n_j) = F(2k + 1)
            b = c + d;  //   F(n_j + 1) = F(2k + 2) = F(2k) + F(2k + 1)
        } else {
            // n_j is even: k = n_j/2 => n_j = 2k
            a = c;  //   F(n_j) = F(2k)
            b = d;  //   F(n_j + 1) = F(2k + 1)
        }
    }
    // Performanc measure stop.
    kt = ktime_sub(ktime_get(), kt);

    return a;
}

static long long fib_sequence(long long k)
{
    if (k < 2) {
        // Performanc measure strat.
        kt = ktime_get();

        long long ret = k;

        // Performanc measure stop.
        kt = ktime_sub(ktime_get(), kt);

        return ret;
    }

    // Performanc measure strat.
    kt = ktime_get();

    long long fk = -1, fk_2 = 0, fk_1 = 1;
    for (int i = 2; i <= k; i++) {
        fk = fk_2 + fk_1;
        fk_2 = fk_1;
        fk_1 = fk;
    }

    // Performanc measure stop.
    kt = ktime_sub(ktime_get(), kt);

    return fk;
}

static int debug_fib_input;

static ssize_t fib_input_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
    return snprintf(buf, 4096, "%d\n", debug_fib_input);
}

static ssize_t fib_input_store(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               const char *buf,
                               size_t count)
{
    int ret;

    ret = kstrtoint(buf, 10, &debug_fib_input);
    if (ret < 0)
        return ret;

    answer = fib_algo(debug_fib_input);

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
    return snprintf(buf, 4096, "%lld\n", answer);
}

/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute fib_output_attribute =
    __ATTR(output, S_IRUSR | S_IRGRP, fib_output_show, NULL);

static ssize_t fib_time_show(struct kobject *kobj,
                             struct kobj_attribute *attr,
                             char *buf)
{
    return snprintf(buf, 4096, "%llu\n", ktime_to_ns(kt));
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
