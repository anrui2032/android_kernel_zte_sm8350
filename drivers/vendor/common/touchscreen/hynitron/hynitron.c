/*
 * drivers/input/touchscreen/cst8xx_ts.c
 *
 * FocalTech cst8xx TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION	DATE		AUTHOR
 *	1.0		2010-01-05	WenFS
 *
 * note: only support mulititouch	Wenfs 2010-10-01
 */

#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/input/mt.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/suspend.h>
#include <linux/irq.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include <linux/hrtimer.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/unistd.h>
#include <asm/io.h>
#include <linux/pm_runtime.h>
#include <linux/types.h>
#include <linux/proc_fs.h>

#if defined(CONFIG_ADF)
#include <linux/notifier.h>
#include <video/adf_notifier.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include  "hynitron.h"
#include  "hynitron_ex_fun.h"
#include  "hynitron_ctl.h"
#include  "hynitron_config.h"
#include "hynitron_rawtest_config.h"

#define ENABLE_DEBUG_PRINT   (0)

#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP
#ifdef CONFIG_PM_WAKELOCKS
static struct wakeup_source tp_wakelock;
#else
static struct wake_lock tp_wakelock;
#endif
#endif

#ifdef TP_PROXIMITY_SENSOR
static unsigned char cst8xx_tpd_prox_old_state = 0;
static int cst8xx_tpd_prox_active = 0;
static int cst8xx_tpd_prox_enable = 0;
#endif
static unsigned char hyn_real_suspend_flag;

#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP

#define GESTURE_FUNCTION_SWITCH			"/data/misc/gesture/gesture_switch"
#define GESTURE_FUNCTION_CALL_SWITCH	"/data/misc/gesture/call_status_switch"

#define GESTURE_LEFT		0x20
#define GESTURE_RIGHT		0x21
#define GESTURE_UP			0x22
#define GESTURE_DOWN		0x23
#define GESTURE_DOUBLECLICK	0x24
#define GESTURE_O			0x30
#define GESTURE_W			0x31
#define GESTURE_M			0x32
#define GESTURE_E			0x33
#define GESTURE_C			0x34
#define GESTURE_V			0x54
#define GESTURE_Z			0x41
#define GESTURE_2			0x65
#define GESTURE_S			0x46
#define HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP_POINTS	255

/* add wake lock for gestrue */
#define GESTURE_WAKE_LOCK
#ifdef GESTURE_WAKE_LOCK
static struct wake_lock suspend_gestrue_lock;
#endif

static int s_gesture_switch = 1;
static int is_sleep = 0;
static short pointnum = 0;
extern suspend_state_t get_suspend_state(void);
#endif

extern void cst8xx_tpd_register_fw_class(void);

struct i2c_client *hyn_i2c_client;

#if USE_WAIT_QUEUE
static struct task_struct *thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int tpd_flag = 0;
#endif

struct cst8xx_ts_data *g_cst8xx_ts;
static struct i2c_client *this_client;
u8 tp_fm_ver = 0;

static u8 cst8xx_ts_debug = 0;
static struct cst8xx_ts_platform_data cst8xx_ts_info = {
	.irq_gpio_number	= HYN_GPIO_TOUCH_IRQ,
	.reset_gpio_number  = HYN_GPIO_TOUCH_RESET,
	.vdd_name		   = "vdd28",
	.TP_MAX_X = HYN_TP_MAX_X - 1,
	.TP_MAX_Y = HYN_TP_MAX_Y - 1,
};

#ifdef TOUCH_VIRTUAL_KEYS
struct cst8xx_key_setup_data {
	u16 key_y;
	u16 key_menu_x;
	u16 key_home_x;
	u16 key_back_x;
	u16 key_search_x;
};
const static struct cst8xx_key_setup_data cst8xx_key_setup[] = {
	/* key_y		key_menu_x		key_home_x		key_back_x		key_search_x */
#if defined(CONFIG_LCD_QHD)
	{1100,	60,	180,	300,	0},
	{1010,	90,	270,	360,	0},
	{980,	40,	120,	200,	0},
	{1030,	60,	170,	280,	0},
	{1400,	40,	120,	200,	0},
#elif defined(CONFIG_LCD_720P)
	{1350,	120,	360,	600,	0},
	{1400,	40,	120,	200,	0},
#else
	{900,	80,	240,	400,	0},
	{980,	40,	120,	200,	0},
#endif

};
#endif

#if HYN_EN_AUTO_UPDATE
unsigned char *p_cst836u_upgrade_firmware = NULL;
unsigned char  apk_upgrade_flag = 0;
extern int cst8xx_update_by_work(struct cst8xx_ts_data *ts);
#endif

static int cst8xx_i2c_rxdata(char *rxdata, int length)
{
	int ret = 0;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	if (i2c_transfer(this_client->adapter, msgs, 2) != 2) {
		ret = -EIO;
		hyn_err("msg %s i2c read error: %d\n", __func__, ret);
	}

	return ret;
}

static int cst8xx_i2c_txdata(char *txdata, int length)
{
	int ret = 0;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};

	if (i2c_transfer(this_client->adapter, msg, 1) != 1) {
		ret = -EIO;
		hyn_err("%s i2c write error: %d\n", __func__, ret);
	}

	return ret;
}

/***********************************************************************************************
Name	:	 cst8xx_write_reg

Input	:	addr -- address
					 para -- parameter

Output	:

function	:	write register of cst8xx

***********************************************************************************************/
static int cst8xx_write_reg(u8 addr, u8 para)
{
	u8 buf[3];
	int ret = -1;

	buf[0] = addr;
	buf[1] = para;
	ret = cst8xx_i2c_txdata(buf, 2);
	if (ret < 0) {
		hyn_err("write reg failed! %#x ret: %d", buf[0], ret);
		return -EPERM;
	}

	return 0;
}
/***********************************************************************************************
Name	:	cst8xx_read_reg

Input	:	addr
					 pdata

Output	:

function	:	read register of cst8xx

***********************************************************************************************/
#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP
static int cst8xx_read_reg(u8 addr, u8 *pdata)
{
	int ret = 0;
	u8 buf[2] = {addr, 0};

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= buf+1,
		},
	};

	if (i2c_transfer(this_client->adapter, msgs, 2) != 2) {
		ret = -EIO;
		hyn_err("msg %s i2c read error: %d\n", __func__, ret);
	}

	*pdata = buf[1];
	return ret;
}
#endif

#ifdef TP_PROXIMITY_SENSOR
static int cst8xx_prox_ctrl(int enable)
{
	hyn_info("%s enable%d\n", __func__, enable);

	if (enable == 1) {
		cst8xx_write_reg(0xb0, 0x01);
	} else if (enable == 0) {
		cst8xx_write_reg(0xb0, 0x00);
	}

	cst8xx_tpd_prox_active = enable;
	return 1;
}

static ssize_t tpd_psensor_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	hyn_info("%s active=%d\n", __func__, cst8xx_tpd_prox_enable);
	return snprintf(buf, 64, "%d\n", cst8xx_tpd_prox_enable);
}

static ssize_t tpd_psensor_enable_store(struct device *dev, struct device_attribute *attr, const char *buf,
					size_t count)
{
	unsigned int enable;
	int ret = 0;

	ret = kstrtouint(buf, 0, &enable);

	hyn_info("%s suspended=%d, enable=%d\n", __func__, g_cst8xx_ts->suspended, enable);

	if (ret)
		return -EINVAL;

	enable = (enable > 0) ? 1 : 0;

	cst8xx_tpd_prox_enable = enable;
	if (!g_cst8xx_ts->suspended) {
		cst8xx_prox_ctrl(enable);
	}

	return count;

}
static DEVICE_ATTR(enable, 0644, tpd_psensor_enable_show, tpd_psensor_enable_store);

static ssize_t tpd_psensor_flush_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 64, "%d\n", cst8xx_tpd_prox_enable);
}

