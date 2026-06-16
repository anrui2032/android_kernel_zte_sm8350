/*
 * drivers/misc/logger.c
 *
 * A Logging Subsystem
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * Robert Love <rlove@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "logger: " fmt

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/vmalloc.h>
#include <linux/aio.h>
#include <linux/uio.h> /* additional header file */
#include "logger.h"

#include <asm/ioctls.h>

#include "logger_misc.h"


#include <vendor/soc/qcom/debug_policy.h>



#define GUID_LEN          40

#define GUID_MAIN         "fdb560d1-13b5-4e63-b258-473fc2177cba"
#define GUID_RADIO        "42516612-5091-4464-be5a-8d7e90092178"
#define GUID_SYSTEM       "f996c893-781e-4c7f-90c4-741147974c41"
#define GUID_CRASH        "f34ed909-d169-4099-a305-c4a98fecc02f"
#define GUID_EVENT        "4a80950f-d53f-4c79-af7d-318b3b3a9ce6"

static int logcat_buf_size = 0;


/**
 * struct logger_log - represents a specific log, such as 'main' or 'radio'
 * @buffer:	The actual ring buffer
 * @misc:	The "misc" device representing the log
 * @wq:		The wait queue for @readers
 * @readers:	This log's readers
 * @mutex:	The mutex that protects the @buffer
 * @w_off:	The current write head offset
 * @head:	The head, or location that readers start reading at.
 * @size:	The size of the log
 * @logs:	The list of log channels
 *
 * This structure lives from module insertion until module removal, so it does
 * not need additional reference counting. The structure is protected by the
 * mutex 'mutex'.
 */
struct logger_log {
	unsigned char guid[GUID_LEN];
	phys_addr_t		paddr;
	size_t			w_off;
	size_t			head;
	size_t			size;
	unsigned char		*buffer;
	struct miscdevice	misc;
	wait_queue_head_t	wq;
	struct list_head	readers;
	struct mutex		mutex;
	struct list_head	logs;
};

static LIST_HEAD(log_list);


/**
 * struct logger_reader - a logging device open for reading
 * @log:	The associated log
 * @list:	The associated entry in @logger_log's list
 * @r_off:	The current read head offset.
 * @r_all:	Reader can read all entries
 * @r_ver:	Reader ABI version
 *
 * This object lives from open to release, so we don't need additional
 * reference counting. The structure is protected by log->mutex.
 */
struct logger_reader {
	struct logger_log	*log;
	struct list_head	list;
	size_t			r_off;
	bool			r_all;
	int			r_ver;
};

static inline struct timespec current_kernel_logger_time(void)
{
	struct timespec64 ts64;

	ktime_get_coarse_real_ts64(&ts64);

	return timespec64_to_timespec(ts64);
}

/* logger_offset - returns index 'n' into the log via (optimized) modulus */
static size_t logger_offset(struct logger_log *log, size_t n)
{
	return n & (log->size - 1);
}


/*
 * file_get_log - Given a file structure, return the associated log
 *
 * This isn't aesthetic. We have several goals:
 *
 *	1) Need to quickly obtain the associated log during an I/O operation
 *	2) Readers need to maintain state (logger_reader)
 *	3) Writers need to be very fast (open() should be a near no-op)
 *
 * In the reader case, we can trivially go file->logger_reader->logger_log.
 * For a writer, we don't want to maintain a logger_reader, so we just go
 * file->logger_log. Thus what file->private_data points at depends on whether
 * or not the file was opened for reading. This function hides that dirtiness.
 */
static inline struct logger_log *file_get_log(struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;

		return reader->log;
	}
	return file->private_data;
}

/*
 * get_entry_header - returns a pointer to the logger_entry header within
 * 'log' starting at offset 'off'. A temporary logger_entry 'scratch' must
 * be provided. Typically the return value will be a pointer within
 * 'logger->buf'.  However, a pointer to 'scratch' may be returned if
 * the log entry spans the end and beginning of the circular buffer.
 */
static struct logger_entry *get_entry_header(struct logger_log *log,
		size_t off, struct logger_entry *scratch)
{
	size_t len = min(sizeof(struct logger_entry), log->size - off);

	if (len != sizeof(struct logger_entry)) {
		memcpy(((void *) scratch), log->buffer + off, len);
		memcpy(((void *) scratch) + len, log->buffer,
			sizeof(struct logger_entry) - len);
		return scratch;
	}

	return (struct logger_entry *) (log->buffer + off);
}

