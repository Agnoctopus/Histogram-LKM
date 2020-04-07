
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/keyboard.h>
#include <linux/slab.h>
#include <uapi/linux/stat.h>

/* Order matter */
#include <linux/kd.h>
#include <linux/tty.h>
#include <linux/console_struct.h>

#include <stdbool.h>
#include <stddef.h>

/* Paremeters */
#define STR_MAX_LEN 32
#define HT_BUCKETS_NB 128

/* ASCII Code */
#define ASCII_DEL 0x7F

/* State */
static struct dentry *debugfs_dir = NULL;
static char kbd_buffer[STR_MAX_LEN] = { 0 };
static size_t kbd_buffer_pos = 0;

/* LKM information */
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Cesar Belley <cesar.belley@lse.epita.fr>");
MODULE_DESCRIPTION("Histogram of written words.");
MODULE_VERSION("0.1");

/* File operations functions */
static ssize_t histogram_read(struct file *file,
        char __user *buf, size_t len, loff_t *ppos);

/* Keyboard notifier function */
static int kbd_notifier_fn(struct notifier_block *nb,
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
	.notifier_call = kbd_notifier_fn
};

/**
** \brief Check if a char can be considered as a word
*/
static bool is_word(char c)
{
    return c != '\x01'
        && c != '\n'
        && c != '\r'
        && c != ' '
        && c != '\t';
}

/**
** \brief The hash_table_item <char*,int> representation
*/
struct hash_table_item
{
    const char *key;
    int value;
    struct hash_table_item *next;
};

/**
** \brief The hash_table <char*,int> representation
*/
struct hash_table
{
    struct hash_table_item **buckets; /**< The buckets */
    size_t buckets_nb; /** The number of buckets */
};

struct hash_table *histogram = NULL;

/**
** \brief Check if to string are equal
**
** \param str1 The string 1
** \param str2 The string 2
** \return true if equal, false else
*/
static bool streq(const char *str1, const char *str2)
{
    while (*str1 != 0 && *str1 == *str2)
    {
        ++str1;
        ++str2;
    }
    return *str1 == *str2;
}

/**
** \brief Get the length of a string excluding \0
**
** \param str The string
** \return The length of the string
*/
static size_t strlen(const char *str)
{
    size_t length = 0;
    while (str[length] != 0)
        ++length;
    return length;
}

/**
** \brief Dup a string
**
** \param str The string
** \return The dupped string
*/
static char *strdup(const char *str)
{
    size_t i = 0;
    /* Allocate */
    size_t length = strlen(str);
    char *str_cpy = kmalloc(length + 1, GFP_KERNEL);
    for (; i < length; ++i)
        str_cpy[i] = str[i];
    str_cpy[length] = 0;
    return str_cpy;
}

