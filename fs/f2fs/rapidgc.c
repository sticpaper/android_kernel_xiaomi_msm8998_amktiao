#include <linux/f2fs_fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "f2fs.h"

#define BUFF_LEN           64
extern struct proc_dir_entry *f2fs_proc_root;
static struct proc_dir_entry *f2fs_node_root;

static int urgent_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "urgent\n");
	return 0;
}

static int urgent_open(struct inode *inode, struct file *file)
{
	return single_open(file, urgent_show, NULL);
}

static ssize_t urgent_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[BUFF_LEN] = {0};
	unsigned int value = 0;

	if (count > BUFF_LEN)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	kstrtouint(buf, 10, &value);
	f2fs_urgent_rapidgc = value;

	return count;
}

static const struct file_operations urgent_ops = {
	.open           = urgent_open,
	.read           = seq_read,
	.write          = urgent_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

void __init f2fs_create_rapidgc_node(void)
{
	if (f2fs_proc_root) {
		f2fs_node_root = proc_mkdir("rapidgc", f2fs_proc_root);
		proc_create("urgent", 0644, f2fs_node_root, &urgent_ops);
	}
}