/*
 * get_entry_msg_len - Grabs the length of the message of the entry
 * starting from from 'off'.
 *
 * An entry length is 2 bytes (16 bits) in host endian order.
 * In the log, the length does not include the size of the log entry structure.
 * This function returns the size including the log entry structure.
 *
 * Caller needs to hold log->mutex.
 */
static __u32 get_entry_msg_len(struct logger_log *log, size_t off)
{
	struct logger_entry scratch;
	struct logger_entry *entry;

	entry = get_entry_header(log, off, &scratch);
	return entry->len;
}

static size_t get_user_hdr_len(int ver)
{
	if (ver < 2)
		return sizeof(struct user_logger_entry_compat);
	return sizeof(struct logger_entry);
}

static ssize_t copy_header_to_user(int ver, struct logger_entry *entry,
					 char __user *buf)
{
	void *hdr;
	size_t hdr_len;
	struct user_logger_entry_compat v1;

	if (ver < 2) {
		v1.len      = entry->len;
		v1.__pad    = 0;
		v1.pid      = entry->pid;
		v1.tid      = entry->tid;
		v1.sec      = entry->sec;
		v1.nsec     = entry->nsec;
		hdr         = &v1;
		hdr_len     = sizeof(struct user_logger_entry_compat);
	} else {
		hdr         = entry;
		hdr_len     = sizeof(struct logger_entry);
	}

	return copy_to_user(buf, hdr, hdr_len);
}

/*
 * do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * user-space buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->mutex.
 */
static ssize_t do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader,
				   char __user *buf,
				   size_t count)
{
	struct logger_entry scratch;
	struct logger_entry *entry;
	size_t len;
	size_t msg_start;

	/*
	 * First, copy the header to userspace, using the version of
	 * the header requested
	 */
	entry = get_entry_header(log, reader->r_off, &scratch);
	if (copy_header_to_user(reader->r_ver, entry, buf))
		return -EFAULT;

	count -= get_user_hdr_len(reader->r_ver);
	buf += get_user_hdr_len(reader->r_ver);
	msg_start = logger_offset(log,
		reader->r_off + sizeof(struct logger_entry));

	/*
	 * We read from the msg in two disjoint operations. First, we read from
	 * the current msg head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - msg_start);
	if (copy_to_user(buf, log->buffer + msg_start, len))
		return -EFAULT;

	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;

	reader->r_off = logger_offset(log, reader->r_off +
		sizeof(struct logger_entry) + count);

	return count + get_user_hdr_len(reader->r_ver);
}

/*
 * get_next_entry_by_uid - Starting at 'off', returns an offset into
 * 'log->buffer' which contains the first entry readable by 'euid'
 */
static size_t get_next_entry_by_uid(struct logger_log *log,
		size_t off, kuid_t euid)
{
	while (off != log->w_off) {
		struct logger_entry *entry;
		struct logger_entry scratch;
		size_t next_len;

		entry = get_entry_header(log, off, &scratch);

		if (uid_eq(entry->euid, euid))
			return off;

		next_len = sizeof(struct logger_entry) + entry->len;
		off = logger_offset(log, off + next_len);
	}

	return off;
}

/*
 * logger_read - our log's read() method
 *
 * Behavior:
 *
 *	- O_NONBLOCK works
 *	- If there are no log entries to read, blocks until log is written to
 *	- Atomically reads exactly one log entry
 *
 * Will set errno to EINVAL if read
 * buffer is insufficient to hold next entry.
 */
static ssize_t logger_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
	DEFINE_WAIT(wait);

start:
	while (1) {
		mutex_lock(&log->mutex);

		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		ret = (log->w_off == reader->r_off);
		mutex_unlock(&log->mutex);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
		return ret;

	mutex_lock(&log->mutex);

	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
		mutex_unlock(&log->mutex);
		goto start;
	}

	/* get the size of the next entry */
	ret = get_user_hdr_len(reader->r_ver) +
		get_entry_msg_len(log, reader->r_off);
	if (count < ret) {
		ret = -EINVAL;
		goto out;
	}

	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, buf, ret);

