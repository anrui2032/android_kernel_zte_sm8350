#include <linux/init.h>
#include<linux/slab.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/kern_levels.h>
#include <linux/version.h>

#ifndef CONFIG_MESSAGE_LOGLEVEL_DEFAULT
#define CONFIG_MESSAGE_LOGLEVEL_DEFAULT 4
#endif

struct printk_ext_log {
	u16 len;            /* length of entire record */
	u16 text_len;       /* length of text buffer */
	bool newline;       /* newline record */
	u8 level:3;         /* log level */
	unsigned int process_id;
	pid_t pid;
	char comm[TASK_COMM_LEN];
	struct timespec ts;
};

#define get_max(a, b)       (((a) > (b)) ? (a) : (b))

#define PREFIX_MAX          128
#define LOG_LINE_MAX        (2048 - PREFIX_MAX)

/* record buffer */
#define LOG_BUF_SHIFT 14
#define LOG_ALIGN __alignof__(struct printk_ext_log)
#define __LOG_BUF_EXT_LEN (1 << LOG_BUF_SHIFT)

static char __log_buf_ext[__LOG_BUF_EXT_LEN] __aligned(LOG_ALIGN);
static char *log_buf_ext = __log_buf_ext;
static u32 log_buf_len = __LOG_BUF_EXT_LEN;

/* the next printk record to read by /proc/kmsg_ext */
static u64 log_read_seq;
static u32 log_read_idx;
static size_t log_partial;

/* index and sequence number of the first record stored in the buffer */
static u64 log_first_seq;
static u32 log_first_idx;

/* index and sequence number of the next record to store in the buffer */
static u64 log_next_seq;
static u32 log_next_idx;

DECLARE_WAIT_QUEUE_HEAD(msg_wait);
DEFINE_RAW_SPINLOCK(buf_lock);

/* human readable text of the record */
static char *log_text(const struct printk_ext_log *msg)
{
	return (char *)msg + sizeof(struct printk_ext_log);
}

/* get next record; idx must point to valid msg */
static u32 msg_next(u32 idx)
{
	struct printk_ext_log *msg = (struct printk_ext_log *)(log_buf_ext + idx);

	/*
	 * A length == 0 record is the end of buffer marker. Wrap around and
	 * read the message at the start of the buffer as *this* one, and
	 * return the one after that.
	 */
	if (!msg->len) {
		msg = (struct printk_ext_log *)log_buf_ext;
		return msg->len;
	}
	return idx + msg->len;
}

/* get record by index; idx must point to valid msg */
static struct printk_ext_log *msg_by_idx(u32 idx)
{
	struct printk_ext_log *msg = (struct printk_ext_log *)(log_buf_ext + idx);

	/*
	 * A length == 0 record is the end of buffer marker. Wrap around and
	 * read the message at the start of the buffer.
	 */
	if (!msg->len)
		return (struct printk_ext_log *)log_buf_ext;
	return msg;
}

/* compute the message size including the padding bytes */
static u32 msg_used_size(u16 text_len, u32 *pad_len)
{
	u32 size;

	size = sizeof(struct printk_ext_log) + text_len;
	*pad_len = (-size) & (LOG_ALIGN - 1);
	size += *pad_len;

	return size;
}

/*
 * Check whether there is enough free space for the given message.
 *
 * The same values of first_idx and next_idx mean that the buffer
 * is either empty or full.
 *
 * If the buffer is empty, we must respect the position of the indexes.
 * They cannot be reset to the beginning of the buffer.
 */
static int logbuf_has_space(u32 msg_size, bool empty)
{
	u32 free;

	if (log_next_idx > log_first_idx || empty)
		free = get_max(log_buf_len - log_next_idx, log_first_idx);
	else
		free = log_first_idx - log_next_idx;

	/*
	 * We need space also for an empty header that signalizes wrapping
	 * of the buffer.
	 */
	return free >= msg_size + sizeof(struct printk_ext_log);
}

