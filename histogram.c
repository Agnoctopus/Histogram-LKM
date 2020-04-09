
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

/* Usefull definitions */
#include <stdbool.h>
#include <stddef.h>

/* Paremeters */
#define STR_MAX_LEN 32
#define HT_BUCKETS_NB 128

/* ASCII Code */
#define ASCII_DEL 0x7F

/* LKM informations */
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Cesar Belley <cesar.belley@lse.epita.fr>");
MODULE_DESCRIPTION("Histogram of written words.");
MODULE_VERSION("0.1");

/* File operations functions */
static ssize_t histogram_read(struct file *file,
        char __user *buf, size_t len, loff_t *ppos);
static int histogram_open(struct inode *inode, struct file* file);
static int histogram_release(struct inode *inode, struct file *file);

/**
** \brief File operations
*/
static struct file_operations file_ops = {
    .owner = THIS_MODULE,
    .read = histogram_read,
    .open = histogram_open,
    .release = histogram_release
};

/* Keyboard notifier function */
static int kbd_notifier_fn(struct notifier_block *nb,
        unsigned long action, void *data);

/**
** \brief Notifier block for keyboard
*/
struct notifier_block kbd_notifier_blk = {
	.notifier_call = kbd_notifier_fn
};

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

/* State */
static struct dentry *debugfs_dir = NULL;
static char kbd_buffer[STR_MAX_LEN] = { 0 };
static size_t kbd_buffer_pos = 0;
static int device_open_count = 0;
static char *histogram_string = NULL;
struct hash_table *histogram = NULL;

/**
** \brief Check if a char can be considered as a word
**
** \param c The char
** \return true if can be considered as word, else false
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
** \brief Check if a char can be considered as a printable ascii
**
** \param c The char
** \return true if can be considered as a printable ascii, else false
*/
static bool is_printable_ascii(char c)
{
    return c >= '!'
        && c <= '~';
}

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
    /* Variables */
    size_t i = 0;
    /* Allocate */
    size_t length = strlen(str);
    char *str_cpy = kmalloc(length + 1, GFP_KERNEL);
    if (str_cpy == NULL)
        return NULL;
    /* Loop though string */
    for (; i < length; ++i)
        str_cpy[i] = str[i];
    /* Null terminated */
    str_cpy[length] = 0;
    return str_cpy;
}

/**
** \brief Jenkins hash function
**
** \param key The key to hash
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
    /* Allocate hash table */
    struct hash_table *hash_table =
            kmalloc(sizeof(struct hash_table), GFP_KERNEL);
    if (hash_table == NULL)
        return NULL;
    /* Allocate hash table buckets */
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
** \param item The first item of the bucket to free
*/
static void ht_free_bucket(struct hash_table_item *item)
{
    /* Get item pointers */
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
    size_t i = 0;
    for (; i < ht->buckets_nb; ++i)
        ht_free_bucket(ht->buckets[i]);
    /* Free table */
    kfree(ht->buckets);
    kfree(ht);
}

/**
** \brief Allocate and initialize a new hash table item
**
** \param key The key
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
**        If the value is not present, set it to 0 and increment it
**
** \param ht The hash table
** \param key The key
*/
static void ht_incr(struct hash_table *ht, const char *key)
{
    /* Variables */
    struct hash_table_item *item_new = NULL;
    char *key_cpy = NULL;
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
    key_cpy = strdup(kbd_buffer);
    if (key_cpy == NULL)
        return;
    item_new = ht_new_item(key_cpy, 1);
    if (item_new == NULL)
    {
        kfree(key_cpy);
        return;
    }
    /* Pointer move */
    item_new->next = *bucket_ref;
    *bucket_ref = item_new;
}

/**
** \brief Keyboard keysym action handler
**
** \param value The keysym action value
*/
static void kdb_handle_keysym(unsigned int value)
{
    /* Cast to character */
    char c = (char)value;

    /* Del touch */
    if (c == ASCII_DEL)
    {
        if (kbd_buffer_pos > 0)
            --kbd_buffer_pos;
        return;
    }

    /* Word switch */
    if (is_word(c))
    {
        if (is_printable_ascii(c))
            kbd_buffer[kbd_buffer_pos++] = c;
    }
    else if (kbd_buffer_pos != 0)
    {
        /* A word is in the buffer, incr it */
        /* Null terminated */
        kbd_buffer[kbd_buffer_pos] = 0;
        ht_incr(histogram, kbd_buffer);
        kbd_buffer_pos = 0;
    }
    /* Kdb buffer length check */
    if (kbd_buffer_pos + 1 >= STR_MAX_LEN)
        kbd_buffer_pos = 0;
}