static ssize_t tpd_psensor_flush_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	hyn_info("%s\n", __func__);

	input_report_abs(g_cst8xx_ts->ps_input_dev, ABS_DISTANCE, -1);
	input_mt_sync(g_cst8xx_ts->ps_input_dev);
	input_sync(g_cst8xx_ts->ps_input_dev);

	return count;

}
static DEVICE_ATTR(flush, 0644, tpd_psensor_flush_show, tpd_psensor_flush_store);

static struct attribute *tpd_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_flush.attr,
	NULL
};

static struct attribute_group tpd_attribute_group = {
	.attrs = tpd_attributes
};
#endif


/*******************************************************
Function:
	Disable irq function
Input:
	ts: cst8xx i2c_client private data
Output:
	None.
*********************************************************/
void cst8xx_irq_disable(struct cst8xx_ts_data *ts)
{
	unsigned long irqflags;

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (!ts->irq_is_disable) {
			ts->irq_is_disable = 1;
			disable_irq_nosync(ts->client->irq);
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

/*******************************************************
Function:
	Enable irq function
Input:
	ts: cst8xx i2c_client private data
Output:
	None.
*********************************************************/
void cst8xx_irq_enable(struct cst8xx_ts_data *ts)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (ts->irq_is_disable) {
			enable_irq(ts->client->irq);
			ts->irq_is_disable = 0;
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);

}


#ifdef TOUCH_VIRTUAL_KEYS
static void virtual_keys_check(u16 *x, u16 *y)
{
	int i;

	if (*y > HYN_TP_MAX_Y) {
		hyn_info("%s x = %d  y = %d\n", __func__, *x, *y);
	} else {
		return;
	}

	for (i = 0; i < sizeof(cst8xx_key_setup) / sizeof(struct cst8xx_key_setup_data); i++) {
		if (*y == cst8xx_key_setup[i].key_y) {
			if (*x == cst8xx_key_setup[i].key_menu_x)
				*x = MENU_CTP_BUTTON_X;
			else if (*x == cst8xx_key_setup[i].key_home_x)
				*x = HOME_CTP_BUTTON_X;
			else if (*x == cst8xx_key_setup[i].key_back_x)
				*x = BACK_CTP_BUTTON_X;
			else if (*x == cst8xx_key_setup[i].key_search_x)
				*x = SEARCH_CTP_BUTTON_X;

			*y = CTP_BUTTON_KEY_Y;
			break;
		}
	}
}


#if 0
static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		"0x%02x:%d:%d:%d:%d:%d:0x%02x:%d:%d:%d:%d:%d:0x%02x:%d:%d:%d:%d:%d:0x%02x:%d:%d:%d:%d:%d\n",
		EV_KEY, KEY_MENU, MENU_CTP_BUTTON_X, CTP_BUTTON_KEY_Y, BUTTON_WIDTH, BUTTON_HEIGHT,
		EV_KEY, KEY_HOMEPAGE, HOME_CTP_BUTTON_X, CTP_BUTTON_KEY_Y, BUTTON_WIDTH, BUTTON_HEIGHT,
		EV_KEY, KEY_BACK, BACK_CTP_BUTTON_X, CTP_BUTTON_KEY_Y, BUTTON_WIDTH, BUTTON_HEIGHT,
		EV_KEY, KEY_SEARCH, SEARCH_CTP_BUTTON_X, CTP_BUTTON_KEY_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
}
#else
static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}
#endif

static struct kobj_attribute virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.hynitron_ts",
		.mode = S_IRUGO,
	},
	.show = &virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
	&virtual_keys_attr.attr,
	NULL
};

static struct attribute_group properties_attr_group = {
	.attrs = properties_attrs,
};

static void cst8xx_ts_virtual_keys_init(void)
{
	int ret = 0;
	struct kobject *properties_kobj;

	hyn_info("%s\n", __func__);

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj, &properties_attr_group);
	if (!properties_kobj || ret)
		hyn_err("failed to create board_properties\n");
}

#endif

#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP
static int cst8xx_gesture_handle(void)
{
	unsigned char buf[HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP_POINTS * 2] = { 0 };
	int ret = -1;
	int gesture_key = 0;
	int gesture_id = 0;
	unsigned char ruby;
	struct cst8xx_ts_data *data = i2c_get_clientdata(this_client);

	buf[0] = 0xd3;
	msleep(20);

	ret = cst8xx_i2c_rxdata(buf, HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP_POINTS);
	if (ret < 0) {
		hyn_err("%s read touchdata failed.\n", __func__);
		return ret;
	}
	cst8xx_read_reg(0xd0, &ruby);

	if (buf[0] == 0xFE) {
		pointnum = (short)(buf[1]) & 0xff;

	} else {
		gesture_id = buf[0];
	}
	switch (gesture_id) {
	case 0x24:
		gesture_key = KEY_U;
		break;
	case GESTURE_LEFT:
		gesture_key = KEY_LEFT;
		break;
	case GESTURE_RIGHT:
		gesture_key = KEY_RIGHT;
		break;
	case GESTURE_UP:
		gesture_key = KEY_UP;
		break;
	case GESTURE_DOWN:
		gesture_key = KEY_DOWN;
		break;
	case GESTURE_O:
		gesture_key = KEY_O;
		break;
	case GESTURE_W:
		gesture_key = KEY_W;
		break;
	case GESTURE_M:
		gesture_key = KEY_M;
		break;
	case GESTURE_E:
		gesture_key = KEY_E;
		break;
	case GESTURE_C:
		gesture_key = KEY_C;
		break;
	case GESTURE_V:
		gesture_key = KEY_V;
		break;
	case GESTURE_S:
		gesture_key = KEY_S;
		break;
	case GESTURE_Z:
	case GESTURE_2:
		gesture_key = KEY_Z;
		break;
	case KEY_POWER:
		gesture_key = KEY_POWER;
		break;
	default:
		break;
	}

	hyn_info("%s gesture_key=%d,gesture_id=%x\n", __func__, gesture_key, gesture_id);

	if (gesture_key > 0) {
	   input_report_key(data->input_dev, gesture_key, 1);
	   input_sync(data->input_dev);
	   input_report_key(data->input_dev, gesture_key, 0);
	   input_sync(data->input_dev);
	}
	msleep(200);

#ifdef CONFIG_PM_WAKELOCKS
	__pm_wakeup_event(&tp_wakelock, 200);
#else
	wake_lock_timeout(&tp_wakelock, msecs_to_jiffies(200));
#endif

	return 0;
}

#endif

static void cst8xx_clear_report_data(struct cst8xx_ts_data *cst8xx_ts)
{
	int i;

	for (i = 0; i < TS_MAX_FINGER; i++) {
	#if MULTI_PROTOCOL_TYPE_B
		input_mt_slot(cst8xx_ts->input_dev, i);
		input_mt_report_slot_state(cst8xx_ts->input_dev, MT_TOOL_FINGER, false);
	#endif
	}
	input_report_key(cst8xx_ts->input_dev, BTN_TOUCH, 0);
	#if !MULTI_PROTOCOL_TYPE_B
		input_mt_sync(cst8xx_ts->input_dev);
	#endif
	input_sync(cst8xx_ts->input_dev);
}

