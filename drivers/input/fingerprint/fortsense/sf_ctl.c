/* SPDX-License-Identifier: GPL-2.0 */

/**
 * The device control driver for Fortsense's fingerprint sensor.
 *
 * Copyright (C) 2018 Fortsense Corporation. <http://www.fortsense.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
**/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>
#include <drm/drm_panel.h>

#include "sf_ctl.h"

#if SF_BEANPOD_COMPATIBLE_V1
#include "nt_smc_call.h"
#endif

#if SF_INT_TRIG_HIGH
#include <linux/irq.h>
#endif

#ifdef CONFIG_RSEE
#include <linux/tee_drv.h>
#endif

#define SF_DRV_VERSION "v2.4.1-2020-01-08"

#define MODULE_NAME "fortsense-sf_ctl"
#define xprintk(level, fmt, args...) pr_notice(level MODULE_NAME": (%d) "fmt, __LINE__, ##args)

#if SF_TRUSTKERNEL_COMPAT_SPI_MT65XX
#define SPI_MODULE_CLOCK	  (120 * 1000 * 1000)
#elif defined(CONFIG_MTK_SPI)
#define SPI_MODULE_CLOCK	  (100 * 1000 * 1000)
#endif

#define SF_DEFAULT_SPI_SPEED  (1 * 1000 * 1000)

static long sf_ctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int sf_ctl_init_irq(void);
static int sf_ctl_init_input(void);
#ifdef CONFIG_COMPAT
static long sf_ctl_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#endif
static int sf_open(struct inode *inode, struct file *filp);
static int sf_release(struct inode *inode, struct file *filp);
#if SF_REE_PLATFORM
static ssize_t sf_ctl_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t sf_ctl_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
#endif
#if (SF_PROBE_ID_EN && !SF_RONGCARD_COMPATIBLE)
static int sf_read_sensor_id(void);
#endif

extern int sf_platform_init(struct sf_ctl_device *ctl_dev);
extern void sf_platform_exit(struct sf_ctl_device *ctl_dev);

/*extern int fb_blank(struct fb_info *info, int blank);*/

#if QUALCOMM_REE_DEASSERT
static int qualcomm_deassert = 0;
#endif

#ifdef CONFIG_RSEE
int rsee_client_get_fpid(int *vendor_id);
#endif
static struct file_operations sf_ctl_fops = {
	.owner		  = THIS_MODULE,
	.unlocked_ioctl = sf_ctl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = sf_ctl_compat_ioctl,
#endif
	.open = sf_open,
	.release = sf_release,
#if SF_REE_PLATFORM
	.read		   = sf_ctl_read,
	.write		  = sf_ctl_write,
#endif
};

static struct sf_ctl_device sf_ctl_dev = {
	.miscdev = {
		.minor  = MISC_DYNAMIC_MINOR,
		.name   = "fortsense_fp",
		.fops   = &sf_ctl_fops,
	},
	.rst_num = 0,
	.irq_pin = 0,
	.irq_num = 0,
	.spi_buf_size = 25 * 1024,
};

#if SF_REG_DEVICE_BY_DRIVER
static struct platform_device *sf_device = NULL;
#endif

#if (defined(CONFIG_MTK_SPI) || SF_TRUSTKERNEL_COMPAT_SPI_MT65XX)
#define SF_DEFAULT_SPI_HALF_CYCLE_TIME  ((SPI_MODULE_CLOCK / SF_DEFAULT_SPI_SPEED) / 2)
#define SF_DEFAULT_SPI_HIGH_TIME   (((SPI_MODULE_CLOCK / SF_DEFAULT_SPI_SPEED) % 2 == 0) ? \
								   SF_DEFAULT_SPI_HALF_CYCLE_TIME : \
								   (SF_DEFAULT_SPI_HALF_CYCLE_TIME + 1))
#define SF_DEFAULT_SPI_LOW_TIME	SF_DEFAULT_SPI_HALF_CYCLE_TIME

static struct mt_chip_conf smt_conf = {
	.setuptime = 15,
	.holdtime = 15,
	.high_time = SF_DEFAULT_SPI_HIGH_TIME, /* 10--6m 15--4m 20--3m 30--2m [ 60--1m 120--0.5m  300--0.2m] */
	.low_time  = SF_DEFAULT_SPI_LOW_TIME,
	.cs_idletime = 20,
	.ulthgh_thrsh = 0,
	.cpol = SPI_CPOL_0,
	.cpha = SPI_CPHA_0,
	.rx_mlsb = SPI_MSB,
	.tx_mlsb = SPI_MSB,
	.tx_endian = 0,
	.rx_endian = 0,
	.com_mod = FIFO_TRANSFER,
	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif

static int sf_remove(sf_device_t *pdev);
static int sf_probe(sf_device_t *pdev);

static struct of_device_id  sf_of_match[] = {
	{ .compatible = COMPATIBLE_SW_FP, },
	{},
};

#if SF_SPI_RW_EN
static struct spi_board_info spi_board_devs[] __initdata = {
	[0] = {
		.modalias = "fortsense-fp",
		.bus_num = 0,
		.chip_select = 0,
		.mode = SPI_MODE_0,
	},
};

static int sf_ctl_spi_speed(unsigned int speed)
{
#ifdef CONFIG_MTK_SPI
	unsigned int time;

	time = SPI_MODULE_CLOCK / speed;
	sf_ctl_dev.mt_conf.high_time = time / 2;
	sf_ctl_dev.mt_conf.low_time  = time / 2;

	if ((time % 2) != 0) {
		sf_ctl_dev.mt_conf.high_time += 1;
	}

#elif (SF_REE_PLATFORM && QUALCOMM_REE_DEASSERT)
	double delay_ns = 0;

	if (speed <= 1000 * 1000) {
		speed = 0.96 * 1000 * 1000; /*0.96M*/
		qualcomm_deassert = 0;
	} else if (speed <= 4800 * 1000) {
		delay_ns = (1.0 / (((double)speed) / (1.0 * 1000 * 1000)) - 1.0 / 4.8) * 8 * 1000;
		speed = 4.8 * 1000 * 1000; /*4.8M*/
		qualcomm_deassert = 3;
	} else {
		delay_ns = (1.0 / (((double)speed) / (1.0 * 1000 * 1000)) - 1.0 / 9.6) * 8 * 1000;
		speed = 9.6 * 1000 * 1000; /*9.6M*/
		qualcomm_deassert = 10;
	}

	xprintk(KERN_INFO, "need delay_ns = xxx, qualcomm_deassert = %d(maybe custom).\n", qualcomm_deassert);
#endif
	sf_ctl_dev.pdev->max_speed_hz = speed;
	spi_setup(sf_ctl_dev.pdev);
	return 0;
}
#endif

static sf_driver_t sf_driver = {
	.driver = {
		.name = "fortsense-fp",
#if SF_SPI_RW_EN
		.bus = &spi_bus_type,
#endif
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sf_of_match,
#endif
	},
	.probe  = sf_probe,
	.remove = sf_remove,
};

static sf_version_info_t sf_hw_ver;

#if SF_INT_TRIG_HIGH
static int sf_ctl_set_irq_type(unsigned long type)
{
	int err = 0;

	if (sf_ctl_dev.irq_num > 0) {
		err = irq_set_irq_type(sf_ctl_dev.irq_num, type | IRQF_NO_SUSPEND | IRQF_ONESHOT);
	}

	return err;
}
#endif

static void sf_ctl_device_event(struct work_struct *ws)
{
	char *uevent_env[2] = { SF_INT_EVENT_NAME, NULL };

	xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __func__);
	kobject_uevent_env(&sf_ctl_dev.miscdev.this_device->kobj,
					   KOBJ_CHANGE, uevent_env);
}