/**
** \brief Keyboard action handler
**
** \param action The action
** \param value The associated value
** \return 1 if the action was treated, 0 if ignored
*/
static int kdb_handle_action(unsigned long action,
        unsigned int value)
{
    /* Switch all possibles actions */
    switch (action)
    {
    case KBD_KEYCODE:
        pr_debug("histogram: keycode");
        return 0;
    case KBD_UNBOUND_KEYCODE:
        pr_debug("histogram: unbound keycode");
        return 0;
    case KBD_KEYSYM:
        pr_debug("histogram: keysym");
        kdb_handle_keysym(value);
        return 1;
    case KBD_POST_KEYSYM:
        pr_debug("histogram: post keysym");
        return 0;
    default:
        pr_debug("histogram: default");
        return 0;
    }
    /* Should not be executed */

    return 0;
}

/**
** \brief Keyboard notifier handler
**
** \param nb The notifier block
** \param action The action
** \param data The data
** \return notifier status
*/
static int kbd_notifier_fn(struct notifier_block *nb,
        unsigned long action, void *data)
{
    /* Get the kbd param */
    const struct keyboard_notifier_param *kbd_param = data;

    /* Null data */
    if (kbd_param == NULL)
    {
        /* Log */
        pr_alert("histogram: Failed to get kbd param");
        return NOTIFY_DONE;
    }

    /* Ensure key down */
    if (kbd_param->down == 0)
        return NOTIFY_DONE;

    /* Handle action */
    if (kdb_handle_action(action, kbd_param->value) == 1)
        return NOTIFY_OK;
    return NOTIFY_DONE;
}

/**
** \brief Get the string representation of the histogram
**
** \return The string representation of the histogram
*/
static char *histogram_tostring(void)
{
    /* Variables */
    struct hash_table_item *item = NULL;
    char *buffer_tmp = NULL;
    size_t space = (1 << 10);
    size_t index = 0;
    size_t i = 0;

    /* Main buffer */
    char *buffer = kmalloc(space, GFP_KERNEL);
    if (buffer == NULL)
        return NULL;

    /* Loop through buckets */
    for (; i < histogram->buckets_nb; ++i)
    {
        /* Empty bucket */
        if (histogram->buckets[i] == NULL)
            continue;

        /* Loop through bucket */
        item = histogram->buckets[i];
        while (item != NULL)
        {
            /* Add to the buffer */
            index += snprintf(buffer + index, STR_MAX_LEN,
                    "%s: %d\n", item->key, item->value);

            /* Check buffer size to reallocate */
            if (index + 16 + STR_MAX_LEN >=  space)
            {
                /* Allocate double space */
                space *= 2;
                buffer_tmp = kmalloc(space, GFP_KERNEL);
                if (buffer_tmp == NULL)
                {
                    kfree(buffer);
                    return NULL;
                }

                /* Copy */
                memcpy(buffer_tmp, buffer, index);

                /* Change */
                kfree(buffer);
                buffer = buffer_tmp;
            }
            item = item->next;
        }
    }
    return buffer;
}

/**
** \brief Read the histogram debugfs file
**
** \param file The file
** \param buf The read buffer
** \param len The length of the read buffer
** \param ppos the position in the file
** \return The number of bytes readed
*/
static ssize_t histogram_read(struct file *file,
        char __user *buf, size_t len, loff_t *ppos)
{
    /* Log */
    pr_debug("histogram: read");

    /* Simple read */
    return simple_read_from_buffer(buf, len, ppos,
            histogram_string, strlen(histogram_string));
}

/**
** \brief Open the histogram debugfs file
**
** \param inode The corresponding inode
** \param file The corresponding file
** \return 0 on success, negative value otherwise
*/
static int histogram_open(struct inode *inode, struct file* file)
{
    /* Log */
    pr_debug("histogram: open");

    /* Check busy */
    if (device_open_count)
        return -EBUSY;
    device_open_count++;
    try_module_get(THIS_MODULE);

    /* Build string */
    histogram_string = histogram_tostring();
    if (histogram_string == NULL)
        return -1;

    return 0;
}

/**
** \brief Release the histogram debugfs file
**
** \param inode The corresponding inode
** \param file The corresponding file
** \return 0 on success, negative value otherwise
*/
static int histogram_release(struct inode *inode, struct file *file)
{
    /* Log */
    pr_debug("histogram: close");

    /* Free string */
    kfree(histogram_string);

    /* Uncheck busy */
    device_open_count--;
    module_put(THIS_MODULE);
    return 0;
}

/**
** \brief Init the histogram linux kernel module
**
** \return 0 on success, negative value otherwise
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

    /* Histogram hash table */
    histogram = ht_init(HT_BUCKETS_NB);
    if (histogram == NULL)
    {
        /* Log */
        pr_alert("histogram: Failed to init the histogram table");
        /* Release */
        debugfs_remove_recursive(debugfs_dir);
        unregister_keyboard_notifier(&kbd_notifier_blk);
        return -1;
    }

    return 0;
}

/**
** \brief Exit the histogram linux kernel module
**
** \return 0 on success, negative value otherwise
*/
static void __exit histogram_exit(void)
{
    /* Log */
    pr_info("histogram: exit\n");

    /* Release */
    debugfs_remove_recursive(debugfs_dir);
    unregister_keyboard_notifier(&kbd_notifier_blk);
    ht_free(histogram);
}

/* Register init and exit functions */
module_init(histogram_init);
module_exit(histogram_exit);