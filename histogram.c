#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/keyboard.h>

/* LKM information */
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Cesar Belley <cesar.belley@lse.epita.fr>");
MODULE_DESCRIPTION("Histogram of written words.");
MODULE_VERSION("0.1");

/**
** \brief LKM init
**
** \return LKM exit code
*/
static int __init histogram_init(void)
{
    /* Log */
    printk(KERN_INFO "histogram: Hello, World!\n");
    return 0;
}

/**
** \brief LKM exit
*/
static void __exit histogram_exit(void)
{
    /* Log */
    printk(KERN_INFO "histogram: Hello, World!\n");
}

/* Register init and exit functions */
module_init(histogram_init);
module_exit(histogram_exit);