out:
	mutex_unlock(&log->mutex);

	return ret;
}

/*
 * get_next_entry - return the offset of the first valid entry at least 'len'
 * bytes after 'off'.
 *
 * Caller must hold log->mutex.
 */
static size_t get_next_entry(struct logger_log *log, size_t off, size_t len)
{
	size_t count = 0;

	do {
		size_t nr = sizeof(struct logger_entry) +
			get_entry_msg_len(log, off);
		off = logger_offset(log, off + nr);
		count += nr;
	} while (count < len);

	return off;
}

/*
 * is_between - is a < c < b, accounting for wrapping of a, b, and c
 *    positions in the buffer
 *
 * That is, if a<b, check for c between a and b
 * and if a>b, check for c outside (not between) a and b
 *
 * |------- a xxxxxxxx b --------|
 *               c^
 *
 * |xxxxx b --------- a xxxxxxxxx|
 *    c^
 *  or                    c^
 */
static inline int is_between(size_t a, size_t b, size_t c)
{
	if (a < b) {
		/* is c between a and b? */
		if (a < c && c <= b)
			return 1;
	} else {
		/* is c outside of b through a? */
		if (c <= b || a < c)
			return 1;
	}

	return 0;
}

/*
 * fix_up_readers - walk the list of all readers and "fix up" any who were
 * lapped by the writer; also do the same for the default "start head".
 * We do this by "pulling forward" the readers and start head to the first
 * entry after the new write head.
 *
 * The caller needs to hold log->mutex.
 */
static void fix_up_readers(struct logger_log *log, size_t len)
{
	size_t old = log->w_off;
	size_t new = logger_offset(log, old + len);
	struct logger_reader *reader;

	if (is_between(old, new, log->head))
		log->head = get_next_entry(log, log->head, len);

	list_for_each_entry(reader, &log->readers, list)
		if (is_between(old, new, reader->r_off))
			reader->r_off = get_next_entry(log, reader->r_off, len);
}

/*
 * logger_write_iter - our write method, implementing support for write(),
 * writev(), and aio_write(). Writes are our fast path, and we try to optimize
 * them above all else.
 */
static ssize_t logger_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct logger_log *log = file_get_log(iocb->ki_filp);
	struct logger_entry header;
	struct timespec now;
	size_t len, count, w_off;

	count = min_t(size_t, from->count, LOGGER_ENTRY_MAX_PAYLOAD);

	now = current_kernel_logger_time();

#ifdef CONFIG_DUAL_SYSTEM_LOGCAT
	header.pid = task_tgid_vnr(current);
	header.tid = task_pid_vnr(current);
#else
	header.pid = current->tgid;
	header.tid = current->pid;
#endif
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.euid = current_euid();
	header.len = count;
	header.hdr_size = sizeof(struct logger_entry);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return 0;

	mutex_lock(&log->mutex);

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	len = min(sizeof(header), log->size - log->w_off);
	memcpy(log->buffer + log->w_off, &header, len);
	memcpy(log->buffer, (char *)&header + len, sizeof(header) - len);

	/* Work with a copy until we are ready to commit the whole entry */
	w_off =  logger_offset(log, log->w_off + sizeof(struct logger_entry));

	len = min(count, log->size - w_off);

	if (copy_from_iter(log->buffer + w_off, len, from) != len) {
		/*
		 * Note that by not updating log->w_off, this abandons the
		 * portion of the new entry that *was* successfully
		 * copied, just above.  This is intentional to avoid
		 * message corruption from missing fragments.
		 */
		mutex_unlock(&log->mutex);
		return -EFAULT;
	}

	if (copy_from_iter(log->buffer, count - len, from) != count - len) {
		mutex_unlock(&log->mutex);
		return -EFAULT;
	}

	log->w_off = logger_offset(log, w_off + count);
	mutex_unlock(&log->mutex);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

	return len;
}

static struct logger_log *get_log_from_minor(int minor)
{
	struct logger_log *log;

	list_for_each_entry(log, &log_list, logs)
		if (log->misc.minor == minor)
			return log;
	return NULL;
}