static irqreturn_t sf_ctl_device_irq(int irq, void *dev_id)
{
	disable_irq_nosync(irq);
	xprintk(SF_LOG_LEVEL, "%s(irq = %d, ..) toggled.\n", __func__, irq);
	schedule_work(&sf_ctl_dev.work_queue);
	__pm_wakeup_event(&sf_ctl_dev.wakelock, msecs_to_jiffies(5000));
#if SF_INT_TRIG_HIGH
	sf_ctl_set_irq_type(IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND | IRQF_ONESHOT);
#endif
	enable_irq(irq);
	return IRQ_HANDLED;
}

static int sf_ctl_report_key_event(struct input_dev *input, sf_key_event_t *kevent)
{
	int err = 0;
	unsigned int key_code = KEY_UNKNOWN;

	xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __func__);

	switch (kevent->key) {
	case SF_KEY_HOME:
		key_code = KEY_HOME;
		break;

	case SF_KEY_MENU:
		key_code = KEY_MENU;
		break;

	case SF_KEY_BACK:
		key_code = KEY_BACK;
		break;

	case SF_KEY_F11:
		key_code = KEY_F11;
		break;

	case SF_KEY_ENTER:
		key_code = KEY_ENTER;
		break;

	case SF_KEY_UP:
		key_code = KEY_UP;
		break;

	case SF_KEY_LEFT:
		key_code = KEY_LEFT;
		break;

	case SF_KEY_RIGHT:
		key_code = KEY_RIGHT;
		break;

	case SF_KEY_DOWN:
		key_code = KEY_DOWN;
		break;

	case SF_KEY_WAKEUP:
		key_code = KEY_WAKEUP;
		break;

	default:
		break;
	}

	input_report_key(input, key_code, kevent->value);
	input_sync(input);
	xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __func__);
	return err;
}

static const char *sf_ctl_get_version(void)
{
	static char version[SF_DRV_VERSION_LEN] = {'\0', };

	strlcpy(version, SF_DRV_VERSION, SF_DRV_VERSION_LEN);
	version[SF_DRV_VERSION_LEN - 1] = '\0';
	return (const char *)version;
}

static long sf_ctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	sf_key_event_t kevent;

	xprintk(SF_LOG_LEVEL, "%s(_IO(type,nr) nr= 0x%08x, ..)\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case SF_IOC_INIT_DRIVER: {
#if MULTI_HAL_COMPATIBLE
		sf_ctl_dev.gpio_init(&sf_ctl_dev);
#endif
		break;
	}

	case SF_IOC_DEINIT_DRIVER: {
#if MULTI_HAL_COMPATIBLE
		sf_ctl_dev.free_gpio(&sf_ctl_dev);
#endif
		break;
	}

	case SPI_IOC_RST:
	case SF_IOC_RESET_DEVICE: {
		sf_ctl_dev.reset(false);
		usleep_range(1000, 1001);
		sf_ctl_dev.reset(true);
		usleep_range(10000, 11000);
		break;
	}

	case SF_IOC_ENABLE_IRQ: {
		/* TODO: */
		break;
	}

	case SF_IOC_DISABLE_IRQ: {
		/* TODO: */
		break;
	}

	case SF_IOC_REQUEST_IRQ: {
#if MULTI_HAL_COMPATIBLE
		sf_ctl_init_irq();
#endif
		break;
	}

	case SF_IOC_ENABLE_SPI_CLK: {
		sf_ctl_dev.spi_clk_on(true);
		break;
	}

	case SF_IOC_DISABLE_SPI_CLK: {
		sf_ctl_dev.spi_clk_on(false);
		break;
	}

	case SF_IOC_ENABLE_POWER: {
		sf_ctl_dev.power_on(true);
		break;
	}

	case SF_IOC_DISABLE_POWER: {
		sf_ctl_dev.power_on(false);
		break;
	}

	case SF_IOC_REPORT_KEY_EVENT: {
		if (copy_from_user(&kevent, (sf_key_event_t *)arg, sizeof(sf_key_event_t))) {
			xprintk(KERN_ERR, "copy_from_user(..) failed.\n");
			err = (-EFAULT);
			break;
		}

		err = sf_ctl_report_key_event(sf_ctl_dev.input, &kevent);
		break;
	}

	case SF_IOC_SYNC_CONFIG: {
		/* TODO: */
		break;
	}

	case SPI_IOC_WR_MAX_SPEED_HZ:
	case SF_IOC_SPI_SPEED: {
#if SF_SPI_RW_EN
		sf_ctl_spi_speed(arg);
#endif
		break;
	}

	case SPI_IOC_RD_MAX_SPEED_HZ: {
		/* TODO: */
		break;
	}

	case FORTSENSE_IOC_ATTRIBUTE:
	case SF_IOC_ATTRIBUTE: {
		err = __put_user(sf_ctl_dev.attribute, (__u32 __user *)arg);
		break;
	}

	case SF_IOC_GET_VERSION: {
		if (copy_to_user((void *)arg, sf_ctl_get_version(), SF_DRV_VERSION_LEN)) {
			xprintk(KERN_ERR, "copy_to_user(..) failed.\n");
			err = (-EFAULT);
			break;
		}

		break;
	}

	case SF_IOC_SET_LIB_VERSION: {
		if (copy_from_user((void *)&sf_hw_ver, (void *)arg, sizeof(sf_version_info_t))) {
			xprintk(KERN_ERR, "sf_hw_info_t copy_from_user(..) failed.\n");
			err = (-EFAULT);
			break;
		}

		break;
	}

	case SF_IOC_GET_LIB_VERSION: {
		if (copy_to_user((void *)arg, (void *)&sf_hw_ver, sizeof(sf_version_info_t))) {
			xprintk(KERN_ERR, "sf_hw_info_t copy_to_user(..) failed.\n");
			err = (-EFAULT);
			break;
		}

		break;
	}

	case SF_IOC_SET_SPI_BUF_SIZE: {
		sf_ctl_dev.spi_buf_size = arg;
		break;
	}

	case SF_IOC_SET_RESET_OUTPUT: {
		if (arg) {
			sf_ctl_dev.reset(true);
		} else {
			sf_ctl_dev.reset(false);
		}

		break;
	}

	default:
		err = (-EINVAL);
		break;
	}

	return err;
}

