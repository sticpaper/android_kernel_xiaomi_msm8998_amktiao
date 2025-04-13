// SPDX-License-Identifier: GPL-2.0
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>

extern bool force_reboot_rec;
extern bool force_warm_reboot;

#define POWERKEY_PANIC_DELAY_MS 5000

static int powerkey_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "warm_reboot: %s reboot_rec: %s\n",
		force_warm_reboot ? "enabled" : "disable",
		force_reboot_rec ? "enabled" : "disable");
	return 0;
}

static int powerkey_open(struct inode *inode, struct file *file)
{
	return single_open(file, powerkey_show, NULL);
}

static ssize_t powerkey_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[60] = {0};
	int warm_reboot = 0;
	int reboot_rec = 0;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	sscanf(buf, "%d,%d", &warm_reboot, &reboot_rec);
	force_warm_reboot = !!warm_reboot;
	force_reboot_rec = !!reboot_rec;
	return count;
}

static void powerkey_panic_work(struct work_struct *unused)
{
	force_warm_reboot = true;
	kernel_restart("powerkey_panic");
}

static DECLARE_DELAYED_WORK(powerkey_panic_worker, powerkey_panic_work);
static void powerkey_panic_event(struct input_handle *handle,
			unsigned int type, unsigned int code, int value)
{
	if (type == EV_KEY && code == KEY_POWER) {
		switch (value) {
		case 0:
			cancel_delayed_work(&powerkey_panic_worker);
			break;
		case 1:
			queue_delayed_work(system_highpri_wq, &powerkey_panic_worker,
					   msecs_to_jiffies(POWERKEY_PANIC_DELAY_MS));
			break;
		}
	}
}

static int powerkey_panic_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "powerkey_panic";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void powerkey_panic_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct file_operations powerkey_ops = {
	.open           = powerkey_open,
	.read           = seq_read,
	.write          = powerkey_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static const struct input_device_id powerkey_ids[] = {
	/* Keypad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{ },
};

static struct input_handler powerkey_panic_handler = {
	.event = powerkey_panic_event,
	.connect = powerkey_panic_connect,
	.disconnect = powerkey_panic_disconnect,
	.name = "powerkey_panic_handler",
	.id_table = powerkey_ids,
};

static int __init powerkey_panic_init(void)
{
#ifdef CONFIG_POWERKEY_FORCE_REBOOT_REC
	force_reboot_rec = true;
#endif
#ifdef CONFIG_POWERKEY_FORCE_WARM_REBOOT
	force_warm_reboot = true;
#endif

	proc_mkdir("powerkey", NULL);
	proc_create("powerkey/reboot", 0664, NULL, &powerkey_ops);
	input_register_handler(&powerkey_panic_handler);

	return 0;
}
module_init(powerkey_panic_init);