static int cst8xx_update_data(void)
{
	struct cst8xx_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	u8 buf[33] = {0};
	int ret = -1;
	int i;
	u16 x, y;
	u8 pressure, size;
	static char finger_down[TS_MAX_FINGER] = {0};
	int index = 0;

#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP
	if (is_sleep == 1) {
#ifdef GESTURE_WAKE_LOCK
		wake_lock_timeout(&suspend_gestrue_lock, msecs_to_jiffies(2500));
#endif
		cst8xx_gesture_handle();
		return 0;
	}
#endif

#ifdef TP_PROXIMITY_SENSOR
	if (cst8xx_tpd_prox_active) {
		buf[0] = 0;
		ret = cst8xx_i2c_rxdata(buf, 4);
		event->touch_point = buf[2] & 0x07;

		if (ret < 0) {
			hyn_err("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
			return ret;
		}

#if (ENABLE_DEBUG_PRINT == 1)
		hyn_info("buf0=%d,buf1=%d,buf2=%d,buf3=%d.\n", buf[0], buf[1], buf[2], buf[3]);
		hyn_info("touch_point=%d.\n", event->touch_point);
#endif

		if (event->touch_point == 0) {
			if (((buf[1] == 0xc0) || (buf[1] == 0xe0)) && (cst8xx_tpd_prox_old_state != buf[1])) {
				input_report_abs(data->ps_input_dev, ABS_DISTANCE, (buf[1] == 0xc0) ? 0 : 1);
				input_mt_sync(data->ps_input_dev);
				input_sync(data->ps_input_dev);
				hyn_info("%s proximity report is %d.\n", __func__, (buf[1] == 0xc0) ? 0 : 1);
			}
#if (ENABLE_DEBUG_PRINT == 1)
			else {
				hyn_info("no proximity report.\n");
			}
#endif
			cst8xx_tpd_prox_old_state = buf[1];
		}
	}
#endif

	buf[0] = 0;
	ret = cst8xx_i2c_rxdata(buf, 16);/* ret = cst8xx_i2c_rxdata(buf, 33); */

	if (ret < 0) {
		hyn_err("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}

	memset(event, 0, sizeof(struct ts_event));
	event->touch_point = buf[2] & 0x07;

	if (event->touch_point > 2)
		event->touch_point = 2;

	for (i = 0; i < event->touch_point/*TS_MAX_FINGER*/; i++) {
		if ((buf[6*i+3] & 0xc0) == 0xc0)
			continue;
		x = (s16)(buf[6*i+3] & 0x0F)<<8 | (s16)buf[6*i+4];
		y = (s16)(buf[6*i+5] & 0x0F)<<8 | (s16)buf[6*i+6];
#ifdef TOUCH_VIRTUAL_KEYS
		virtual_keys_check(&x, &y);
#endif
		pressure = buf[6*i+7];
		if (pressure > 127 || pressure == 0)
			pressure = 127;
		size = (buf[6*i+8]>>4) & 0x0F;
		size = (size == 0 ? 1 : size);

		index = buf[6 * i + 5] >> 4;
		if ((buf[6*i+3] & 0x40) == 0x0) {
			if (!finger_down[index]) {
				finger_down[index] = 1;
				hyn_info("touch down id: %d, coord [%d:%d]\n", index, x, y);
			}
		#if MULTI_PROTOCOL_TYPE_B
			input_mt_slot(data->input_dev, buf[6*i+5]>>4);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
		#else
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, buf[6*i+5]>>4);
		#endif
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, y);
		#if HYN_PRESSURE_ENABLE
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, pressure);
		#endif
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, size);
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		#if !MULTI_PROTOCOL_TYPE_B
			input_mt_sync(data->input_dev);
		#endif

		#if (ENABLE_DEBUG_PRINT == 1)
			hyn_info("===x%d = %d,y%d = %d ====", i, x, i, y);
		#endif
		/* hyn_info("cst8xx touch_point %d ===x%d = %d,y%d = %d ====\n",event->touch_point,i, x, i, y); */
		} else {
			if (finger_down[index] == 1) {
				finger_down[index] = 0;
				hyn_info("touch up id: %d, coord [%d:%d]\n", index, x, y);
			}
		#if MULTI_PROTOCOL_TYPE_B
			input_mt_slot(data->input_dev, buf[6 * i + 5] >> 4);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
		#endif
		}
	}
	if (event->touch_point == 0) {
		for (i = 0; i < TS_MAX_FINGER; i++) {
			#if MULTI_PROTOCOL_TYPE_B
			input_mt_slot(data->input_dev, i);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			#endif
		}
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		#if !MULTI_PROTOCOL_TYPE_B
			input_mt_sync(data->input_dev);
		#endif
	}

	if (event->touch_point == 0) {
		cst8xx_clear_report_data(data);
	}

	input_sync(data->input_dev);

#if (ENABLE_DEBUG_PRINT == 1)
	hyn_info("touch report----report end event->touch_point:%d\n", event->touch_point);
#endif

	return 0;
}

#if USE_WAIT_QUEUE
static int touch_event_handler(void *unused)
{
	struct sched_param param = { .sched_priority = 5 };

	sched_setscheduler(current, SCHED_RR, &param);
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, (tpd_flag != 0));
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);
		cst8xx_update_data();

	} while (!kthread_should_stop());

	return 0;
}
#endif

#if USE_WORK_QUEUE
static void cst8xx_ts_pen_irq_work(struct work_struct *work)
{
	cst8xx_update_data();
	cst8xx_irq_enable(g_cst8xx_ts);
}
#endif

static irqreturn_t cst8xx_ts_interrupt(int irq, void *dev_id)
{
#if defined(HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP)
	if (s_gesture_switch) {
		irq_set_irq_type(this_client->irq, IRQF_TRIGGER_FALLING);
	}
#endif

#if USE_WAIT_QUEUE
	tpd_flag = 1;
	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
#endif

#if USE_WORK_QUEUE
/* disable_irq_nosync(hyn_i2c_client->irq); */
	struct cst8xx_ts_data *cst8xx_ts = (struct cst8xx_ts_data *)dev_id;

	if (!work_pending(&cst8xx_ts->pen_event_work)) {
		queue_work(cst8xx_ts->ts_workqueue, &cst8xx_ts->pen_event_work);
	}
	return IRQ_HANDLED;
#endif
}

void cst8xx_ts_reset(void)
{
	struct cst8xx_ts_platform_data *pdata = g_cst8xx_ts->platform_data;

	gpio_direction_output(pdata->reset_gpio_number, 1);
	mdelay(1);
	gpio_set_value(pdata->reset_gpio_number, 0);
	mdelay(10);
	gpio_set_value(pdata->reset_gpio_number, 1);
}

static void cst8xx_suspend(void)
{
#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP
	struct file *fp = NULL;
#endif
	int ret = -1;

	pr_info("%s\n", __func__);
	if (g_cst8xx_ts->fw_update_flag) {
		return;
	}

	if (g_cst8xx_ts->suspended) {
		hyn_info("Already in suspend state.\n");
		return;
	}

#ifdef TP_PROXIMITY_SENSOR
	if (cst8xx_tpd_prox_active) {
		hyn_real_suspend_flag = 0;
		enable_irq_wake(this_client->irq);
		g_cst8xx_ts->suspended = true;
		return;
	}
#endif

#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP
	fp = filp_open(GESTURE_FUNCTION_SWITCH, O_RDONLY, 0);
	if (!IS_ERR(fp)) {
		hyn_info("open file %s success!\n", GESTURE_FUNCTION_SWITCH);
		s_gesture_switch = 1;
		filp_close(fp, NULL);
	} else {
		hyn_info("open file %s error!\n", GESTURE_FUNCTION_SWITCH);
		s_gesture_switch = 0;
	}

	if (s_gesture_switch) {
		is_sleep = 1;
		cst8xx_write_reg(0xd0, 0x1);
		irq_set_irq_type(this_client->irq, IRQF_TRIGGER_LOW);
		return;
	}
#endif

	disable_irq_nosync(this_client->irq);

	ret = cst8xx_write_reg(HYN8XX_REG_PMODE, PMODE_HIBERNATE);
	if (ret) {
		hyn_info("%s  cst8xx_write_reg fail\n", __func__);
	}

	hyn_real_suspend_flag = 1;
	g_cst8xx_ts->suspended = true;
	/* disable_irq(this_client->irq); */
	/* cst8xx_irq_disable(g_cst8xx_ts); */
	cst8xx_clear_report_data(g_cst8xx_ts);

#if (ENABLE_DEBUG_PRINT == 1)
	hyn_info("%s end\n", __func__);
#endif
}