static int log_make_free_space(u32 msg_size)
{
	while (log_first_seq < log_next_seq &&
		!logbuf_has_space(msg_size, false)) {
		/* drop old messages until we have enough contiguous space */
		log_first_idx = msg_next(log_first_idx);
		log_first_seq++;
	}

	/* sequence numbers are equal, so the log buffer is empty */
	if (logbuf_has_space(msg_size, log_first_seq == log_next_seq))
		return 0;

	return -ENOMEM;
}

/*
 * Define how much of the log buffer we could take at maximum. The value
 * must be greater than two. Note that only half of the buffer is available
 * when the index points to the middle.
 */
#define MAX_LOG_TAKE_PART 4
static const char trunc_msg[] = "<truncated>";
static u32 truncate_msg(u16 *text_len, u16 *trunc_msg_len, u32 *pad_len)
{
	/*
	 * The message should not take the whole buffer. Otherwise, it might
	 * get removed too soon.
	 */
	u32 max_text_len = log_buf_len / MAX_LOG_TAKE_PART;

	if (*text_len > max_text_len)
		*text_len = max_text_len;
	/* enable the warning message */
	*trunc_msg_len = strlen(trunc_msg);
	/* compute the size again, count also the warning message */
	return msg_used_size(*text_len + *trunc_msg_len, pad_len);
}

/* insert record into the buffer, discard old ones, update heads */
static int store_to_buf(int level, bool newline,
		const char *text, u16 text_len)
{
	struct printk_ext_log *msg;
	u32 size, pad_len;
	u16 trunc_msg_len = 0;

	/* number of '\0' padding bytes to next message */
	size = msg_used_size(text_len, &pad_len);

	if (log_make_free_space(size)) {
		/* truncate the message if it is too long for empty buffer */
		size = truncate_msg(&text_len, &trunc_msg_len, &pad_len);
		/* survive when the log buffer is too small for trunc_msg */
		if (log_make_free_space(size))
			return 0;
	}

	if (log_next_idx + size + sizeof(struct printk_ext_log) > log_buf_len) {
		/*
		 * This message + an additional empty header does not fit
		 * at the end of the buffer. Add an empty header with len == 0
		 * to signify a wrap around.
		 */
		memset(log_buf_ext + log_next_idx, 0, sizeof(struct printk_ext_log));
		log_next_idx = 0;
	}
    /* fill message */
	msg = (struct printk_ext_log *)(log_buf_ext + log_next_idx);
	memcpy(log_text(msg), text, text_len);
	msg->text_len = text_len;

	if (trunc_msg_len) {
		memcpy(log_text(msg) + text_len, trunc_msg, trunc_msg_len);
		msg->text_len += trunc_msg_len;
	}

	msg->level = level & 7;
	msg->newline = newline;
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	msg->ts = current_kernel_time();
#else
	msg->ts = __current_kernel_time();
#endif
	msg->process_id = smp_processor_id();
	msg->pid = current->pid;
	snprintf(msg->comm, sizeof(msg->comm), "%s", current->comm);
	memset(log_text(msg) + msg->text_len, 0, pad_len);
	msg->len = size;

	/* insert message */
	log_next_idx += msg->len;
	log_next_seq++;

	wake_up_interruptible(&msg_wait);

	return msg->text_len;
}

static inline int print_get_level(const char *buffer)
{
	if (buffer[0] == KERN_SOH_ASCII && buffer[1]) {
		switch (buffer[1]) {
		case '0' ... '7':
		case 'd':   /* the default kernel loglevel */
			return buffer[1];
		}
	}
	return 0;
}

