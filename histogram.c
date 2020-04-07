#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/keyboard.h>
#include <uapi/linux/stat.h>

#include <stddef.h>


/* State */
static struct dentry *debugfs_dir = NULL;

/* LKM information */
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Cesar Belley <cesar.belley@lse.epita.fr>");
MODULE_DESCRIPTION("Histogram of written words.");
MODULE_VERSION("0.1");

/* File operations functions */
static ssize_t histogram_read(struct file *file,
        char __user *buf, size_t len, loff_t *ppos);

/* Keyboard notifier function */
static int kdb_notifier_fn(struct notifier_block *nb,
        unsigned long action, void *data);

/**
** \brief File operations
*/
static struct file_operations file_ops = {
    .owner = THIS_MODULE,
    .read = histogram_read
};

/**
** \brief Notifier block for keyboard
*/
struct notifier_block kbd_notifier_blk = {
	.notifier_call = kdb_notifier_fn
};


/**
** \brief Read the histogram
**
** \param file The file
** \param bug The read buffer
** \param len The length of the read buffer
** \param ppos the position in the file
*/
static ssize_t histogram_read(struct file *file,
        char __user *buf, size_t len, loff_t *ppos)
{
    return 0;
}

/**
** \brief Keyboard notifier handler
**
** \param nb The notifier block
** \param action The action
** \param data The data
*/
static int kdb_notifier_fn(struct notifier_block *nb,
        unsigned long action, void *data)
{
    return 0;
}

/**
** \brief LKM init
**
** \return LKM exit code
*/
static int __init histogram_init(void)
{
    /* Debug file */
    struct dentry *debugfs_file = NULL;

    /* Log */
    pr_info("histogram: init\n");

    /* Debug directory creation */
    debugfs_dir = debugfs_create_dir("histogram", NULL);
    if (debugfs_dir == NULL)
    {
        /* Log */
        pr_alert("histogram: Failed to create debugfs dir");
        return -1;
    }
    /* File creation */
    debugfs_file = debugfs_create_file("histogram",
            S_IRUSR, debugfs_dir, NULL, &file_ops);
    if (debugfs_file == NULL)
    {
        /* Log */
        pr_alert("histogram: Failed to create debugfs dir");
        /* Release */
        debugfs_remove_recursive(debugfs_dir);
        return -1;
    }
    /* Register keyboard notifier */
    if (register_keyboard_notifier(&kbd_notifier_blk) != 0)
    {
        /* Log */
        pr_alert("histogram: Failed to register keyboard notifier");
        /* Release */
        debugfs_remove_recursive(debugfs_dir);
        return -1;
    }
    return 0;
}

/**
** \brief LKM exit
*/
static void __exit histogram_exit(void)
{
    /* Log */
    pr_info("histogram: exit\n");

    /* Release */
    unregister_keyboard_notifier(&kbd_notifier_blk);
    debugfs_remove_recursive(debugfs_dir);
}

/* Register init and exit functions */
module_init(histogram_init);
module_exit(histogram_exit);