static void cst8xx_resume(void)
{
	struct cst8xx_ts_data  *cst8xx_ts = (struct cst8xx_ts_data *)i2c_get_clientdata(this_client);

#if (ENABLE_DEBUG_PRINT == 1)
	pr_info("%s\n", __func__);
#endif

	if (g_cst8xx_ts->fw_update_flag) {
		return;
	}

	if (g_cst8xx_ts->suspended == false) {
		hyn_info("Already in resume state.\n");
		return;
	}

	queue_work(cst8xx_ts->ts_resume_workqueue, &cst8xx_ts->resume_work);

#if (ENABLE_DEBUG_PRINT == 1)
	hyn_info("%s end\n", __func__);
#endif
}

int test_cst8xx_suspend(void)
{
	hyn_info("%s\n", __func__);

	cst8xx_suspend();
	return 0;
}

int test_cst8xx_resume(void)
{
	hyn_info("%s\n", __func__);

	cst8xx_resume();
	return 0;
}

static void cst8xx_ts_resume_work(struct work_struct *work)
{
	hyn_info("%s\n", __func__);

#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP
	is_sleep = 0;
	cst8xx_write_reg(0xd0, 0x00);
	irq_set_irq_type(this_client->irq, IRQF_TRIGGER_FALLING);
#endif

	cst8xx_clear_report_data(g_cst8xx_ts);

#ifdef TP_PROXIMITY_SENSOR
	/* disable irq to avoid i2c errors in irq handler when chip reset */
	if (cst8xx_tpd_prox_active) {
		disable_irq(this_client->irq);
		hyn_info("%s: disable irq for chip reset\n", __func__);
	}
#endif
	cst8xx_ts_reset();

#ifdef TP_PROXIMITY_SENSOR
	msleep(100);/* wait for stable */

	if (cst8xx_tpd_prox_active) {
		disable_irq_wake(this_client->irq);
		hyn_info("%s: disable irq wake\n", __func__);
		}

	if (cst8xx_tpd_prox_enable) {
		cst8xx_prox_ctrl(cst8xx_tpd_prox_enable);
		hyn_info("%s: enable tp_proximity\n", __func__);
	} else {
		cst8xx_tpd_prox_active = 0;
	}
#endif

	enable_irq(this_client->irq);

#if (ENABLE_DEBUG_PRINT == 1)
	hyn_info("%s resume,hyn_real_suspend_flag = %d\n", __func__, hyn_real_suspend_flag);
#endif

	hyn_real_suspend_flag = 0;
	g_cst8xx_ts->suspended = false;

#if (ENABLE_DEBUG_PRINT == 1)
	hyn_info("%s end\n", __func__);
#endif
}