static ssize_t sf_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf, PAGE_SIZE, "%s\n", sf_hw_ver.driver);
	return len;
}
static DEVICE_ATTR(version, 0644, sf_version_show, NULL);

static ssize_t sf_chip_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf((char *)buf, PAGE_SIZE,
			"chip   : %s %s\nid	 : 0x0 lib:%s\nvendor : fw:%s\nmore   : fingerprint\n",
			sf_hw_ver.fortsense_id, sf_hw_ver.ca_version,
			sf_hw_ver.algorithm,
			sf_hw_ver.firmware);
	return len;
}
static DEVICE_ATTR(chip_info, 0644, sf_chip_info_show, NULL);

static ssize_t sf_tee_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "solution:%s\n", sf_hw_ver.tee_solution);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "ca	  :%s\n", sf_hw_ver.ca_version);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "ta	  :%s\n", sf_hw_ver.ta_version);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "alg	 :%s\n", sf_hw_ver.algorithm);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "nav	 :%s\n", sf_hw_ver.algo_nav);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "driver  :%s\n", sf_hw_ver.driver);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "firmware:%s\n", sf_hw_ver.firmware);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "sensor  :%s\n", sf_hw_ver.fortsense_id);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "vendor  :%s\n", sf_hw_ver.vendor_id);
	return ret;
}

static DEVICE_ATTR(tee_version, 0644, sf_tee_version_show, NULL);

static ssize_t
sf_set_fun_store(struct device *d, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int blank;
	int ret;

	ret = sscanf(buf, "%d", &blank);
	pr_notice("%s: start blank=%d\n", __func__, blank);
	/*fb_blank(NULL, blank);*/
	pr_notice("%s: end\n", __func__);
	return count;
}
static DEVICE_ATTR(set_fun, 0644, NULL, sf_set_fun_store);

static struct attribute *sf_sysfs_entries[] = {
	&dev_attr_set_fun.attr,
	&dev_attr_tee_version.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_version.attr,
	NULL
};

static struct attribute_group sf_attribute_group = {
	.attrs = sf_sysfs_entries,
};

#ifdef CONFIG_COMPAT
static long sf_ctl_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return sf_ctl_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int sf_suspend(void)
{
	char *screen[2] = { "SCREEN_STATUS=OFF", NULL };

	kobject_uevent_env(&sf_ctl_dev.miscdev.this_device->kobj, KOBJ_CHANGE, screen);
#if SF_INT_TRIG_HIGH
	sf_ctl_set_irq_type(IRQF_TRIGGER_HIGH);
#endif
	return 0;
}

static int sf_resume(void)
{
	char *screen[2] = { "SCREEN_STATUS=ON", NULL };

	kobject_uevent_env(&sf_ctl_dev.miscdev.this_device->kobj, KOBJ_CHANGE, screen);
#if SF_INT_TRIG_HIGH
	sf_ctl_set_irq_type(IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND | IRQF_ONESHOT);
#endif
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sf_early_suspend(struct early_suspend *handler)
{
	sf_suspend();
}

static void sf_late_resume(struct early_suspend *handler)
{
	sf_resume();
}

#elif defined(CONFIG_ADF_SPRD)
/*
 * touchscreen's suspend and resume state should rely on screen state,
 * as fb_notifier and early_suspend are all disabled on our platform,
 * we can only use adf_event now
 */
static int sf_adf_event_handler(struct notifier_block *nb, unsigned long action, void *data)
{
	struct adf_notifier_event *event = data;
	int adf_event_data;

	if (action != ADF_EVENT_BLANK) {
		return NOTIFY_DONE;
	}

	adf_event_data = *(int *)event->data;
	xprintk(KERN_INFO, "receive adf event with adf_event_data=%d\n", adf_event_data);

	switch (adf_event_data) {
	case DRM_MODE_DPMS_ON:
		sf_resume();
		break;

	case DRM_MODE_DPMS_OFF:
		sf_suspend();
		break;

	default:
		xprintk(KERN_ERR, "receive adf event with error data, adf_event_data=%d\n",
				adf_event_data);
		break;
	}

	return NOTIFY_OK;
}

#else

static int sf_fb_notifier_callback(struct notifier_block *self,
								   unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;
	int retval = 0;

	if (event != DRM_PANEL_EARLY_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
		return 0;
	}

	blank = *(int *)evdata->data;

	switch (blank) {
	case DRM_PANEL_BLANK_UNBLANK:
		sf_resume();
		break;

	case DRM_PANEL_BLANK_POWERDOWN:
		sf_suspend();
		break;

	default:
		break;
	}

	return retval;
}
#endif /*SF_CFG_HAS_EARLYSUSPEND*/

static int sf_remove(sf_device_t *spi)
{
	int err = 0;

	if (sf_ctl_dev.pdev != NULL) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&sf_ctl_dev.early_suspend);
#elif defined(CONFIG_ADF_SPRD)
		/* TODO: is it name adf_unregister_client? Unverified it. */
		adf_unregister_client(&sf_ctl_dev.adf_event_block);
#else
		fb_unregister_client(&sf_ctl_dev.notifier);
#endif

		if (sf_ctl_dev.input) {
			input_unregister_device(sf_ctl_dev.input);
		}

		/*if (sf_ctl_dev.irq_num >= 0) {
			free_irq(sf_ctl_dev.irq_num, (void *)&sf_ctl_dev);
			sf_ctl_dev.irq_num = 0;
		}*/

		misc_deregister(&sf_ctl_dev.miscdev);
		wakeup_source_remove(&sf_ctl_dev.wakelock);
		/*sf_ctl_dev.free_gpio(&sf_ctl_dev);*/
		sf_platform_exit(&sf_ctl_dev);
		sf_ctl_dev.pdev = NULL;
	}

	return err;
}