/*
 * logger_open - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int logger_open(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		reader->r_ver = 1;
		reader->r_all = in_egroup_p(inode->i_gid) ||
			capable(CAP_SYSLOG);

		INIT_LIST_HEAD(&reader->list);

		mutex_lock(&log->mutex);
		reader->r_off = log->head;
		list_add_tail(&reader->list, &log->readers);
		mutex_unlock(&log->mutex);

		file->private_data = reader;
	} else
		file->private_data = log;

	return 0;
}

/*
 * logger_release - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int logger_release(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		struct logger_log *log = reader->log;

		mutex_lock(&log->mutex);
		list_del(&reader->list);
		mutex_unlock(&log->mutex);

		kfree(reader);
	}

	return 0;
}

/*
 * logger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int logger_poll(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	mutex_lock(&log->mutex);
	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	if (log->w_off != reader->r_off)
		ret |= POLLIN | POLLRDNORM;
	mutex_unlock(&log->mutex);

	return ret;
}

static long logger_set_version(struct logger_reader *reader, void __user *arg)
{
	int version;

	if (copy_from_user(&version, arg, sizeof(int)))
		return -EFAULT;

	if ((version < 1) || (version > 2))
		return -EINVAL;

	reader->r_ver = version;
	return 0;
}

static long logger_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -EINVAL;
	void __user *argp = (void __user *) arg;

	mutex_lock(&log->mutex);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off >= reader->r_off)
			ret = log->w_off - reader->r_off;
		else
			ret = (log->size - reader->r_off) + log->w_off;
		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;

		if (!reader->r_all)
			reader->r_off = get_next_entry_by_uid(log,
				reader->r_off, current_euid());

		if (log->w_off != reader->r_off)
			ret = get_user_hdr_len(reader->r_ver) +
				get_entry_msg_len(log, reader->r_off);
		else
			ret = 0;
		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
		if (!(in_egroup_p(file_inode(file)->i_gid) ||
				capable(CAP_SYSLOG))) {
			ret = -EPERM;
			break;
		}
		list_for_each_entry(reader, &log->readers, list)
			reader->r_off = log->w_off;
		log->head = log->w_off;
		ret = 0;
		break;
	case LOGGER_GET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = reader->r_ver;
		break;
	case LOGGER_SET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = logger_set_version(reader, argp);
		break;
	}

	mutex_unlock(&log->mutex);

	return ret;
}

static const struct file_operations logger_fops = {
	.owner = THIS_MODULE,
	.read = logger_read,
	.write_iter = logger_write_iter,
	.poll = logger_poll,
	.unlocked_ioctl = logger_ioctl,
	.compat_ioctl = logger_ioctl,
	.open = logger_open,
	.release = logger_release,
};

/*
 * Log size must must be a power of two, and greater than
 * (LOGGER_ENTRY_MAX_PAYLOAD + sizeof(struct logger_entry)).
 */
static int __init create_log(char *log_name, int size, char* guid)
{
	int ret = 0;
	struct logger_log *log;
	unsigned char *buffer;

	buffer = kmalloc(size, GFP_KERNEL);
	if (buffer == NULL)
		return -ENOMEM;

	log = kzalloc(sizeof(struct logger_log), GFP_KERNEL);
	if (log == NULL) {
		ret = -ENOMEM;
		goto out_free_buffer;
	}
	log->buffer = buffer;

	if (guid) {
		strncpy(log->guid, guid, GUID_LEN);
		log->paddr = virt_to_phys(buffer);
	}

	log->misc.minor = MISC_DYNAMIC_MINOR;
	log->misc.name = kstrdup(log_name, GFP_KERNEL);
	if (log->misc.name == NULL) {
		ret = -ENOMEM;
		goto out_free_log;
	}

	log->misc.fops = &logger_fops;
	log->misc.parent = NULL;

	init_waitqueue_head(&log->wq);
	INIT_LIST_HEAD(&log->readers);
	mutex_init(&log->mutex);
	log->w_off = 0;
	log->head = 0;
	log->size = size;

	INIT_LIST_HEAD(&log->logs);
	list_add_tail(&log->logs, &log_list);

	/* finally, initialize the misc device for this log */
	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		pr_err("failed to register misc device for log '%s'!\n",
				log->misc.name);
		goto out_free_misc_name;
	}

	pr_info("created %luK log '%s'\n",
		(unsigned long) log->size >> 10, log->misc.name);

	return 0;

out_free_misc_name:
	kfree(log->misc.name);