#if defined(CONFIG_ADF)
static int ts_adf_event_handler(struct notifier_block *nb, unsigned long action, void *data)
{
	struct adf_notifier_event *event = data;
	int adf_event_data;

#if (ENABLE_DEBUG_PRINT == 1)
	hyn_info("%s\n", __func__);
#endif

	if (action != ADF_EVENT_BLANK)
		return NOTIFY_DONE;

	adf_event_data = *(int *)event->data;

	switch (adf_event_data) {
	case DRM_MODE_DPMS_ON:
		cst8xx_resume();
		break;
	case DRM_MODE_DPMS_OFF:
		cst8xx_suspend();
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

#elif defined(CONFIG_FB)
/*****************************************************************************
*  Name: fb_notifier_callback
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct cst8xx_ts_data *cst8xx_ts = container_of(self, struct cst8xx_ts_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && cst8xx_ts && cst8xx_ts->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			hyn_info("resume notifier.\n");
			cst8xx_resume();
		} else if (*blank == FB_BLANK_POWERDOWN) {
			hyn_info("suspend notifier.\n");
			cancel_work_sync(&cst8xx_ts->resume_work);
			cst8xx_suspend();
		}
	}

	return 0;
}

#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void cst8xx_ts_suspend(struct early_suspend *handler)
{
	hyn_info("%s\n", __func__);
	cst8xx_suspend();
}

static void cst8xx_ts_resume(struct early_suspend *handler)
{
	hyn_info("%s\n", __func__);
	cst8xx_resume();
}
#endif

unsigned char get_suspend_flag(void)
{
	return hyn_real_suspend_flag;
}

static int cst8xx_ts_hw_init(struct cst8xx_ts_data *cst8xx_ts)
{
	struct regulator *reg_vdd;
	struct i2c_client *client = cst8xx_ts->client;
	struct cst8xx_ts_platform_data *pdata = cst8xx_ts->platform_data;
	int ret = 0;

	hyn_info("%s [irq=%d];[rst=%d]\n", __func__, pdata->irq_gpio_number, pdata->reset_gpio_number);
	ret = gpio_request(pdata->irq_gpio_number, "cst8xx_ts_irq_pin");
	if (ret < 0) {
		hyn_err("%s request irq failed %d", __func__, ret);
		goto EXIT;
	}

	ret = gpio_request(pdata->reset_gpio_number, "cst8xx_ts_rst_pin");
	if (ret < 0) {
		hyn_err("%s request reset failed %d", __func__, ret);
		goto EXIT;
	}
	if (gpio_is_valid(pdata->power_gpio_number)) {
		if (gpio_request(pdata->power_gpio_number, "cst8xx_tp_power_gpio") < 0) {
			hyn_info("%s: request power gpio fail\n", __func__);
			goto EXIT;
		} else {
			gpio_direction_output(pdata->power_gpio_number, 1);
		}
	}

	gpio_direction_output(pdata->reset_gpio_number, 1);
	gpio_direction_input(pdata->irq_gpio_number);

	reg_vdd = regulator_get(&client->dev, pdata->vdd_name);
	if (!WARN(IS_ERR(reg_vdd), "[HYN] %s regulator: failed to get %s.\n", __func__, pdata->vdd_name)) {
		regulator_set_voltage(reg_vdd, 2800000, 2800000);
		ret = regulator_enable(reg_vdd);
		if (ret) {
			hyn_info("%s regulator_enable fail\n", __func__);
		}
		cst8xx_ts->reg_vdd = reg_vdd;
	} else {
		cst8xx_ts->reg_vdd = NULL;
	}
	msleep(100);
	cst8xx_ts_reset();
	msleep(200);

	return 0;
EXIT:
	return -EPERM;
}


#ifdef CONFIG_OF
static struct cst8xx_ts_platform_data *cst8xx_ts_parse_dt(struct device *dev)
{

	struct cst8xx_ts_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);

	hyn_info("%s\n", __func__);

	if (!pdata) {
		hyn_err("Could not allocate struct cst8xx_ts_platform_data");
		return NULL;
	}
	pdata->reset_gpio_number = of_get_gpio(np, 0);
	if (pdata->reset_gpio_number < 0) {
		hyn_err("fail to get reset_gpio_number\n");
		goto fail;
	}
	pdata->irq_gpio_number = of_get_gpio(np, 1);
	if (pdata->reset_gpio_number < 0) {
		hyn_err("fail to get reset_gpio_number\n");
		goto fail;
	}

	pdata->power_gpio_number = of_get_gpio(np, 2);
	if (pdata->power_gpio_number < 0) {
		hyn_info("no power_gpio_number %d.\n", pdata->power_gpio_number);
	}

	ret = of_property_read_string(np, "vdd_name", &pdata->vdd_name);
	if (ret) {
		hyn_err("fail to get vdd_name\n");
		goto fail;
	}
#ifdef TOUCH_VIRTUAL_KEYS
	ret = of_property_read_u32_array(np, "virtualkeys", pdata->virtualkeys, 12);
	if (ret) {
		hyn_err("fail to get virtualkeys\n");
		goto fail;
	}
#endif
	ret = of_property_read_u32(np, "TP_MAX_X", &pdata->TP_MAX_X);
	if (ret) {
		hyn_err("fail to get TP_MAX_X\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "TP_MAX_Y", &pdata->TP_MAX_Y);
	if (ret) {
		hyn_err("fail to get TP_MAX_Y\n");
		goto fail;
	}

	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif

static ssize_t cst8xx_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "cst8xx debug %d\n", cst8xx_ts_debug);
}

/* Allow users to enable/disable the device */
static ssize_t cst8xx_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long enable = 0;

	ret = kstrtoul(buf, 10, &enable);
	if (ret) {
		hyn_err("invalid param:%s", buf);
		return -EIO;
	}
	/* hyn_info("%s:enable=0x%lx\n", __func__, enable); */
	cst8xx_ts_debug = (enable > 0) ? 1 : 0;
	return count;

}

extern struct i2c_client *client_up;
extern int cst78xx_enter_bootmode(void);
static int cst8xx_detect(struct i2c_client *client)
{
	int ret = 0;

	client_up = client;
	client->addr = 0x6A;
	ret = cst78xx_enter_bootmode();
	client->addr = 0x15;

	return ret;
}

/*===================================================================
The definitions are located in src/include/stat.h
Possible options include:
S_IRUSR	/ S_IWUSR	enable read/write permission for user
S_IRGRP / S_IWGRP	enable read/write permission for group
S_IROTH / S_IWOTH	enable read/write permission for other
S_IRUGO / S_IWUGO	enable read/write perimission for user+group+other
Alternatively, you can directly use the number to indicate the permission, where S_IRUSR = 00400 and etc.
Current setting, S_IRUGO|S_IWUSR means 644.
=====================================================================*/
/* static DEVICE_ATTR(debug, S_IRUGO|S_IWUGO, cst8xx_debug_show, cst8xx_debug_store); */
static struct device_attribute ctp_dev_debug_attribute = __ATTR(debug, 0664, cst8xx_debug_show, cst8xx_debug_store);


static struct attribute *cst8xx_attributes[] = {
	&ctp_dev_debug_attribute.attr,
	NULL
};
static const struct attribute_group cst8xx_attr_group = {
	.attrs = cst8xx_attributes,
};

#define TEST_PASS					0x0000
#define TEST_BEYOND_MAX_LIMIT		0x0001
#define TEST_BEYOND_MIN_LIMIT		0x0002
#define TEST_KEY_STR_SIZE 6

static int testResult = -1;
static unsigned short m_os_test_buf[50];
static unsigned char  g_testdata_buf[100];
unsigned char *pcst8xx_test_cfg_buffer = NULL;
static int	 cst8xx_test_cfg_len = 0;
char *test_key_str[TEST_KEY_STR_SIZE] = { "sensor_num", "screen_idac_list", "test_cp_max", "test_cp_min",
"test_cmod_max", "test_cmod_min"};
extern char hyn_self_test_cfg_name[];
extern char *g_cst8xx_sensor_result_file;
extern int cst8xx_save_failed_node(int failed_node);

int cst8xx_fif_write(char *fname, u8 *pdata, u16 len)
{
	int ret = 0;
	loff_t pos = 0;
	static struct file *pfile = NULL;
	mm_segment_t old_fs = KERNEL_DS;

	pfile = filp_open(fname, O_TRUNC | O_CREAT | O_RDWR, 0644);
	if (IS_ERR(pfile)) {
		ret = -EFAULT;
		hyn_info("%s:open error!\n", __func__);
	} else {
		hyn_info("%s:start write!\n", __func__);
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		ret = (int)vfs_write(pfile, (__force const char __user *)pdata, (size_t)len, &pos);
		vfs_fsync(pfile, 0);
		filp_close(pfile, NULL);
		set_fs(old_fs);
	}
	return ret;
}

int hynitron_request_test_cfg(void)
{
	int ret = 0;
	u8 *tmpbuf = NULL;
	const struct firmware *fw = NULL;

	ret = request_firmware(&fw, hyn_self_test_cfg_name, g_cst8xx_ts->dev);
	cst8xx_test_cfg_len	 = 0;
	pcst8xx_test_cfg_buffer = NULL;
	if (ret == 0) {
		hyn_info("firmware request(%s) success", hyn_self_test_cfg_name);
		tmpbuf = vmalloc(fw->size + 1);
		if (tmpbuf == NULL) {
			hyn_err("test cfg buffer vmalloc fail\n");
			ret = -ENOMEM;
		} else {
			hyn_info("test cfg buffer vmalloc success\n");
			memcpy(tmpbuf, fw->data, fw->size);
			tmpbuf[fw->size] = '\n';
			cst8xx_test_cfg_len	 = fw->size + 1;
			pcst8xx_test_cfg_buffer = tmpbuf;
		}
	} else {
		hyn_err("firmware request(%s) fail,ret=%d", hyn_self_test_cfg_name, ret);
	}

	if (fw != NULL) {
		release_firmware(fw);
		fw = NULL;
	}
	return ret;
}

int get_oneline(unsigned char *pfile_buf, int start_pos, unsigned char *pvalid)
{
	int i;

	*pvalid = 0;
	i = start_pos;
	while (1) {
		if (pfile_buf[i] == '=') {
			*pvalid = 1;
		}

		if (pfile_buf[i] == '\n' || pfile_buf[i] == '\0') {
			break;
		}
		i++;
	}
	return i+1;
}

static int atoi(const char *str)
{
	int value = 0;

	while (*str >= '0' && *str <= '9') {
		value *= 10;
		value += *str - '0';
		str++;
	}
	return value;
}

int parse_valid_line(unsigned char *pfile_buf, int start_pos)
{
	int i, k, pos, end_flag, last_pos, num = 0, pos_equal;
	unsigned char *pbuf = pfile_buf+start_pos;
	unsigned char item[100];
	int ret, is_param_name, param_idx, node_idx;

	hyn_info("start_pos= %d\n", start_pos);

	ret = 0;
	i = 0;
	last_pos	= 0;
	pos_equal   = 0;
	is_param_name = 0;
	param_idx	 = 0;
	node_idx	  = 0;
	while (1) {
		if (pbuf[i] == '\n' || pbuf[i] == '\0') {
			break;
		}

		end_flag = 0;
		k = 0;

		pos = last_pos;
		while (1) {
			if (pbuf[pos] == '\n' || pbuf[pos] == '\0' || pbuf[pos] == ' ' || pbuf[pos] == '=') {
				if (pbuf[pos] == '\n' || pbuf[pos] == '\0') {
					end_flag = 1;
				} else if (pbuf[pos] == '=') {
					pos_equal = 1;
				}
				pos++;
				break;
			}

			item[k] = pbuf[pos];
			k++;
			pos++;
		}

		last_pos = pos;

		if (k > 0) {
			if (pos_equal && is_param_name) {
				item[k] = 0;
				num = atoi((const char *)item);

				switch (param_idx) {
				case 0:
					{
						cst8xx_test_info.chan_num = num;
					}
					break;
				case 1:
					{
						cst8xx_test_info.screen_idac_list[node_idx] = num;
					}
					break;
				case 2:
					{
						cst8xx_test_info.test_cp_max[node_idx] = num;
					}
					break;
				case 3:
					{
						cst8xx_test_info.test_cp_min[node_idx] = num;
					}
					break;
				case 4:
					{
						cst8xx_test_info.test_cmod_max[node_idx] = num;
					}
					break;
				case 5:
					{
						cst8xx_test_info.test_cmod_min[node_idx] = num;
					}
					break;
				case 6:
					{
						memcpy(cst8xx_test_info.fw_version, item, 5);
					}
					break;
				default:
					{
					}
					break;
				}

				node_idx++;
			} else {
				item[k] = 0;

				for (i = 0; i < TEST_KEY_STR_SIZE; i++) {
					if (memcmp(item, test_key_str[i], k) == 0) {
						hyn_info("key=%s,test_key_str[i]=%s,size=%d\n", item, test_key_str[i],
							   k);
						param_idx = i;
						is_param_name = 1;
						break;
					}
				}
			}
		}

		i = last_pos;
		if (end_flag)
			i--;
	}

	if (node_idx > 0) {
		cst8xx_test_info.test_param_cnt++;

		if (param_idx >= 1 && param_idx <= 3) {
			if (node_idx != cst8xx_test_info.chan_num) {
				hyn_info("parse test_cp param error,param_idx=%d,node_idx=%d\n", param_idx, node_idx);
				ret = -1;
			}
		} else if (param_idx == 4 || param_idx == 5) {
			if (node_idx != cst8xx_test_info.cmod_num) {
				hyn_info("parse test_cmod param error,param_idx=%d,node_idx=%d\n", param_idx, node_idx);
				ret = -2;
			}
		}
	}

	hyn_info("param_cnt=%d\n", cst8xx_test_info.test_param_cnt);
	hyn_info("parse one line end\n");

	return ret;
}


void print_test_cfg_params(void)
{
	int i;

	hyn_info("get test param from cfg file success!!!\n");

	hyn_info("chan_num=%d\n", cst8xx_test_info.chan_num);
	hyn_info("cmod_num=%d\n", cst8xx_test_info.cmod_num);

	hyn_info("screen_idac_list is: ");
	for (i = 0; i < cst8xx_test_info.chan_num; i++) {
		hyn_info("%d,", cst8xx_test_info.screen_idac_list[i]);
	}
	hyn_info("\n");

	hyn_info("test_cp_max is: ");
	for (i = 0; i < cst8xx_test_info.chan_num; i++) {
		hyn_info("%d,", cst8xx_test_info.test_cp_max[i]);
	}
	hyn_info("\n");

	hyn_info("test_cp_min is: ");
	for (i = 0; i < cst8xx_test_info.chan_num; i++) {
		hyn_info("%d,", cst8xx_test_info.test_cp_min[i]);
	}
	hyn_info("\n");

	hyn_info("test_cmod_max is: ");
	for (i = 0; i < cst8xx_test_info.cmod_num; i++) {
		hyn_info("%d,", cst8xx_test_info.test_cmod_max[i]);
	}
	hyn_info("\n");

	hyn_info("test_cmod_min is: ");
	for (i = 0; i < cst8xx_test_info.cmod_num; i++) {
		hyn_info("%d,", cst8xx_test_info.test_cmod_min[i]);
	}
	hyn_info("\n");

	hyn_info("fw_version=%s\n", cst8xx_test_info.fw_version);

}

int pasre_cfg_file(void)
{
	int i, ret = 0, line_num, valid_num, start_pos;
	unsigned char valid_line;

	cst8xx_test_info.test_param_cnt = 0;
	i = 0;
	line_num  = 0;
	valid_num = 0;
	start_pos = 0;
	while (i < cst8xx_test_cfg_len) {
		i = get_oneline(pcst8xx_test_cfg_buffer, start_pos, &valid_line);
		line_num++;

		if (valid_line) {
			valid_num++;
			ret = parse_valid_line(pcst8xx_test_cfg_buffer, start_pos);
		}
		hyn_info("i=%d,line_num=%d,valid_num=%d\n", i, line_num, valid_num);
		start_pos = i;
	}

	if (cst8xx_test_info.test_param_cnt != cst8xx_test_info.test_param_num) {
		hyn_err("test_param_cnt(%d)!=test_param_num(%d).\n", cst8xx_test_info.test_param_cnt,
			   cst8xx_test_info.test_param_num);
		ret = -3;
	}

	return ret;
}


int hynitron_chip_self_test_sub(void)
{
	int i, ret;
	int min_raw, max_raw;

	ret = 0;
	if (cst8xx_test_info.chan_num + cst8xx_test_info.cmod_num != HYN_ALL_CHANNEL_NUM) {
		ret = -1;
		hyn_info("test channel num error,%d,%d,%d\n", HYN_ALL_CHANNEL_NUM, cst8xx_test_info.chan_num,
			   cst8xx_test_info.cmod_num);
	}

	for (i = 0; i < 5; i++) {
		cst8xx_ts_reset();

		msleep(100);

		ret = cst8xx_write_reg(0x00, 0x04);

		msleep(1500);

		if (ret < 0)
			continue;

		ret = cst8xx_write_reg(0x00, 0x04);

		g_testdata_buf[0]  = 0;

		ret = cst8xx_i2c_rxdata((char *)g_testdata_buf, HYN_ALL_CHANNEL_NUM * 2 + 1);

		if (ret < 0)
			continue;

		if (g_testdata_buf[0] == 0x04) {	/* read the factory test mode */
			break;
		}
		ret = -2;
	}

/* #if (ENABLE_DEBUG_PRINT==1) */
	hyn_info("hynitron test buf(i=%d):\n", i);
	for (i = 0; i < HYN_ALL_CHANNEL_NUM * 2 + 1; i++) {
		hyn_info("%d ", g_testdata_buf[i]);
	}
	hyn_info("hynitron test buf end\n");
/* #endif */

	hyn_info("hynitron test data(i=%d):\n", i);
	for (i = 0; i < HYN_ALL_CHANNEL_NUM; i++) {
		m_os_test_buf[i] = (g_testdata_buf[i*2+1]<<8) + (g_testdata_buf[i*2+2]);
		hyn_info("%d ", m_os_test_buf[i]);
	}
	hyn_info("hynitron read test data end\n");


	if (ret >= 0) {
		ret = 0;

		for (i = 0; i < HYN_ALL_CHANNEL_NUM; i++) {
			if (i < cst8xx_test_info.cmod_num) {
				min_raw = cst8xx_test_info.test_cmod_min[i];
				max_raw = cst8xx_test_info.test_cmod_max[i];
			} else {
				min_raw = cst8xx_test_info.test_cp_min[i-cst8xx_test_info.cmod_num];
				max_raw = cst8xx_test_info.test_cp_max[i-cst8xx_test_info.cmod_num];
			}

			if (m_os_test_buf[i] < min_raw) {
				ret |= 1;

				cst8xx_save_failed_node(i);

#if (ENABLE_DEBUG_PRINT == 1)
				hyn_info("min err:%d,%d,%d\n", m_os_test_buf[i], min_raw, i);
#endif
			} else if (m_os_test_buf[i] > max_raw) {
				ret |= 2;

				cst8xx_save_failed_node(i);

#if (ENABLE_DEBUG_PRINT == 1)
				hyn_info("max err:%d,%d,%d\n", m_os_test_buf[i], max_raw, i);
#endif
			}
		}
	}

	if (g_cst8xx_sensor_result_file != NULL) {

		unsigned char *pstr_buf = NULL;
		char fileFullName[80] = {0};
		unsigned int flen = 0;

		if (ret == 0) {
			snprintf(fileFullName, sizeof(fileFullName), "%s.bin", g_cst8xx_sensor_result_file);
		} else {
			snprintf(fileFullName, sizeof(fileFullName), "%s_failed.bin", g_cst8xx_sensor_result_file);
		}
		hyn_info("save file name:%s\n", fileFullName);
		cst8xx_fif_write(fileFullName, (u8 *)m_os_test_buf, (u16)sizeof(m_os_test_buf));

		pstr_buf = kzalloc(1024, GFP_KERNEL);	/* auto clear */
		if (pstr_buf == NULL) {
			hyn_info("cst8xx_rawdata_test_allch error::alloc file buffer fail!\n");
			goto exit;
		}
		flen = 0;
		for (i = 0; i < HYN_ALL_CHANNEL_NUM; i++) {
			if (i && ((i % 0x7) == 0)) {
				flen += snprintf(pstr_buf + flen, 1024 - flen, "%05d,\n", m_os_test_buf[i]);
			} else {
				flen += snprintf(pstr_buf + flen, 1024 - flen, "%05d,", m_os_test_buf[i]);
			}
		}
		if (ret == 0) {
			snprintf(fileFullName, sizeof(fileFullName), "%s.txt", g_cst8xx_sensor_result_file);
		} else {
			snprintf(fileFullName, sizeof(fileFullName), "%s_failed.txt", g_cst8xx_sensor_result_file);
		}
		cst8xx_fif_write(fileFullName, (u8 *)pstr_buf, (u16)(flen + 1));
		hyn_info("save file name:%s\n", fileFullName);
		kfree(pstr_buf);
	}

exit:
	cst8xx_ts_reset();
	msleep(100);

	return ret;
}

int hynitron_cst8xx_self_test(void)
{
	int ret = 0;

	if (hynitron_request_test_cfg() == 0) {
		if (pasre_cfg_file() == 0) {
			print_test_cfg_params();
		} else {
			hyn_info("test cfg file is unvalid\n");
			ret = -1;
		}

		if (pcst8xx_test_cfg_buffer != NULL) {
			vfree(pcst8xx_test_cfg_buffer);
			pcst8xx_test_cfg_buffer = NULL;
		}
		if (ret < 0) {
			hyn_info("test fail, no valid test cfg file\n");
			goto TEST_EXIT;
		}
	} else {
		hyn_info("no test cfg file,exit\n");
		ret = -2;
		goto TEST_EXIT;
	}

	disable_irq(this_client->irq);

	msleep(200);

	ret = hynitron_chip_self_test_sub();
	if (ret != 0) {
		ret = hynitron_chip_self_test_sub();
	}

	testResult = ret;

	enable_irq(this_client->irq);

TEST_EXIT:

	return ret;
}

static int cst8xx_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cst8xx_ts_data *cst8xx_ts;
	struct input_dev *input_dev;
	struct cst8xx_ts_platform_data *pdata = client->dev.platform_data;
	int err = 0;

#ifdef TP_PROXIMITY_SENSOR
	struct input_dev *ps_input_dev;
#endif

#if ANDROID_TOOL_SURPORT
	int ret	= -1;
#endif

#ifdef CONFIG_OF
	struct device_node *np = client->dev.of_node;
#endif

	if (tpd_fw_cdev.TP_have_registered) {
		hyn_info("%s:TP have registered by other TP.\n", __func__);
		return -EPERM;
	}
	hyn_info("%s\n", __func__);

#ifdef CONFIG_OF
	if (np && !pdata) {
		pdata = cst8xx_ts_parse_dt(&client->dev);
		if (pdata) {
			client->dev.platform_data = pdata;
		} else {
			err = -ENOMEM;
			goto exit_alloc_platform_data_failed;
		}

		hyn_info("%s config of\n", __func__);
	}
#endif
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	cst8xx_ts = kzalloc(sizeof(*cst8xx_ts), GFP_KERNEL);
	if (!cst8xx_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	g_cst8xx_ts = cst8xx_ts;
	if (pdata) {
		cst8xx_ts->platform_data = pdata;
	} else {
		cst8xx_ts->platform_data = &cst8xx_ts_info;
		pdata = &cst8xx_ts_info;
	}
	cst8xx_tpd_prox_active = 0;
	cst8xx_tpd_prox_old_state = 0x00;	/* default is 0x00*/
	cst8xx_ts->suspended = false;
	cst8xx_ts->fw_update_flag = false;
	this_client = client;
	cst8xx_ts->client = client;
	hyn_i2c_client = client;
	cst8xx_ts->dev = &client->dev;

	if (cst8xx_ts_hw_init(cst8xx_ts) < 0) {
		goto  exit_gpio_request_failed;
	}

	i2c_set_clientdata(client, cst8xx_ts);

	if (cst8xx_detect(client) < 0) {
		hyn_err("%s chip detect failed", __func__);
		goto exit_chip_check_failed;
	}
	cst8xx_ts_reset();
	msleep(50);

	client->irq = gpio_to_irq(pdata->irq_gpio_number);

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_ADF) || defined(CONFIG_FB)
	INIT_WORK(&cst8xx_ts->resume_work, cst8xx_ts_resume_work);
	cst8xx_ts->ts_resume_workqueue = create_singlethread_workqueue("cst8xx_ts_resume_work");
	if (!cst8xx_ts->ts_resume_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
#endif

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		hyn_err("failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}

	cst8xx_ts->input_dev = input_dev;

#ifdef TP_PROXIMITY_SENSOR
	ps_input_dev = input_allocate_device();
	if (!ps_input_dev) {
		err = -ENOMEM;
		hyn_err("failed to allocate ps-input device\n");
		goto exit_input_register_device_failed;
	}
	cst8xx_ts->ps_input_dev = ps_input_dev;
	ps_input_dev->name = "proximity_tp";

	set_bit(EV_ABS, ps_input_dev->evbit);
	input_set_capability(ps_input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(ps_input_dev, ABS_DISTANCE, -1, 1, 0, 0);
	err = input_register_device(ps_input_dev);

	if (err) {
		hyn_err("failed to register ps-input device: %s\n", dev_name(&client->dev));
		goto exit_ps_input_register_device_failed;
	}
	err = sysfs_create_group(&ps_input_dev->dev.kobj, &tpd_attribute_group);
	if (err) {
		hyn_info("input create group failed.\n");
		goto exit_ps_input_register_device_failed;
	}
#endif

	__set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_X,  input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y,  input_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	__set_bit(KEY_MENU,  input_dev->keybit);
	__set_bit(KEY_BACK,  input_dev->keybit);
	__set_bit(KEY_HOMEPAGE,  input_dev->keybit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit); /* for google img 2018 04 23 */

#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP
	input_set_capability(input_dev, EV_KEY, KEY_LEFT);
	input_set_capability(input_dev, EV_KEY, KEY_RIGHT);
	input_set_capability(input_dev, EV_KEY, KEY_UP);
	input_set_capability(input_dev, EV_KEY, KEY_DOWN);
	input_set_capability(input_dev, EV_KEY, KEY_U);
	input_set_capability(input_dev, EV_KEY, KEY_O);
	input_set_capability(input_dev, EV_KEY, KEY_W);
	input_set_capability(input_dev, EV_KEY, KEY_M);
	input_set_capability(input_dev, EV_KEY, KEY_E);
	input_set_capability(input_dev, EV_KEY, KEY_C);
	input_set_capability(input_dev, EV_KEY, KEY_V);
	input_set_capability(input_dev, EV_KEY, KEY_S);
	input_set_capability(input_dev, EV_KEY, KEY_Z);
	input_set_capability(input_dev, EV_KEY, KEY_POWER);

#ifdef GESTURE_WAKE_LOCK
	wake_lock_init(&suspend_gestrue_lock, WAKE_LOCK_SUSPEND, "suspend_gestrue");
#endif

#endif

#if MULTI_PROTOCOL_TYPE_B
	input_mt_init_slots(input_dev, TS_MAX_FINGER, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->TP_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->TP_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 15, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
#if HYN_PRESSURE_ENABLE
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 127, 0, 0);
#endif
#if !MULTI_PROTOCOL_TYPE_B
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
#endif

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);


	input_dev->name = HYNITRIN_TS_NAME;
	err = input_register_device(input_dev);
	if (err) {
		hyn_err("%s failed to register input device: %s\n", __func__, dev_name(&client->dev));
		goto exit_ps_input_register_device_failed;
	}

#ifdef TOUCH_VIRTUAL_KEYS
	cst8xx_ts_virtual_keys_init();
#endif

	spin_lock_init(&cst8xx_ts->irq_lock);

	err = sysfs_create_group(&(cst8xx_ts->input_dev->dev.kobj), &cst8xx_attr_group);
	if (err < 0) {
		hyn_err("%s: could not sysfs_create_group\n", __func__);
		goto exit_ps_input_register_device_failed;
	}

#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP

#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_init(&tp_wakelock, "tp_input_wakelock");
#else
	wake_lock_init(&tp_wakelock, WAKE_LOCK_SUSPEND, "tp_input_wakelock");
#endif

#endif

#if defined(CONFIG_ADF)
	hyn_info("%s CONFIG_ADF\n", __func__);
	cst8xx_ts->fb_notif.notifier_call = ts_adf_event_handler;
	cst8xx_ts->fb_notif.priority = 1000;
	err = adf_register_client(&cst8xx_ts->fb_notif);
	if (err) {
		hyn_err("unable to register fb_notifier: %d", err);
	}
#elif defined(CONFIG_FB)
	cst8xx_ts->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&cst8xx_ts->fb_notif);
	if (ret) {
		hyn_err("Unable to register fb_notifier: %d", ret);
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	cst8xx_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	cst8xx_ts->early_suspend.suspend = cst8xx_ts_suspend;
	cst8xx_ts->early_suspend.resume	 = cst8xx_ts_resume;
	register_early_suspend(&cst8xx_ts->early_suspend);
	hyn_info("hynitron probe CONFIG_HAS_EARLYSUSPEND: %d\n", 10);
#endif

#ifdef HYN_CTL_IIC
	if (hyn_rw_iic_drv_init(client) < 0) {
		hyn_err("%s create hyn control iic driver failed\n", __func__);
	}
#endif

#if HYN_EN_AUTO_UPDATE
	cst8xx_update_by_work(cst8xx_ts);
#endif

#if ANDROID_TOOL_SURPORT
	ret = cst8xx_proc_fs_init();
	if (ret < 0) {
		hyn_info("create proc fs failed.\n");
	}

#if SYSFS_DEBUG
	hyn_create_sysfs(client);
#endif

#endif

#if USE_WORK_QUEUE
	INIT_WORK(&cst8xx_ts->pen_event_work, cst8xx_ts_pen_irq_work);

	cst8xx_ts->ts_workqueue = create_singlethread_workqueue("hyn-work-queue");
	if (!cst8xx_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_sysfs_create_group_failed;
	}
#endif

#if USE_WAIT_QUEUE
	thread = kthread_run(touch_event_handler, 0, "hyn-wait-queue");
	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		hyn_err("failed to create kernel thread: %d\n", err);
	}
#endif

	err = request_irq(client->irq, cst8xx_ts_interrupt,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->name, cst8xx_ts);
	if (err < 0) {
		hyn_err("%s request irq failed %d\n", __func__, err);
		goto exit_irq_request_failed;
	}

	cst8xx_tpd_register_fw_class();

	tpd_fw_cdev.TP_have_registered = true;

	hyn_info("%s success\n", __func__);
	return 0;

exit_irq_request_failed:

#if USE_WORK_QUEUE
exit_sysfs_create_group_failed:
#endif
#if  SYSFS_DEBUG
	hyn_release_sysfs(client);
#endif

	sysfs_remove_group(&(cst8xx_ts->input_dev->dev.kobj), &cst8xx_attr_group);
	input_unregister_device(input_dev);
	input_unregister_device(ps_input_dev);

exit_ps_input_register_device_failed:
#ifdef TP_PROXIMITY_SENSOR
	input_free_device(ps_input_dev);
#endif

exit_input_register_device_failed:
	input_free_device(input_dev);

exit_input_dev_alloc_failed:
#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_ADF)
	if (cst8xx_ts->ts_resume_workqueue) {
		destroy_workqueue(cst8xx_ts->ts_resume_workqueue);
	}
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_ADF) || defined(CONFIG_FB)
exit_create_singlethread:
#endif
exit_chip_check_failed:
	if (cst8xx_ts->reg_vdd) {
		regulator_disable(cst8xx_ts->reg_vdd);
		regulator_put(cst8xx_ts->reg_vdd);
	}
	gpio_free(pdata->irq_gpio_number);
	gpio_free(pdata->reset_gpio_number);
	if (gpio_is_valid(pdata->power_gpio_number))
		gpio_free(pdata->power_gpio_number);
exit_gpio_request_failed:
	kfree(cst8xx_ts);
	cst8xx_ts = NULL;
exit_alloc_data_failed:
exit_check_functionality_failed:
	if (pdata != NULL) {
		kfree(pdata);
	}
	i2c_set_clientdata(client, cst8xx_ts);
exit_alloc_platform_data_failed:

	hyn_info("%s failed\n", __func__);
	return err;
}