static int sf_probe(sf_device_t *dev)
{
	int err = 0;

	xprintk(KERN_INFO, "fortsense %s enter\n", __func__);
	sf_ctl_dev.pdev = dev;
	/* setup spi config */
#ifdef CONFIG_MTK_SPI
	memcpy(&sf_ctl_dev.mt_conf, &smt_conf, sizeof(struct mt_chip_conf));
	sf_ctl_dev.pdev->controller_data = (void *)&sf_ctl_dev.mt_conf;
	xprintk(KERN_INFO, "fortsense %s old MTK SPI.\n", __func__);
#endif
#if SF_SPI_RW_EN
	sf_ctl_dev.pdev->mode			= SPI_MODE_0;
	sf_ctl_dev.pdev->bits_per_word   = 8;
	sf_ctl_dev.pdev->max_speed_hz	= SF_DEFAULT_SPI_SPEED;
	spi_setup(sf_ctl_dev.pdev);
#endif
	/* Initialize the platform config. */
	err = sf_platform_init(&sf_ctl_dev);

	if (err) {
		sf_ctl_dev.pdev = NULL;
		xprintk(KERN_ERR, "sf_platform_init failed with %d.\n", err);
		return err;
	}

	wakeup_source_add(&sf_ctl_dev.wakelock);

	/* Initialize the GPIO pins. */
#if MULTI_HAL_COMPATIBLE
	xprintk(KERN_INFO, " do not initialize the GPIO pins.\n");
#else
	/*err = sf_ctl_dev.gpio_init(&sf_ctl_dev);

	if (err) {
		xprintk(KERN_ERR, "gpio_init failed with %d.\n", err);
		sf_platform_exit(&sf_ctl_dev);
		sf_ctl_dev.pdev = NULL;
		return err;
	}

	sf_ctl_dev.reset(false);
	usleep_range(1000, 1001);
	sf_ctl_dev.reset(true);
	usleep_range(10000, 11000);*/
#endif
#if SF_PROBE_ID_EN
#if SF_BEANPOD_COMPATIBLE_V2
	err = get_fp_spi_enable();

	if (err != 1) {
		xprintk(KERN_ERR, "get_fp_spi_enable ret=%d\n", err);
		/*sf_ctl_dev.free_gpio(&sf_ctl_dev);*/
		sf_platform_exit(&sf_ctl_dev);
		sf_ctl_dev.pdev = NULL;
		err = -1;
		return err;
	}

#endif
#if SF_RONGCARD_COMPATIBLE
#ifdef CONFIG_RSEE
	uint64_t vendor_id = 0x00;

	sf_ctl_dev.spi_clk_on(true);
	err = rsee_client_get_fpid(&vendor_id);
	sf_ctl_dev.spi_clk_on(false);
	xprintk(KERN_INFO, "rsee_client_get_fpid vendor id is 0x%x\n", vendor_id);

	if (err || !((vendor_id >> 8) == 0x82)) {
		xprintk(KERN_ERR, "rsee_client_get_fpid failed !\n");
		err = -1;
	}

#else
	err = -1;
	xprintk(KERN_INFO, "CONFIG_RSEE not define, skip rsee_client_get_fpid!\n");
#endif
#else
	sf_ctl_dev.spi_clk_on(true);
	err = sf_read_sensor_id();
	sf_ctl_dev.spi_clk_on(false);
#endif

	if (err < 0) {
		xprintk(KERN_ERR, "fortsense probe read chip id is failed\n");
		/*sf_ctl_dev.free_gpio(&sf_ctl_dev);*/
		sf_platform_exit(&sf_ctl_dev);
		sf_ctl_dev.pdev = NULL;
		err = -1;
		return err;
	}

#if SF_BEANPOD_COMPATIBLE_V2
#ifndef MAX_TA_NAME_LEN
	set_fp_vendor(FP_VENDOR_FORTSENSE);
#else
	set_fp_ta_name("fp_server_fortsense", MAX_TA_NAME_LEN);
#endif /*MAX_TA_NAME_LEN*/
#endif /*SF_BEANPOD_COMPATIBLE_V2*/
#endif /*SF_PROBE_ID_EN*/
	/* reset spi dma mode in old MTK. */
#if (SF_REE_PLATFORM && defined(CONFIG_MTK_SPI))
	{
		sf_ctl_dev.mt_conf.com_mod = DMA_TRANSFER;
		spi_setup(sf_ctl_dev.pdev);
	}
#endif
	/* Initialize the input subsystem. */
	err = sf_ctl_init_input();

	if (err) {
		/*sf_ctl_dev.free_gpio(&sf_ctl_dev);*/
		sf_platform_exit(&sf_ctl_dev);
		sf_ctl_dev.pdev = NULL;
		xprintk(KERN_ERR, "sf_ctl_init_input failed with %d.\n", err);
		return err;
	}

	/* Register as a miscellaneous device. */
	err = misc_register(&sf_ctl_dev.miscdev);

	if (err) {
		/*sf_ctl_dev.free_gpio(&sf_ctl_dev);*/
		sf_platform_exit(&sf_ctl_dev);
		xprintk(KERN_ERR, "misc_register(..) = %d.\n", err);
		input_unregister_device(sf_ctl_dev.input);
		sf_ctl_dev.pdev = NULL;
		return err;
	}

	err = sysfs_create_group(&sf_ctl_dev.miscdev.this_device->kobj, &sf_attribute_group);
	/* Initialize the interrupt callback. */
	INIT_WORK(&sf_ctl_dev.work_queue, sf_ctl_device_event);
#if MULTI_HAL_COMPATIBLE
	xprintk(KERN_INFO, " do not initialize the fingerprint interrupt.\n");
#else
	/*err = sf_ctl_init_irq();

	if (err) {
		xprintk(KERN_ERR, "sf_ctl_init_irq failed with %d.\n", err);
		input_unregister_device(sf_ctl_dev.input);
		misc_deregister(&sf_ctl_dev.miscdev);
		sf_platform_exit(&sf_ctl_dev);
		sf_ctl_dev.pdev = NULL;
		return err;
	}*/
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	sf_ctl_dev.early_suspend.level = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1);
	sf_ctl_dev.early_suspend.suspend = sf_early_suspend;
	sf_ctl_dev.early_suspend.resume = sf_late_resume;
	register_early_suspend(&sf_ctl_dev.early_suspend);
