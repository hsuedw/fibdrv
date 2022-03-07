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

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static DEFINE_MUTEX(fib_mutex);

static long long fib_sequence(long long k)
{
    if (k < 2)
        return k;

    long long fk = -1, fk_2 = 0, fk_1 = 1;
    for (int i = 2; i <= k; i++) {
        fk = fk_2 + fk_1;
        fk_2 = fk_1;
        fk_1 = fk;
    }

    return fk;
}

static int debug_fib_input;

static ssize_t fib_input_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
    return snprintf(buf, 4, "%d\n", debug_fib_input);
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
    long long fk = fib_sequence(debug_fib_input);
    return snprintf(buf, 21, "%lld\n", fk);
}

/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute fib_output_attribute =
    __ATTR(output, S_IRUSR | S_IRGRP, fib_output_show, NULL);

static ssize_t fib_time_show(struct kobject *kobj,
                             struct kobj_attribute *attr,
                             char *buf)
{
    return snprintf(buf, 1, "%d\n", 0);
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