static int cst8xx_ts_remove(struct i2c_client *client)
{
	struct cst8xx_ts_data *cst8xx_ts = i2c_get_clientdata(client);

	hyn_info("%s\n", __func__);

#ifdef HYN_CTL_IIC
	hyn_rw_iic_drv_exit();
#endif

#if  SYSFS_DEBUG
	hyn_release_sysfs(client);
#endif

#if defined(CONFIG_ADF)
	adf_unregister_client(&cst8xx_ts->fb_notif);
#elif defined(CONFIG_FB)
	fb_unregister_client(&cst8xx_ts->fb_notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&cst8xx_ts->early_suspend);
#endif

	free_irq(client->irq, cst8xx_ts);
	input_unregister_device(cst8xx_ts->input_dev);
	input_free_device(cst8xx_ts->input_dev);
#if USE_WORK_QUEUE
	cancel_work_sync(&cst8xx_ts->pen_event_work);
	destroy_workqueue(cst8xx_ts->ts_workqueue);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	cancel_work_sync(&cst8xx_ts->resume_work);
	destroy_workqueue(cst8xx_ts->ts_resume_workqueue);
#endif

	cancel_work_sync(&cst8xx_ts->fwupdate_work);
	destroy_workqueue(cst8xx_ts->fwupdate_workqueue);

	if (cst8xx_ts->reg_vdd) {
		regulator_disable(cst8xx_ts->reg_vdd);
		regulator_put(cst8xx_ts->reg_vdd);
	}
	kfree(cst8xx_ts);
	cst8xx_ts = NULL;
	i2c_set_clientdata(client, cst8xx_ts);
#ifdef HYN_DRV_TOUCHSCREEN_GESTURE_WAKEUP
#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_trash(&tp_wakelock);
#else
	wake_lock_destroy(&tp_wakelock);
#endif
#endif
	return 0;
}