#elif defined(CONFIG_ADF_SPRD)
	sf_ctl_dev.adf_event_block.notifier_call = sf_adf_event_handler;
	err = adf_register_client(&sf_ctl_dev.adf_event_block);

	if (err < 0) {
		xprintk(KERN_ERR, "register adf notifier fail, cannot sleep when screen off");
	} else {
		xprintk(KERN_ERR, "register adf notifier succeed");
	}

#else
	sf_ctl_dev.notifier.notifier_call = sf_fb_notifier_callback;
	fb_register_client(&sf_ctl_dev.notifier);
#endif
	/* beanpod ISEE2.7 */
#if SF_BEANPOD_COMPATIBLE_V2_7
	{
		/* fortsense define, flow by trustonic */
		struct TEEC_UUID vendor_uuid = {0x0401c03f, 0xc30c, 0x4dd0,
			{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04}
		};
		memcpy(&uuid_fp, &vendor_uuid, sizeof(struct TEEC_UUID));
	}
	xprintk(KERN_ERR, "%s set beanpod isee2.7.0 uuid ok\n", __func__);
#endif
	xprintk(KERN_ERR, "%s leave\n", __func__);
	return err;
}

#if SF_SPI_TRANSFER
static int tee_spi_transfer(void *sf_conf, int cfg_len, const char *txbuf, char *rxbuf, int len)
{
	struct spi_transfer t;
	struct spi_message m;

	memset(&t, 0, sizeof(t));
	spi_message_init(&m);
	t.tx_buf = txbuf;
	t.rx_buf = rxbuf;
	t.bits_per_word = 8;
	t.len = len;
#if QUALCOMM_REE_DEASSERT
	t.speed_hz = qualcomm_deassert;
#else
	t.speed_hz = sf_ctl_dev.pdev->max_speed_hz;
#endif
	spi_message_add_tail(&t, &m);
	return spi_sync(sf_ctl_dev.pdev, &m);
}
#endif

static int sf_open(struct inode *inode, struct file *filp)
{
	int err = 0;

	if (sf_ctl_dev.gpio_init == NULL ||
		sf_ctl_dev.free_gpio == NULL ||
		sf_ctl_dev.reset == NULL) {
		pr_err("%s: gpio_init or free_gpio or reset is NULL\n", __func__);
		err = -1;
		return err;
	}

	pr_info("%s: fortsense, init gpio", __func__);
	err = sf_ctl_dev.gpio_init(&sf_ctl_dev);
	if (err) {
		pr_err("%s: fail to init gpio\n", __func__);
		goto err_init_gpio;
	}

	pr_info("%s: fortsense, do reset", __func__);
	sf_ctl_dev.reset(false);
	usleep_range(1000, 1001);
	sf_ctl_dev.reset(true);
	usleep_range(10000, 11000);

	pr_info("%s: fortsense, init irq", __func__);
	err = sf_ctl_init_irq();
	if (err) {
		pr_err("%s: fail to init irq\n", __func__);
		goto err_init_irq;
	}

	return err;

err_init_irq:
err_init_gpio:
	sf_ctl_dev.free_gpio(&sf_ctl_dev);

	return err;
}

static int sf_release(struct inode *inode, struct file *filp)
{
	int err = 0;

	pr_info("%s: disble_irq\n", __func__);
	disable_irq_wake(sf_ctl_dev.irq_num);
	disable_irq(sf_ctl_dev.irq_num);

	pr_info("%s : free_irq\n", __func__);
	free_irq(sf_ctl_dev.irq_num, (void *)&sf_ctl_dev);
	sf_ctl_dev.irq_num = 0;

	if (sf_ctl_dev.free_gpio != NULL && sf_ctl_dev.power_on != NULL) {
		pr_info("%s : free gpio and power off\n", __func__);
		sf_ctl_dev.free_gpio(&sf_ctl_dev);
		sf_ctl_dev.power_on(false);
	} else {
		pr_err("%s: free_gpio or power_on is NULL\n", __func__);
		err = -1;
	}

	return err;
}

#if SF_REE_PLATFORM
static ssize_t sf_ctl_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	const size_t bufsiz = sf_ctl_dev.spi_buf_size;
	ssize_t status = 0;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz) {
		return (-EMSGSIZE);
	}

	if (sf_ctl_dev.spi_buffer == NULL) {
		sf_ctl_dev.spi_buffer = kmalloc(bufsiz, GFP_KERNEL);

		if (sf_ctl_dev.spi_buffer == NULL) {
			xprintk(KERN_ERR, " %s malloc spi_buffer failed.\n", __func__);
			return (-ENOMEM);
		}
	}

	memset(sf_ctl_dev.spi_buffer, 0, bufsiz);

	if (copy_from_user(sf_ctl_dev.spi_buffer, buf, count)) {
		xprintk(KERN_ERR, "%s copy_from_user(..) failed.\n", __func__);
		return (-EMSGSIZE);
	}

	{
		/* not used */
		void *sf_conf;
		int cfg_len = 0;

		status = tee_spi_transfer(sf_conf, cfg_len, sf_ctl_dev.spi_buffer, sf_ctl_dev.spi_buffer, count);
	}

	if (status == 0) {
		status = copy_to_user(buf, sf_ctl_dev.spi_buffer, count);

		if (status != 0) {
			status = -EFAULT;
		} else {
			status = count;
		}
	} else {
		xprintk(KERN_ERR, " %s spi_transfer failed.\n", __func__);
	}

	return status;
}

static ssize_t sf_ctl_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	const size_t bufsiz = sf_ctl_dev.spi_buf_size;
	ssize_t status = 0;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz) {
		return (-EMSGSIZE);
	}

	if (sf_ctl_dev.spi_buffer == NULL) {
		sf_ctl_dev.spi_buffer = kmalloc(bufsiz, GFP_KERNEL);

		if (sf_ctl_dev.spi_buffer == NULL) {
			xprintk(KERN_ERR, " %s malloc spi_buffer failed.\n", __func__);
			return (-ENOMEM);
		}
	}

	memset(sf_ctl_dev.spi_buffer, 0, bufsiz);

	if (copy_from_user(sf_ctl_dev.spi_buffer, buf, count)) {
		xprintk(KERN_ERR, "%s copy_from_user(..) failed.\n", __func__);
		return (-EMSGSIZE);
	}

	{
		/* not used */
		void *sf_conf;
		int cfg_len = 0;

		status = tee_spi_transfer(sf_conf, cfg_len, sf_ctl_dev.spi_buffer, sf_ctl_dev.spi_buffer, count);
	}

	if (status == 0) {
		status = count;
	} else {
		xprintk(KERN_ERR, " %s spi_transfer failed.\n", __func__);
	}

	return status;
}
#endif