out_free_log:
	kfree(log);

out_free_buffer:
	vfree(buffer);
	return ret;
}
#ifdef CONFIG_DUAL_SYSTEM_LOGCAT

static int work_logger_init(void)
{
	int ret;

	if (logcat_buf_size > 0)
	{
		ret = create_log(WORK_LOGGER_LOG_MAIN, logcat_buf_size * 1024, NULL);
		if (unlikely(ret))
			goto out;

		ret = create_log(WORK_LOGGER_LOG_EVENTS, logcat_buf_size * 1024, NULL);
		if (unlikely(ret))
			goto out;

		ret = create_log(WORK_LOGGER_LOG_RADIO, logcat_buf_size * 1024, NULL);
		if (unlikely(ret))
			goto out;

		ret = create_log(WORK_LOGGER_LOG_SYSTEM, logcat_buf_size * 1024, NULL);
		if (unlikely(ret))
			goto out;
	}
	else
	{
		ret = create_log(WORK_LOGGER_LOG_MAIN, 4096 * 1024, NULL);
		if (unlikely(ret))
			goto out;

		ret = create_log(WORK_LOGGER_LOG_EVENTS, 256 * 1024, NULL);
		if (unlikely(ret))
			  goto out;

		ret = create_log(WORK_LOGGER_LOG_RADIO, 256 * 1024, NULL);
		if (unlikely(ret))
			  goto out;

		ret = create_log(WORK_LOGGER_LOG_SYSTEM, 256 * 1024, NULL);
		if (unlikely(ret))
			  goto out;
	}


	ret = create_log(WORK_LOGGER_LOG_CRASH, 64 * 1024, NULL);
	if (unlikely(ret))
		goto out;

	ret = create_log(WORK_LOGGER_LOG_STATS, 64 * 1024, NULL);
	if (unlikely(ret))
		goto out;

	ret = create_log(WORK_LOGGER_LOG_SECURITY, 64 * 1024, NULL);
	if (unlikely(ret))
		goto out;


out:
	return ret;

}

static int __init home_logger_init(void)
{
	int ret;

	if (logcat_buf_size > 0)
	{
		ret = create_log(HOME_LOGGER_LOG_MAIN, logcat_buf_size * 1024, NULL);
		if (unlikely(ret))
			goto out;

		ret = create_log(HOME_LOGGER_LOG_EVENTS, logcat_buf_size * 1024, NULL);
		if (unlikely(ret))
			goto out;

		ret = create_log(HOME_LOGGER_LOG_RADIO, logcat_buf_size * 1024, NULL);
		if (unlikely(ret))
			goto out;

		ret = create_log(HOME_LOGGER_LOG_SYSTEM, logcat_buf_size * 1024, NULL);
		if (unlikely(ret))
			goto out;
	}
	else
	{
		ret = create_log(HOME_LOGGER_LOG_MAIN, 256 * 1024, NULL);
		if (unlikely(ret))
			goto out;

		ret = create_log(HOME_LOGGER_LOG_EVENTS, 256 * 1024, NULL);
		if (unlikely(ret))
			goto out;

		ret = create_log(HOME_LOGGER_LOG_RADIO, 256 * 1024, NULL);
		if (unlikely(ret))
			goto out;

		ret = create_log(HOME_LOGGER_LOG_SYSTEM, 256 * 1024, NULL);
		if (unlikely(ret))
			goto out;
	}



	ret = create_log(HOME_LOGGER_LOG_CRASH, 64 * 1024, NULL);
	if (unlikely(ret))
		goto out;

	ret = create_log(HOME_LOGGER_LOG_STATS, 64 * 1024, NULL);
	if (unlikely(ret))
		goto out;

	ret = create_log(HOME_LOGGER_LOG_SECURITY, 64 * 1024, NULL);
	if (unlikely(ret))
		goto out;

out:
	return ret;
}
#endif


#ifdef CONFIG_ANDROID_ZLOG_LOW_MEM
/* fixed 512M */
#define ZLOG_LOW_MEM_LIMIT ((512 * 1024 * 1024) / PAGE_SIZE)
/* represent 1/2 of the total buffer size is saved */
#define ZLOG_LOW_MEM_FACTOR 2
extern unsigned long _totalram_pages;
static inline bool is_zlog_low_mem_config(void)
{
	pr_info("check zlog low mem %lu:%lu:\n",
		_totalram_pages,
		ZLOG_LOW_MEM_LIMIT);

	return (_totalram_pages <= ZLOG_LOW_MEM_LIMIT);
}