static const struct i2c_device_id cst8xx_ts_id[] = {
	{ HYNITRIN_TS_NAME, 0 }, { }
};

MODULE_DEVICE_TABLE(i2c, cst8xx_ts_id);

static const struct of_device_id hynitron_of_match[] = {
	   { .compatible = "hynitron,hynitron_ts", },
	   { }
};
MODULE_DEVICE_TABLE(of, hynitron_of_match);
static struct i2c_driver cst8xx_ts_driver = {
	.probe		= cst8xx_ts_probe,
	.remove		= cst8xx_ts_remove,
	.id_table	= cst8xx_ts_id,
	.driver	= {
		.name	= HYNITRIN_TS_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = hynitron_of_match,
	},
	/* .suspend = cst8xx_suspend, */
	/* .resume = cst8xx_resume, */
};

static int __init cst8xx_ts_init(void)
{
	hyn_info("HYN_DRIVER_VERSION: %s", HYN_DRIVER_VERSION);
	hyn_info("HYN_PROJECT_ID:		%s", HYN_PROJECT_ID);
#if HYN_IIC_REISTER_DRIVER_STATIC
	return i2c_add_driver(&cst8xx_ts_driver);
#else
	return sprd_add_i2c_device(&cst8xx_ts_setup, &cst8xx_ts_driver);
#endif

	hyn_info("hynitron late init end: %d\n", 0);
}

static void __exit cst8xx_ts_exit(void)
{
#if HYN_IIC_REISTER_DRIVER_STATIC
	i2c_del_driver(&cst8xx_ts_driver);
#else
	sprd_del_i2c_device(this_client, &cst8xx_ts_driver);
#endif

	hyn_info("hynitron exit end: %d\n", 0);
}

module_init(cst8xx_ts_init);
module_exit(cst8xx_ts_exit);

MODULE_AUTHOR("Hynitron Driver Team");
MODULE_DESCRIPTION("Hynitron Touchscreen Driver for Spread");
MODULE_LICENSE("GPL");