#if (SF_PROBE_ID_EN && !SF_RONGCARD_COMPATIBLE)
static int sf_read_sensor_id(void)
{
	int ret = -1;
	int trytimes = 3;
	char readbuf[16]  = {0};
	char writebuf[16] = {0};
	int cfg_len = 0;

	/*默认速度设置为1M, 不然8201/8211系列有可能读不到ID*/
#if (defined(CONFIG_MTK_SPI) || SF_TRUSTKERNEL_COMPAT_SPI_MT65XX)
	smt_conf.high_time = SF_DEFAULT_SPI_HIGH_TIME;
	smt_conf.low_time  = SF_DEFAULT_SPI_LOW_TIME;
	smt_conf.com_mod   = FIFO_TRANSFER;
	smt_conf.cpol	  = SPI_CPOL_0; /* SPI_MODE_0: cpol = 0 and cpha = 0*/
	smt_conf.cpha	  = SPI_CPHA_0;
#if (SF_COMPATIBLE_SEL == SF_COMPATIBLE_TRUSTKERNEL)
	cfg_len = sizeof(struct mt_chip_conf);
#else
	memcpy(&sf_ctl_dev.mt_conf, &smt_conf, sizeof(struct mt_chip_conf));
#endif
#else
	/* not used */
	int smt_conf;
#endif
	sf_ctl_dev.pdev->max_speed_hz = SF_DEFAULT_SPI_SPEED;
	sf_ctl_dev.pdev->bits_per_word = 8;
	sf_ctl_dev.pdev->mode = SPI_MODE_0;
	spi_setup(sf_ctl_dev.pdev);
	xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __func__);
	usleep_range(10000, 11000);

	do {
		/* 0.under display fingerprint */
		sf_ctl_dev.reset(false);
		usleep_range(1000, 1001);
		sf_ctl_dev.reset(true);
		msleep(50);
		memset(readbuf,  0, sizeof(readbuf));
		memset(writebuf, 0, sizeof(writebuf));

		do {
			int ic_type = -1;
			char chip_id[2] = {0x00, 0x00};

			writebuf[0] = 0x00;
			writebuf[1] = 0x00;
			writebuf[2] = 0x00;
			writebuf[3] = 0x00;
			writebuf[4] = 0x00;
			ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);

			if (ret != 0) {
				xprintk(KERN_ERR, "SPI transfer address 0x00 = 0x%02x failed, ret = %d\n",
						readbuf[4], ret);
				break;
			}

			chip_id[0] = readbuf[4];
			writebuf[0] = 0x00;
			writebuf[1] = 0x01;
			writebuf[2] = 0x00;
			writebuf[3] = 0x00;
			writebuf[4] = 0x00;
			ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);

			if (ret != 0) {
				xprintk(KERN_ERR, "SPI transfer address 0x01 = 0x%02x failed, ret = %d\n",
						readbuf[4], ret);
				break;
			}

			chip_id[1] = readbuf[4];

			if ((chip_id[0] == 0x42) && (chip_id[1] == 0x53)) {
				ic_type = 1;
				xprintk(KERN_INFO, "%s: found ic_type 1\n", __func__);
			} else if ((chip_id[0] == 0x53) && (chip_id[1] == 0x75)) {
				ic_type = 2;
				xprintk(KERN_INFO, "%s: found ic_type 2\n", __func__);
			} else {
				ic_type = -1;
				xprintk(KERN_ERR, "%s: unknown chip_id = 0x%02x, 0x%02x\n",
						__func__, chip_id[0], chip_id[1]);
				break;
			}

			if (ic_type == 1) {
				/* I2C config 0x42 */
				writebuf[0] = 0x01;
				writebuf[1] = 0x42;
				writebuf[2] = 0x00;
				writebuf[3] = 0x00;
				writebuf[4] = 0x00;
				writebuf[5] = 0x00;
				ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 7);

				if (ret != 0 || readbuf[6] != writebuf[2]) {
					xprintk(KERN_ERR, "SPI transfer readbuf[6] = %x failed, ret = %d\n",
							readbuf[6], ret);
					break;
				}

				/* I2C config 0x43 */
				writebuf[0] = 0x01;
				writebuf[1] = 0x43;
				writebuf[2] = 0x3b;
				writebuf[3] = 0x00;
				writebuf[4] = 0x00;
				writebuf[5] = 0x00;
				ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 7);

				if (ret != 0 || readbuf[6] != writebuf[2]) {
					xprintk(KERN_ERR, "SPI transfer readbuf[6] = %x failed, ret = %d\n",
							readbuf[6], ret);
					break;
				}

				/* I2C config 0x40 */
				writebuf[0] = 0x01;
				writebuf[1] = 0x40;
				writebuf[2] = 0x80;
				writebuf[3] = 0x00;
				writebuf[4] = 0x00;
				writebuf[5] = 0x00;
				ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 7);

				if (ret != 0 || readbuf[6] != writebuf[2]) {
					xprintk(KERN_ERR, "SPI transfer readbuf[6] = %x failed, ret = %d\n",
							readbuf[6], ret);
					break;
				}

				/* read chip ID 0x31 or 0x32 */
				writebuf[0] = 0x03;
				writebuf[1] = 0x49;
				writebuf[2] = 0x31;
				writebuf[3] = 0x08;
				writebuf[4] = (0x60 | 0x01);
				ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);
				usleep_range(2000, 2001);
				writebuf[0] = 0x00;
				writebuf[1] = 0x47;
				writebuf[2] = 0x00;
				writebuf[3] = 0x00;
				ret +=  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);

				if (ret != 0 || ((readbuf[4] != 0x31) && (readbuf[4] != 0x32))) {
					xprintk(KERN_ERR, "SPI transfer readbuf[4] = 0x%02x failed, ret = %d\n",
							readbuf[4], ret);
					break;
				}

				xprintk(KERN_INFO, "read under display chip type %d is ok\n", ic_type);
				return 0;
			}
			writebuf[0]  = 0xaa;
			writebuf[1]  = 0x06;
			writebuf[2]  = 0x00;
			writebuf[3]  = 0x00;
			writebuf[4]  = 0x00;
			writebuf[5]  = 0x00;
			writebuf[6]  = 0x04;
			writebuf[7]  = 0x31;
			writebuf[8]  = 0x08;
			writebuf[9]  = 0x00;
			writebuf[10] = 0x01;
			writebuf[11] = 0x00;
			writebuf[12] = 0x00;
			writebuf[13] = 0x00;
			ret = tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 14);
			usleep_range(2000, 2001);
			writebuf[0]  = 0xaa;
			writebuf[1]  = 0x07;
			writebuf[2]  = 0x00;
			writebuf[3]  = 0x00;
			writebuf[4]  = 0x00;
			writebuf[5]  = 0x00;
			writebuf[6]  = 0x01;
			writebuf[7]  = 0x00;
			writebuf[8]  = 0x00;
			writebuf[9]  = 0x00;
			writebuf[10] = 0x00;
			ret += tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 11);

			if (ret != 0 || ((readbuf[7] != 0x31) && (readbuf[7] != 0x32))) {
				xprintk(KERN_ERR, "SPI transfer readbuf[7] = 0x%02x failed, ret = %d\n",
						readbuf[7], ret);
				break;
			}

			xprintk(KERN_INFO, "read under display chip type %d is ok\n", ic_type);
			return 0;
		} while (0);

		/* 1.detect Fortsense ID */
		sf_ctl_dev.reset(false);
		usleep_range(1000, 1001);
		sf_ctl_dev.reset(true);
		usleep_range(10000, 11000);
		memset(readbuf,  0, sizeof(readbuf));
		memset(writebuf, 0, sizeof(writebuf));
		writebuf[0] = 0x65;
		writebuf[1] = (uint8_t)(~0x65);
		writebuf[2] = 0x00;
		writebuf[3] = 0x04;
		writebuf[4] = 0x20;
		writebuf[5] = 0x04;
		writebuf[6] = 0x20;
		ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 12);

		if (ret != 0) {
			xprintk(KERN_ERR, "SPI transfer failed\n");
			continue;
		}

		if ((readbuf[7] == 0x46) && (readbuf[8] == 0x74) && (readbuf[9] == 0x53)
			&& (readbuf[10] == 0x73) && (readbuf[11] == 0x8e)) {
			xprintk(KERN_INFO, "read fortsense id is ok\n");
			return 0;
		}

		/* 2.detect 8205, 8231, 8241 or 8271 */
		sf_ctl_dev.reset(false);
		usleep_range(1000, 1001);
		sf_ctl_dev.reset(true);
		usleep_range(10000, 11000);
		memset(readbuf,  0, sizeof(readbuf));
		memset(writebuf, 0, sizeof(writebuf));
		writebuf[0] = 0xA0;
		writebuf[1] = (uint8_t)(~0xA0);
		ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 6);

		if (ret != 0) {
			xprintk(KERN_ERR, "SPI transfer failed\n");
			continue;
		}

		if ((readbuf[2] == 0x53) && (readbuf[3] == 0x75) && (readbuf[4] == 0x6e)
			&& (readbuf[5] == 0x57)) {
			xprintk(KERN_INFO, "read chip is ok\n");
			return 0;
		}

		/* 3.detect 8202, 8205 or 8231 */
		sf_ctl_dev.reset(false);
		usleep_range(1000, 1001);
		sf_ctl_dev.reset(true);
		usleep_range(10000, 11000);
		memset(readbuf,  0, sizeof(readbuf));
		memset(writebuf, 0, sizeof(writebuf));
		writebuf[0] = 0x60;
		writebuf[1] = (uint8_t)(~0x60);
		writebuf[2] = 0x28;
		writebuf[3] = 0x02;
		writebuf[4] = 0x00;
		ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 7);

		if (ret != 0) {
			xprintk(KERN_ERR, "SPI transfer failed\n");
			continue;
		}

		if (readbuf[5] == 0x82) {
			xprintk(KERN_INFO, "read chip is ok\n");
			return 0;
		}

		/* 4.detect 8221 */
		sf_ctl_dev.reset(false);
		usleep_range(1000, 1001);
		sf_ctl_dev.reset(true);
		usleep_range(10000, 11000);
		memset(readbuf,  0, sizeof(readbuf));
		memset(writebuf, 0, sizeof(writebuf));
		writebuf[0] = 0x60;
		writebuf[1] = 0x28;
		writebuf[2] = 0x02;
		writebuf[3] = 0x00;
		ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 6);

		if (ret != 0) {
			xprintk(KERN_ERR, "SPI transfer failed\n");
			continue;
		}

		if (readbuf[4] == 0x82) {
			xprintk(KERN_INFO, "read chip is ok\n");
			return 0;
		}

		/* trustkernel bug, can not read more than 8 bytes ,
		#if 1, trustkernel has fix it with 12bytes in version 0.5.2,
		show trustkernel version you can see it by cat /proc/tkcore/tkcore_os_version*/