/**
** \brief Jenkins hash function
**
** \param key The key to hash
** \param length The key length
** \return The u32 hash
*/
static uint32_t hash_function(const char *key)
{
    /* Computed hash */
    uint32_t hash = 0;

    /* Coompute */
    for (; *key != 0; ++key)
    {
        hash += *key;
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

/**
** \brief Allocate and initialize a new hash table
**
** \param buckets_nb The number of buckets
** \return The new allocated and initialize hash table
*/
static struct hash_table *ht_init(size_t buckets_nb)
{
    /* Allocate hashset */
    struct hash_table *hash_table =
            kmalloc(sizeof(struct hash_table), GFP_KERNEL);
    if (hash_table == NULL)
        return NULL;
    /* Allocate hashset slots*/
    hash_table->buckets = kcalloc(buckets_nb,
            sizeof(struct hash_table_item *), GFP_KERNEL);
    if (hash_table->buckets == NULL)
    {
        /* Release */
        kfree(hash_table);
        return NULL;
    }
    /* Setup size*/
    hash_table->buckets_nb = buckets_nb;
    return hash_table;
}

/**
** \brief Free a bucket
**
** \param bucket The bucket to free
*/
static void ht_free_bucket(struct hash_table_item *bucket)
{
    /* Get item pointers */
    struct hash_table_item *item = bucket;
    struct hash_table_item *item_tmp = NULL;

    /* Loop through all the bucket */
    while (item != NULL)
    {
        /* Get next */
        item_tmp = item->next;
        /* Free item */
        kfree(item->key);
        kfree(item);

        item = item_tmp;
    }
}

/**
** \brief Free an hash table
**
** \param ht The hash table
*/
static void ht_free(struct hash_table *ht)
{
    /* Loop through the hash table */
    //size_t i = 0;
    //for (; i < ht->buckets_nb; ++i)
    //    ht_free_bucket(ht->buckets[i]);
    /* Free table */
    kfree(ht->buckets);
    kfree(ht);
}

/**
** \brief Allocate and initialize a new hash table item
**
** \param key The key
** \param length The key length
** \param value The associated value indexed by the key
*/
static struct hash_table_item *ht_new_item(char *key, int value)
{
    /* Allocate item */
    struct hash_table_item *node =
            kmalloc(sizeof(struct hash_table_item), GFP_KERNEL);
    if (node == NULL)
        return NULL;

    /* Setup attributes */
    node->key = key;
    node->value = value;
    node->next = NULL;
    return node;
}


/**
** \brief Find the item associated with a key is in a bucket
**
** \param item The first item of the bucket
** \param key The key
** \return The hash item if found, else NULL
*/
static struct hash_table_item *ht_bucket_find(
        struct hash_table_item *item, const char *key)
{
    /* Loop through the bucket */
    while (item != NULL)
    {
        /* Compare */
        if (streq(key, item->key))
            return item;
        item = item->next;
    }
    return NULL;
}

/**
** \brief Increment a value indexed by a key in the hash table.
**        If the value is not present, set it to 0 and increment it.
**
** \param ht The hash table
** \param key The key
*/
static void ht_incr(struct hash_table *ht, char *key)
{
    /* New item */
    struct hash_table_item *item_new = NULL;
    /* Computed the index */
    size_t index = hash_function(key) % ht->buckets_nb;
    struct hash_table_item **bucket_ref = ht->buckets + index;
    struct hash_table_item *item = ht_bucket_find(*bucket_ref, key);

    /* Found */
    if (item != NULL)
    {
        ++item->value;
        return;
    }

    /* New item */
    item_new = ht_new_item(key, 1);
    item_new->next = *bucket_ref;
    *bucket_ref = item_new;
}

static void ht_dump(const struct hash_table *ht)
{
    size_t i = 0;
    struct hash_table_item *item = NULL;
    for (; i < ht->buckets_nb; ++i)
    {
        /* Bucket number */
        pr_info("[%zu] = ", i + 1);

        /* Empty bucket */
        if (ht->buckets[i] == NULL)
            continue;

        /* Loop through bucket */
        item = ht->buckets[i];
        while (item->next != NULL)
        {
            pr_info("%s (%d) -> ", item->key, item->value);
            item = item->next;
        }
        pr_info("%s (%d)", item->key, item->value);
    }
}

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
static int kbd_notifier_fn(struct notifier_block *nb,
        unsigned long action, void *data)
{
    char c;
    /* Get the kbd param */
    struct keyboard_notifier_param *kbd_param = data;

    /* Null data */
    if (kbd_param == NULL)
    {
        /* Log */
        pr_alert("histogram: Failed to get kbd param");
        return NOTIFY_DONE;
    }

    /* Ensure key down */
    if (!kbd_param->down)
        return NOTIFY_DONE;

    pr_info("histogram: ---------------------------");

    switch (action)
    {
    case KBD_KEYCODE:
        pr_debug("histogram: keycode");
        return NOTIFY_DONE;
    case KBD_UNBOUND_KEYCODE:
        pr_debug("histogram: unbound keycode");
        return NOTIFY_DONE;
    case KBD_KEYSYM:
        pr_debug("histogram: keysym");
        break;
    case KBD_POST_KEYSYM:
        pr_debug("histogram: post keysym");
        return NOTIFY_DONE;
    default:
        pr_debug("histogram: default");
        return NOTIFY_DONE;
    }
    c = (char)kbd_param->value;
    pr_info("histogram: action: %lu", action);
    pr_info("histogram: down: %d", kbd_param->down);
    pr_info("histogram: value: %u", kbd_param->value);
    pr_info("histogram: char value: %c", c);
    pr_info("histogram: shift: %d", kbd_param->shift);
    pr_info("histogram: led state: %d", kbd_param->ledstate);
    pr_info("histogram: ---------------------------");


    if (c == ASCII_DEL)
    {
        if (kbd_buffer_pos > 0)
            --kbd_buffer_pos;
    }
    else if (is_word(c))
    {
        if (c >= '!' && c <= '~')
            kbd_buffer[kbd_buffer_pos++] = c;
    }
    else if (kbd_buffer_pos != 0)
    {
        kbd_buffer[kbd_buffer_pos] = 0;
        ht_incr(histogram, strdup(kbd_buffer));
        kbd_buffer_pos = 0;
        ht_dump(histogram);
    }
    /* Kdb buffer length check */
    if (kbd_buffer_pos + 1 >= STR_MAX_LEN)
        kbd_buffer_pos = 0;

    return NOTIFY_OK;
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
    histogram = ht_init(HT_BUCKETS_NB);

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
    ht_free(histogram);
}

/* Register init and exit functions */
module_init(histogram_init);
module_exit(histogram_exit);