asmlinkage int printk_func(int level, const char *fmt, va_list args)
{
	static char textbuf[LOG_LINE_MAX];
	char *text = textbuf;
	int kern_level;
	int print_len;
	size_t text_len;
	unsigned long flags;
	bool newline = false;

	raw_spin_lock_irqsave(&buf_lock, flags);
	text_len = vscnprintf(text, sizeof(textbuf), fmt, args);

	/* mark and strip a trailing newline */
	if (text_len && text[text_len-1] == '\n') {
		text_len--;
		newline = true;
	}

	/* extract log level */
	while ((kern_level = print_get_level(text)) != 0) {
		switch (kern_level) {
		case '0' ... '7':
			level = kern_level - '0';
			/* fallthrough */
		case 'd':
			break;
		}
		text_len -= 2;
		text += 2;
	}

	if (level == LOGLEVEL_DEFAULT)
		level = CONFIG_MESSAGE_LOGLEVEL_DEFAULT;

	print_len = store_to_buf(level, newline, text, text_len);
	raw_spin_unlock_irqrestore(&buf_lock, flags);

	return print_len;
}

static size_t format_prefix(struct timespec ts, char *buf,
		unsigned int process_id, pid_t pid, const char *comm)
{
	int msg_len;
	struct rtc_time tm;

	ts.tv_sec -= 60*sys_tz.tz_minuteswest;
	rtc_time_to_tm(ts.tv_sec, &tm);
	msg_len = snprintf(buf, buf ? 100 : 0,
			"[%02d-%02d %02d:%02d:%02d.%03d] [%u][%d: %s] ",
			tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
			tm.tm_sec, (int)(ts.tv_nsec / NSEC_PER_MSEC),
			process_id, pid, comm);

	return msg_len;
}

static size_t get_prefix(const struct printk_ext_log *msg, char *buf)
{
	return format_prefix(msg->ts, buf, msg->process_id, msg->pid, msg->comm);
}

static size_t print_text(const struct printk_ext_log *msg,
		char *buf, size_t size)
{
	const char *text = log_text(msg);
	size_t text_size = msg->text_len;
	bool newline = msg->newline;
	size_t len = 0;

	do {
		const char *next = memchr(text, '\n', text_size);
		size_t text_len;

		if (next) {
			text_len = next - text;
			next++;
			text_size -= next - text;
		} else {
			text_len = text_size;
		}
		if (get_prefix(msg, NULL) + text_len + 1 >= size - len)
			break;

		len += get_prefix(msg, buf + len);
		memcpy(buf + len, text, text_len);
		len += text_len;
		if (next || newline)
			buf[len++] = '\n';

		text = next;
	} while (text);

	return len;
}

static int log_print(char __user *buf, int size)
{
	char *text;
	struct printk_ext_log *msg;
	int len = 0;

	text = kmalloc(LOG_LINE_MAX + PREFIX_MAX, GFP_KERNEL);
	if (!text)
		return -ENOMEM;

	while (size > 0) {
		size_t n;
		size_t skip;

		raw_spin_lock_irq(&buf_lock);
		if (log_read_seq < log_first_seq) {
			/* messages are gone, move to first one */
			log_read_seq = log_first_seq;
			log_read_idx = log_first_idx;
			log_partial = 0;
		}
		if (log_read_seq == log_next_seq) {
			raw_spin_unlock_irq(&buf_lock);
			break;
		}

		skip = log_partial;
		msg = msg_by_idx(log_read_idx);
		n = print_text(msg, text, LOG_LINE_MAX + PREFIX_MAX);
		if (n - log_partial <= size) {
			/* message fits into buffer, move forward */
			log_read_idx = msg_next(log_read_idx);
			log_read_seq++;
			n -= log_partial;
			log_partial = 0;
		} else if (!len) {
			/* partial read(), remember position */
			n = size;
			log_partial += n;
		} else
			n = 0;
		raw_spin_unlock_irq(&buf_lock);

		if (!n)
			break;

		if (copy_to_user(buf, text + skip, n)) {
			if (!len)
				len = -EFAULT;
			break;
		}

		len += n;
		size -= n;
		buf += n;
	}

	kfree(text);
	return len;
}