#if !SF_TRUSTKERNEL_COMPATIBLE
		/*#if 1*/
		/* 5.detect 8211 */
		{
			int retry_8201 = 0;

			do {
				sf_ctl_dev.reset(false);
				usleep_range(1000, 1001);
				sf_ctl_dev.reset(true);
				usleep_range(10000, 11000);
				/* reset 脚拉高后，
				需在 6ms~26ms 内读 ID，
				小于 6ms 或者超过 26ms 都读不到 ID */
				msleep(1 + retry_8201 * 2);
				memset(readbuf,  0, sizeof(readbuf));
				memset(writebuf, 0, sizeof(writebuf));
				writebuf[0] = 0x32;
				writebuf[1] = (uint8_t)(~0x32);
				writebuf[2] = 0x00;
				writebuf[3] = 0x00;
				writebuf[4] = 0x84;
				ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);

				if (ret != 0) {
					xprintk(KERN_ERR, "SPI transfer failed step 1\n");
					continue;
				}

				usleep_range(1000, 1001);
				memset(readbuf,  0, sizeof(readbuf));
				memset(writebuf, 0, sizeof(writebuf));
				writebuf[0] = 0x32;
				writebuf[1] = (uint8_t)(~0x32);
				writebuf[2] = 0x00;
				writebuf[3] = 0x00;
				writebuf[4] = 0x81;
				ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);

				if (ret != 0) {
					xprintk(KERN_ERR, "SPI transfer failed step 2\n");
					continue;
				}

				usleep_range(5000, 5001);
				memset(readbuf,  0, sizeof(readbuf));
				memset(writebuf, 0, sizeof(writebuf));
				writebuf[0] = 0x81;
				writebuf[1] = (uint8_t)(~0x81);
				writebuf[2] = 0x00;
				writebuf[3] = 0x00;
				writebuf[4] = 0x21;
				writebuf[5] = 0x00;
				writebuf[6] = 0x00;
				writebuf[7] = 0x00;
				ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 10);

				if (ret != 0) {
					xprintk(KERN_ERR, "SPI transfer failed step 3\n");
					continue;
				}

				if (readbuf[8] == 0x82) {
					xprintk(KERN_INFO, "read chip is ok: 0x%02x%02x\n", readbuf[8], readbuf[9]);
					memset(readbuf,  0, sizeof(readbuf));
					memset(writebuf, 0, sizeof(writebuf));
					writebuf[0] = 0x32;
					writebuf[1] = (uint8_t)(~0x32);
					writebuf[2] = 0x00;
					writebuf[3] = 0x00;
					writebuf[4] = 0x83;
					ret =  tee_spi_transfer(&smt_conf, cfg_len, writebuf, readbuf, 5);
					return 0;
				}
				xprintk(KERN_ERR, "read chip: 0x%02x%02x\n", readbuf[8], readbuf[9]);
			} while (retry_8201++ < 10);
		}
