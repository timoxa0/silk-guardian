#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/reboot.h>

#include <linux/proc_fs.h>

#include "config.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Greg Kroah-Hartman and Nate Brune, adjusted by kmille and tx0");
MODULE_DESCRIPTION("A module that protects you from having a terrible horrible no good very bad day.");


static struct proc_dir_entry *ent;
static int enabled = 1;

#define BUFSIZE 2000

// called on echo 0 > /proc/silk
static ssize_t write_callback(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[BUFSIZE];
    int num, i, len;
	
    if(*ppos > 0 || count > BUFSIZE)
		return -EFAULT;

    if(copy_from_user(buf, ubuf, count))
		return -EFAULT;

    i = sscanf(buf, "%d", &num);
    if (i != 1) {
        printk("Could not convert input to a number. Got: %s\n", buf);
        return -EFAULT;
    }

    if (num == 0) {
        enabled = 0;
        pr_info("turned off\n");
    } else if (num == 1) {
        enabled = 1;
        pr_info("turned on\n");
    } else {
        printk("Invalid input: %d (use 1 to enable, 0 to disable)\n", num);
        return -EFAULT;
    }

    len = strlen(buf);
    *ppos = len;
    return len;
}

// called on cat /proc/silk
static ssize_t read_callback(struct file *file, char __user *ubuf,size_t count, loff_t *ppos)
{
    char buf[BUFSIZE];
	int i, len = 0;
    const struct usb_device_id* ptr = devlist_table;
    const struct usb_device_id* endPtr = devlist_table + sizeof(devlist_table)/sizeof(devlist_table[0]);
    
    if(*ppos > 0 || count < BUFSIZE)
		return 0;
    
    if (enabled) {
	    len = sprintf(buf, "silk is turned on\n");
    } else {
	    len = sprintf(buf, "silk is turned off\n");
    }
	    
    len += sprintf(buf + len, "version: %s\n", silk_version);
#ifdef USE_SHELL_COMMAND
    // silly buffer overflow prevention?
    if(strlen(kill_command_argv[2]) < 100) {
        len += sprintf(buf + len, "shell command is enabled: %s\n", kill_command_argv[2]);
    } else {
        len += sprintf(buf + len, "shell command is enabled\n");
    }
#else
    len += sprintf(buf + len, "shell command is disabled\n");
#endif
    
    // i is only used for buffer overflow prevention ...
    while ( ptr < endPtr && i < 50 ){
#ifdef DEVLIST_IS_WHITELIST
       len += sprintf(buf + len, "whitelisted device: 0x%04x, 0x%04x\n", ptr->idVendor, ptr->idProduct);
#else
       len += sprintf(buf + len, "blacklisted device: 0x%04x, 0x%04x\n", ptr->idVendor, ptr->idProduct);
#endif
       ptr++;
       i++;
    }

    if(copy_to_user(ubuf,buf,len))
		return -EFAULT;
	*ppos = len;
	return len;

}



static void panic_time(struct usb_device *usb)
{
  int ret = 0;

  if (!enabled) {
    pr_info("silk: currently disabled, doing nothing\n");
    return;
  }
  
#ifdef USE_SHELL_COMMAND
    printk("silk: running shell command\n");
    ret = call_usermodehelper(kill_command_argv[0], kill_command_argv, NULL, UMH_WAIT_EXEC);
    if (ret != 0) {
        printk("silk: error in call to usermodehelper: %i\n", ret);
        return;
    }
#endif
  
#ifdef USE_SHUTDOWN
  struct device *dev;
    for (dev = &usb->dev; dev; dev = dev->parent)
        mutex_unlock(&dev->mutex);
    printk("silk: syncing && powering off\n");
    orderly_poweroff(true);
#endif
}


/*
 * returns 0 if no match, 1 if match
 *
 * Taken from drivers/usb/core/driver.c, as it's not exported for our use :(
 */
static int usb_match_device(struct usb_device *dev,
          const struct usb_device_id *id)
{
  if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
      id->idVendor != le16_to_cpu(dev->descriptor.idVendor))
    return 0;

  if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
      id->idProduct != le16_to_cpu(dev->descriptor.idProduct))
    return 0;

  /* No need to test id->bcdDevice_lo != 0, since 0 is never
     greater than any unsigned number. */
  if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
      (id->bcdDevice_lo > le16_to_cpu(dev->descriptor.bcdDevice)))
    return 0;

  if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
      
          (id->bcdDevice_hi < le16_to_cpu(dev->descriptor.bcdDevice)))
    return 0;

  if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
      (id->bDeviceClass != dev->descriptor.bDeviceClass))
    return 0;

  if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
      (id->bDeviceSubClass != dev->descriptor.bDeviceSubClass))
    return 0;

  if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
      (id->bDeviceProtocol != dev->descriptor.bDeviceProtocol))
    return 0;

  return 1;
}



static void usb_dev_change(struct usb_device *dev)
{
  const struct usb_device_id *dev_id;

  /* Check our devlist to see if we want to ignore this device */
   unsigned long devlist_len = sizeof(devlist_table)/sizeof(devlist_table[0]);
   int i; // GNU89 standard
   for(i = 0; i < devlist_len; i++)
   {
      dev_id = &devlist_table[i];
#ifdef DEVLIST_IS_WHITELIST
      if (usb_match_device(dev, dev_id))
#else
      if (!usb_match_device(dev, dev_id))
#endif
      {
         pr_info("silk: device 0x%04x, 0x%04x is ignored\n", dev_id->idVendor, dev_id->idProduct);
         return;
      }
   }

#ifdef DEVLIST_IS_WHITELIST
  pr_info("silk: unknown device 0x%04x, 0x%04x (not whitelisted)\n", dev->descriptor.idVendor, dev->descriptor.idProduct);
#else
  pr_info("silk: blacklisted device 0x%04x, 0x%04x\n", dev->descriptor.idVendor, dev->descriptor.idProduct);
#endif
  panic_time(dev);
}

static int notify(struct notifier_block *self, unsigned long action, void *dev)
{
  switch (action) {
  case USB_DEVICE_ADD:
    /* We added a new device, lets check if its known */
    usb_dev_change(dev);
    break;
  case USB_DEVICE_REMOVE:
    /* A USB device was removed, possibly as security measure */
    usb_dev_change(dev);
    break;
  default:
    break;
  }
  return 0;
}

static struct proc_ops myops =
{
	.proc_read = read_callback,
	.proc_write = write_callback,
};

static struct notifier_block usb_notify = {
  .notifier_call = notify,
};

static int __init silk_init(void)
{
  usb_register_notify(&usb_notify);

  ent = proc_create("silk", 0600, NULL, &myops);
  if (ent == NULL)
	  return -ENOMEM;
  pr_info("silk: now watching USB devices\n");
  return 0;
}
module_init(silk_init);

static void __exit silk_exit(void)
{
  usb_unregister_notify(&usb_notify);
  proc_remove(ent);
  pr_info("silk: no longer watching USB devices\n");
}
module_exit(silk_exit);