#endif

/* ratio factor 1 means no decreasing, the size is as specified */
#define ZLOG_SIZE_AS_SPECIFIED 1

static int host_logger_init(void)
{
	int ret;
	/* how many proportion buffer size is used: 1/factor */
	int factor = ZLOG_SIZE_AS_SPECIFIED;


#ifdef CONFIG_ANDROID_ZLOG_LOW_MEM
		if (is_zlog_low_mem_config())
			factor = ZLOG_LOW_MEM_FACTOR;
#endif
	pr_notice("logcat_buf_size is %d\n", logcat_buf_size);
	if (logcat_buf_size > 0)
	{
		ret = create_log(LOGGER_LOG_MAIN, logcat_buf_size * 1024 / factor, GUID_MAIN);
		if (unlikely(ret))
			goto out;

		ret = create_log(LOGGER_LOG_EVENTS, logcat_buf_size * 1024 / factor, GUID_EVENT);
		if (unlikely(ret))
			goto out;

		ret = create_log(LOGGER_LOG_RADIO, logcat_buf_size * 1024 / factor, GUID_RADIO);
		if (unlikely(ret))
			goto out;

		ret = create_log(LOGGER_LOG_SYSTEM, logcat_buf_size * 1024 / factor, GUID_SYSTEM);
		if (unlikely(ret))
			goto out;
	}
	else
	{
		ret = create_log(LOGGER_LOG_MAIN, 256 * 1024 / factor, GUID_MAIN);
		if (unlikely(ret))
			goto out;

		ret = create_log(LOGGER_LOG_EVENTS, 256 * 1024 / factor, GUID_EVENT);
		if (unlikely(ret))
			goto out;

		ret = create_log(LOGGER_LOG_RADIO, 256 * 1024 / factor, GUID_RADIO);
		if (unlikely(ret))
			goto out;

		ret = create_log(LOGGER_LOG_SYSTEM, 256 * 1024 / factor, GUID_SYSTEM);
		if (unlikely(ret))
			goto out;
	}

	ret = create_log(LOGGER_LOG_CRASH, 64 * 1024 / factor, GUID_CRASH);
	if (unlikely(ret))
		goto out;

	ret = create_log(LOGGER_LOG_STATS, 64 * 1024 / factor, NULL);
	if (unlikely(ret))
		goto out;

	ret = create_log(LOGGER_LOG_SECURITY, 64 * 1024 / factor, NULL);
	if (unlikely(ret))
		goto out;

out:
	return ret;
}

static int __init logger_init(void)
{
	int ret;

#ifdef CONFIG_ANDROID_ZLOG_BUFFER
  if (!is_kernel_log_driver_enabled()) {
		return 0;
	}
#endif

	ret = host_logger_init();
	if (unlikely(ret))
		goto out;

#ifdef CONFIG_DUAL_SYSTEM_LOGCAT
	ret = home_logger_init();
	if (unlikely(ret))
		goto out;

	ret = work_logger_init();
	if (unlikely(ret))
		goto out;
#endif

out:
	return ret;

}

static void __exit logger_exit(void)
{
	struct logger_log *current_log, *next_log;

	list_for_each_entry_safe(current_log, next_log, &log_list, logs) {
		/* we have to delete all the entry inside log_list */
		misc_deregister(&current_log->misc);
		vfree(current_log->buffer);
		kfree(current_log->misc.name);
		list_del(&current_log->logs);
		kfree(current_log);
	}
}

static int __init logcat_buf_size_setup(char *str)
{
	unsigned long size = 0;

	if (!str)
		return -EINVAL;

	if (kstrtoul(str,10, &size)) {
		pr_notice("%s can not convert %s in boot_command_line\n", __func__, str);
		return -EINVAL;
	}
	pr_notice("logcat buffer size is %ld\n", size);
	logcat_buf_size = size;
	return 0;
}


early_param("logcat_buf", logcat_buf_size_setup);


device_initcall(logger_init);
module_exit(logger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert Love, <rlove@google.com>");
MODULE_DESCRIPTION("Android Logger");