#endif
	} while (trytimes--);

	ret = -1;
	return ret;
}

#endif

static int sf_ctl_init_irq(void)
{
	int err = 0;
	unsigned long flags = IRQF_TRIGGER_FALLING; /* IRQF_TRIGGER_FALLING or IRQF_TRIGGER_RISING */

#if !SF_MTK_CPU
	flags |= IRQF_ONESHOT;
#if SF_INT_TRIG_HIGH
	flags |= IRQF_NO_SUSPEND;
#endif
#endif
	xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __func__);
	/* Register interrupt callback. */
	/*err = request_irq(sf_ctl_dev.irq_num, sf_ctl_device_irq,
					  flags, "sf-irq", NULL);*/
	err = request_threaded_irq(sf_ctl_dev.irq_num, NULL, sf_ctl_device_irq,
					flags, "sf-irq", &sf_ctl_dev);

	if (err) {
		xprintk(KERN_ERR, "request_irq(..) = %d.\n", err);
		return err;
	}

	enable_irq_wake(sf_ctl_dev.irq_num);
	xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __func__);
	return err;
}

static int sf_ctl_init_input(void)
{
	int err = 0;

	xprintk(SF_LOG_LEVEL, "%s(..) enter.\n", __func__);
	sf_ctl_dev.input = input_allocate_device();

	if (!sf_ctl_dev.input) {
		xprintk(KERN_ERR, "input_allocate_device(..) failed.\n");
		return (-ENOMEM);
	}

	sf_ctl_dev.input->name = "sf-keys";
	__set_bit(EV_KEY,	 sf_ctl_dev.input->evbit);
	__set_bit(KEY_HOME,   sf_ctl_dev.input->keybit);
	__set_bit(KEY_MENU,   sf_ctl_dev.input->keybit);
	__set_bit(KEY_BACK,   sf_ctl_dev.input->keybit);
	__set_bit(KEY_F11,	sf_ctl_dev.input->keybit);
	__set_bit(KEY_ENTER,  sf_ctl_dev.input->keybit);
	__set_bit(KEY_UP,	 sf_ctl_dev.input->keybit);
	__set_bit(KEY_LEFT,   sf_ctl_dev.input->keybit);
	__set_bit(KEY_RIGHT,  sf_ctl_dev.input->keybit);
	__set_bit(KEY_DOWN,   sf_ctl_dev.input->keybit);
	__set_bit(KEY_WAKEUP, sf_ctl_dev.input->keybit);
	err = input_register_device(sf_ctl_dev.input);

	if (err) {
		xprintk(KERN_ERR, "input_register_device(..) = %d.\n", err);
		input_free_device(sf_ctl_dev.input);
		sf_ctl_dev.input = NULL;
		return (-ENODEV);
	}

	xprintk(SF_LOG_LEVEL, "%s(..) leave.\n", __func__);
	return err;
}

static int __init sf_ctl_driver_init(void)
{
	int err = 0;

	xprintk(KERN_INFO, "'%s' SW_BUS_NAME = %s\n", __func__, SW_BUS_NAME);
#if SF_BEANPOD_COMPATIBLE_V1
	uint64_t fp_vendor_id = 0x00;

	get_t_device_id(&fp_vendor_id);
	xprintk(KERN_INFO, "'%s' fp_vendor_id = 0x%x\n", __func__, fp_vendor_id);

	if (fp_vendor_id != 0x02) {
		return 0;
	}

#endif
#if SF_SPI_RW_EN
	/**register SPI device、driver***/
	spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));
	err = spi_register_driver(&sf_driver);

	if (err < 0) {
		xprintk(KERN_ERR, "%s, Failed to register SPI driver.\n", __func__);
	}

#else
#if SF_REG_DEVICE_BY_DRIVER
	sf_device = platform_device_alloc("fortsense-fp", 0);

	if (sf_device) {
		err = platform_device_add(sf_device);

		if (err) {
			platform_device_put(sf_device);
			sf_device = NULL;
		}
	} else {
		err = -ENOMEM;
	}

#endif

	if (err) {
		xprintk(KERN_ERR, "%s, Failed to register platform device.\n", __func__);
	}

	err = platform_driver_register(&sf_driver);

	if (err) {
		xprintk(KERN_ERR, "%s, Failed to register platform driver.\n", __func__);
		return -EINVAL;
	}

#endif
	xprintk(KERN_INFO, "fortsense fingerprint device control driver registered.\n");
	xprintk(KERN_INFO, "driver version: '%s'.\n", sf_ctl_get_version());
	return err;
}

static void __exit sf_ctl_driver_exit(void)
{
#if SF_SPI_RW_EN
	spi_unregister_driver(&sf_driver);
#else
	platform_driver_unregister(&sf_driver);
#endif
#if SF_REG_DEVICE_BY_DRIVER

	if (sf_device) {
		platform_device_unregister(sf_device);
	}

#endif
	xprintk(KERN_INFO, "fortsense fingerprint device control driver released.\n");
}

module_init(sf_ctl_driver_init);
module_exit(sf_ctl_driver_exit);

MODULE_DESCRIPTION("The device control driver for Fortsense's fingerprint sensor.");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Fortsense");