int size_unread(void)
{
	int size;

	raw_spin_lock_irq(&buf_lock);
	if (log_read_seq < log_first_seq) {
		/* messages are gone, move to first one */
		log_read_seq = log_first_seq;
		log_read_idx = log_first_idx;
		log_partial = 0;
	}
	raw_spin_unlock_irq(&buf_lock);
	/* return the count of records, not the length */
	size = log_next_seq - log_read_seq;

	return size;
}

asmlinkage __visible int printk_ext(const char *fmt, ...)
{
	int len;
	va_list args;

	va_start(args, fmt);
	len = printk_func(LOGLEVEL_DEFAULT, fmt, args);
	va_end(args);

	return len;
}
EXPORT_SYMBOL(printk_ext);

static int kmsg_ext_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t kmsg_ext_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	int error;

	if ((file->f_flags & O_NONBLOCK) && !size_unread())
		return -EAGAIN;

	if (!buf || count <= 0)
		return (count == 0) ? 0 : -EINVAL;

	if (!access_ok(buf, count))
		return -EFAULT;

	error = wait_event_interruptible(msg_wait,
			log_read_seq != log_next_seq);

	return error ? error : log_print(buf, count);
}

const struct file_operations proc_kmsg_ext_operations = {
	.open = kmsg_ext_open,
	.read = kmsg_ext_read,
};

/* printk_ext test */
#define TIMER_COUNT 10

struct timer_list timers[TIMER_COUNT];
static bool printk_test_enabled = false;

void timer_func_1(struct timer_list *timer)
{
	unsigned long index = ((unsigned long)timer -
						(unsigned long)&timers[0]) /
								sizeof(timers[0]);

	mod_timer(timer, jiffies + HZ/10);
	printk_ext("printk_ext test, timer id: %u\n", index);
}

void timer_func_2(unsigned long nr)
{
	nr &= 0xf;
	mod_timer(&timers[nr], jiffies + HZ/10);
	printk_ext("printk_ext test, timer id: %u\n", nr);
}

static int set_test_enabled(const char *val, const struct kernel_param *kp)
{
	int i, rv;
	bool raw_val, cur_val;

	raw_val = *(bool *)kp->arg;
	rv = param_set_bool(val, kp);
	if (rv)
		return rv;

	cur_val = *(bool *)kp->arg;
	if (!raw_val && cur_val) {
		/* create timer */
		for (i = 0; i < TIMER_COUNT; i++) {
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
			timer_setup(&timers[i], timer_func_1, 0);
#else
			init_timer(&timers[i]);
			timers[i].data = i;
			timers[i].function = timer_func_2;
#endif
			timers[i].expires = jiffies + HZ/10;
			add_timer(&timers[i]);
			msleep(100);
		}
	} else if (raw_val && !cur_val) {
		for (i = 0; i < TIMER_COUNT; i++) {
			del_timer(&timers[i]);
		}
	}

	return 0;
}

static struct kernel_param_ops enabled_param_ops = {
	.set = set_test_enabled,
	.get = param_get_bool,
};

static int kmsg_ext_init(void)
{
	proc_create("kmsg_ext", S_IRUSR | S_IRGRP, NULL, &proc_kmsg_ext_operations);

	return 0;
}

static void kmsg_ext_exit(void)
{
	int i;

	remove_proc_entry("kmsg_ext", NULL);
	if (printk_test_enabled) {
		for (i = 0; i < TIMER_COUNT; i++) {
			del_timer(&timers[i]);
		}
	}
}

module_param_cb(printk_test_enabled, &enabled_param_ops, &printk_test_enabled, 0644);
MODULE_PARM_DESC(printk_test_enabled, "enable printk test");

module_init(kmsg_ext_init);
module_exit(kmsg_ext_exit);
MODULE_LICENSE("GPL");
