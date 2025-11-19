/*
 * Goodix Gesture Module
 *
 * Copyright (C) 2019 - 2020 Goodix, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include "goodix_ts_core.h"
#include "../tpd_ufp_mac.h"

#define GSX_GESTURE_CMD				0x08

#define QUERYBIT(longlong, bit) (!!(longlong[bit/8] & (1 << bit%8)))

#define GSX_MAX_KEY_DATA_LEN    64
#define GSX_KEY_DATA_LEN	37
#define GSX_KEY_DATA_LEN_YS	42
#define GSX_GESTURE_TYPE_LEN	32

/*
 * struct gesture_module - gesture module data
 * @registered: module register state
 * @sysfs_node_created: sysfs node state
 * @gesture_type: store valid gesture type,each bit stand for a gesture
 * @gesture_data: gesture data
 * @gesture_ts_cmd: gesture command data
 */
struct gesture_module {
	atomic_t registered;
	unsigned int kobj_initialized;
	rwlock_t rwlock;
	unsigned char gesture_type[GSX_GESTURE_TYPE_LEN];
	unsigned char gesture_data[GSX_MAX_KEY_DATA_LEN];
	struct goodix_ext_module module;
	struct mutex cmd_lock;
	struct goodix_ts_cmd cmd;
};

static int zte_gsx_enter_gesture_mode(struct goodix_ts_core *core_data, int status);

static bool is_gesture_init = false;
static bool is_exit_init = false;

static struct gesture_module *gsx_gesture; /*allocated in gesture init module*/

/**
 * gsx_gesture_type_show_gt9886 - show valid gesture type
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to output buffer
 * Returns >=0 - succeed,< 0 - failed
 */
static ssize_t gsx_gesture_type_show_gt9886(struct goodix_ext_module *module,
				char *buf)
{
	int count = 0, i, ret = 0;
	unsigned char *type;

	if (atomic_read(&gsx_gesture->registered) != 1) {
		ts_info("Gesture module not register!");
		return -EPERM;
	}
	type = kzalloc(256, GFP_KERNEL);
	if (!type)
		return -ENOMEM;
	read_lock(&gsx_gesture->rwlock);
	for (i = 0; i < 256; i++) {
		if (QUERYBIT(gsx_gesture->gesture_type, i)) {
			type[count] = i;
			count++;
		}
	}
	type[count] = '\0';
	if (count > 0)
		ret = scnprintf(buf, PAGE_SIZE, "%s", type);
	read_unlock(&gsx_gesture->rwlock);

	kfree(type);
	return ret;
}

/**
 * gsx_gesture_type_store_gt9886 - set vailed gesture
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to valid gesture type
 * @count: length of buf
 * Returns >0 - valid gestures, < 0 - failed
 */
static ssize_t gsx_gesture_type_store_gt9886(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	int i;

	if (count <= 0 || count > 256 || buf == NULL) {
		ts_err("Parameter error");
		return -EINVAL;
	}

	write_lock(&gsx_gesture->rwlock);
	memset(gsx_gesture->gesture_type, 0, GSX_GESTURE_TYPE_LEN);
	for (i = 0; i < count; i++)
		gsx_gesture->gesture_type[buf[i]/8] |= (0x1 << buf[i]%8);
	write_unlock(&gsx_gesture->rwlock);

	return count;
}

static ssize_t gsx_gesture_enable_show_gt9886(struct goodix_ext_module *module,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 atomic_read(&gsx_gesture->registered));
}

static ssize_t gsx_gesture_enable_store_gt9886(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	unsigned int tmp;
	int ret;

	if (sscanf(buf, "%u", &tmp) != 1) {
		ts_info("Parameter illegal");
		return -EINVAL;
	}
	ts_debug("Tmp value =%d", tmp);

	if (tmp == 1) {
		if (atomic_read(&gsx_gesture->registered)) {
			ts_debug("Gesture module has aready registered");
			return count;
		}
		ret = goodix_register_ext_module_gt9886(&gsx_gesture->module);
		if (!ret) {
			ts_info("Gesture module registered!");
			atomic_set(&gsx_gesture->registered, 1);
		} else {
			atomic_set(&gsx_gesture->registered, 0);
			ts_err("Gesture module register failed");
		}
	} else if (tmp == 0) {
		if (!atomic_read(&gsx_gesture->registered)) {
			ts_debug("Gesture module has aready unregistered");
			return count;
		}
		ts_debug("Start unregistered gesture module");
		ret = goodix_unregister_ext_module_gt9886(&gsx_gesture->module);
		if (!ret) {
			atomic_set(&gsx_gesture->registered, 0);
			ts_info("Gesture module unregistered success");
		} else {
			atomic_set(&gsx_gesture->registered, 1);
			ts_info("Gesture module unregistered failed");
		}
	} else {
		ts_err("Parameter error!");
		return -EINVAL;
	}
	return count;
}

static ssize_t gsx_gesture_data_show_gt9886(struct goodix_ext_module *module,
				char *buf)
{
	ssize_t count;

	if (atomic_read(&gsx_gesture->registered) != 1) {
		ts_info("Gesture module not register!");
		return -EPERM;
	}
	if (!buf) {
		ts_info("Parameter error!");
		return -EPERM;
	}
	read_lock(&gsx_gesture->rwlock);

	count = scnprintf(buf, PAGE_SIZE, "Previous gesture type:0x%x\n",
			  gsx_gesture->gesture_data[2]);
	read_unlock(&gsx_gesture->rwlock);

	return count;
}

const struct goodix_ext_attribute gesture_attrs_gt9886[] = {
	__EXTMOD_ATTR(type, 0666, gsx_gesture_type_show_gt9886,
		gsx_gesture_type_store_gt9886),
	__EXTMOD_ATTR(enable, 0666, gsx_gesture_enable_show_gt9886,
		gsx_gesture_enable_store_gt9886),
	__EXTMOD_ATTR(data, 0444, gsx_gesture_data_show_gt9886, NULL)
};

/* double singe&fp onekey */
static inline void gsx_enable_dbl_single_onekey_click(struct goodix_ts_device *ts_dev)
{
	ts_info("%s\n", __func__);
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE) {
		gsx_gesture->cmd.cmd_reg = ts_dev->reg.command;
		gsx_gesture->cmd.length = 5;
		gsx_gesture->cmd.cmds[0] = GSX_GESTURE_CMD;
		gsx_gesture->cmd.cmds[1] = 0x0;
		gsx_gesture->cmd.cmds[2] = 0x0;
		gsx_gesture->cmd.cmds[3] = 0x0;
		gsx_gesture->cmd.cmds[4] = GSX_GESTURE_CMD;
		gsx_gesture->cmd.initialized = 1;
	}
}

/* double singe&fp */
static inline void gsx_enable_dbl_single_click(struct goodix_ts_device *ts_dev)
{
	ts_info("%s\n", __func__);
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE) {
		gsx_gesture->cmd.cmd_reg = ts_dev->reg.command;
		gsx_gesture->cmd.length = 5;
		gsx_gesture->cmd.cmds[0] = GSX_GESTURE_CMD;
		gsx_gesture->cmd.cmds[1] = 0x08;
		gsx_gesture->cmd.cmds[2] = 0x0;
		gsx_gesture->cmd.cmds[3] = 0x0;
		gsx_gesture->cmd.cmds[4] = 0x10;
		gsx_gesture->cmd.initialized = 1;
	}
}

/* double */
static inline void gsx_enable_dbl_click(struct goodix_ts_device *ts_dev)
{
	ts_info("%s\n", __func__);
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE) {
		gsx_gesture->cmd.cmd_reg = ts_dev->reg.command;
		gsx_gesture->cmd.length = 5;
		gsx_gesture->cmd.cmds[0] = GSX_GESTURE_CMD;
		gsx_gesture->cmd.cmds[1] = 0x0d;
		gsx_gesture->cmd.cmds[2] = 0x00;
		gsx_gesture->cmd.cmds[3] = 0x00;
		gsx_gesture->cmd.cmds[4] = 0x15;
		gsx_gesture->cmd.initialized = 1;
	}
}

/* singe&fp */
static inline void gsx_enable_single_click(struct goodix_ts_device *ts_dev)
{
	ts_info("%s\n", __func__);
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE) {
		gsx_gesture->cmd.cmd_reg = ts_dev->reg.command;
		gsx_gesture->cmd.length = 5;
		gsx_gesture->cmd.cmds[0] = GSX_GESTURE_CMD;
		gsx_gesture->cmd.cmds[1] = 0x0a;
		gsx_gesture->cmd.cmds[2] = 0x0;
		gsx_gesture->cmd.cmds[3] = 0x0;
		gsx_gesture->cmd.cmds[4] = 0x12;
		gsx_gesture->cmd.initialized = 1;
	}
}

/* singe&fp onekey */
static inline void gsx_enable_single_onekey_click(struct goodix_ts_device *ts_dev)
{
	ts_info("%s\n", __func__);
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE) {
		gsx_gesture->cmd.cmd_reg = ts_dev->reg.command;
		gsx_gesture->cmd.length = 5;
		gsx_gesture->cmd.cmds[0] = GSX_GESTURE_CMD;
		gsx_gesture->cmd.cmds[1] = 0x02;
		gsx_gesture->cmd.cmds[2] = 0x0;
		gsx_gesture->cmd.cmds[3] = 0x0;
		gsx_gesture->cmd.cmds[4] = 0x0a;
		gsx_gesture->cmd.initialized = 1;
	}
}
/* todo */
static int zte_gsx_enter_gesture_mode(struct goodix_ts_core *core_data, int status)
{
	int is_init_cmd = 1;
	int ret = -EINVAL;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	if (!ts_dev->reg.command) {
		ts_err("command reg can not be null");
		return -EINVAL;
	}

	/* we may test one_key here */
	status = status | core_data->zc.is_one_key << 3;

	mutex_lock(&gsx_gesture->cmd_lock);
	switch (status) {
	case (ZTE_GOODIX_SIN_TAP | ZTE_GOODIX_DOU_TAP | ZTE_GOODIX_FP | ZTE_GOODIX_ONE_KEY):
		gsx_enable_dbl_single_onekey_click(ts_dev);
		break;
	case (ZTE_GOODIX_DOU_TAP | ZTE_GOODIX_SIN_TAP | ZTE_GOODIX_FP):
		gsx_enable_dbl_single_click(ts_dev);
		break;
	case (ZTE_GOODIX_DOU_TAP):
		gsx_enable_dbl_click(ts_dev);
		break;
	case (ZTE_GOODIX_SIN_TAP | ZTE_GOODIX_FP):
		gsx_enable_single_click(ts_dev);
		break;
	case (ZTE_GOODIX_SIN_TAP | ZTE_GOODIX_FP | ZTE_GOODIX_ONE_KEY):
		gsx_enable_single_onekey_click(ts_dev);
		break;
	default:
		ts_err("status is error!\n", status);
		is_init_cmd = 0;
		break;
	}

	if (is_init_cmd) {
		ret = ts_dev->hw_ops->send_cmd(ts_dev, &gsx_gesture->cmd);
		msleep(20);
	}

	mutex_unlock(&gsx_gesture->cmd_lock);

	return ret;
}

static int gsx_gesture_init_gt9886(struct goodix_ts_core *core_data,
		struct goodix_ext_module *module)
{
	int i, ret = -EINVAL;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	if (!core_data || !ts_dev->hw_ops->write || !ts_dev->hw_ops->read) {
		ts_err("Register gesture module failed, ts_core unsupported");
		goto exit_gesture_init;
	}

	memset(gsx_gesture->gesture_type, 0, GSX_GESTURE_TYPE_LEN);
	memset(gsx_gesture->gesture_data, 0xff,
	       sizeof(gsx_gesture->gesture_data));

	ts_debug("Set gesture type manually");
	/* set all bit to 1 to enable all gesture wakeup */
	memset(gsx_gesture->gesture_type, 0xff, GSX_GESTURE_TYPE_LEN);

	if (gsx_gesture->kobj_initialized) {
		ret = 0;
		goto exit_gesture_init;
	}

	ret = kobject_init_and_add(&module->kobj, goodix_get_default_ktype_gt9886(),
			&core_data->pdev->dev.kobj, "gesture");

	if (ret) {
		ts_err("Create gesture sysfs node error!");
		goto exit_gesture_init;
	}

	ret = 0;
	for (i = 0; i < ARRAY_SIZE(gesture_attrs_gt9886) && !ret; i++)
		ret = sysfs_create_file(&module->kobj, &gesture_attrs_gt9886[i].attr);
	if (ret) {
		ts_err("failed create gst sysfs files");
		while (--i >= 0)
			sysfs_remove_file(&module->kobj, &gesture_attrs_gt9886[i].attr);

		kobject_put(&module->kobj);
		goto exit_gesture_init;
	}

	gsx_gesture->kobj_initialized = 1;

exit_gesture_init:
	return ret;
}

static int gsx_gesture_exit_gt9886(struct goodix_ts_core *core_data,
		struct goodix_ext_module *module)
{
	atomic_set(&gsx_gesture->registered, 0);
	return 0;
}

static int __gsx_gesture_before_suspend_gt9886(struct goodix_ts_core *core_data)
{
	const struct goodix_ts_hw_ops *hw_ops = core_data->ts_dev->hw_ops;
	int ret = 0;
	int status;

	if (hw_ops == NULL) {
		ts_err("Uninitialized doze command or hw_ops");
		goto exit;
	}

	/* one key is rely on fingerprint! do not test one_key here! */
	status = (core_data->zc.is_wakeup_gesture << 1) | core_data->zc.is_single_tap;

	if (status) {
			zte_gsx_enter_gesture_mode(core_data, status);
			if (ret != 0) {
				ts_err("failed enter gesture mode");
				goto exit;
			}
		ret = EVT_CANCEL_SUSPEND;
	}

exit:
	return ret;
}

static inline void goodix_print_coor(u8 *temp_data)
{
	int start_x, start_y;

	start_x = (temp_data[8] << 8) + temp_data[9];
	start_y = (temp_data[10] << 8) + temp_data[11];

	UFP_INFO("single touch coor[%d, %d]", start_x, start_y);
}

/**
 * gsx_gesture_ist_gt9886 - Gesture Irq handle
 * This functions is excuted when interrupt happended and
 * ic in doze mode.
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_CANCEL_IRQEVT  stop execute
 */
static int gsx_gesture_ist_gt9886(struct goodix_ts_core *core_data,
	struct goodix_ext_module *module)
{
	int ret;
	int key_data_len = 0;
	u8 clear_reg = 0, checksum = 0, gsx_type = 0;
	u8 temp_data[GSX_MAX_KEY_DATA_LEN];
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	if (atomic_read(&core_data->suspended) == 0)
		return EVT_CONTINUE;

	if (!ts_dev->reg.gesture) {
		ts_err("gesture reg can't be null");
		return EVT_CONTINUE;
	}
	/* get ic gesture state*/
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE)
		key_data_len = GSX_KEY_DATA_LEN_YS;
	else
		key_data_len = GSX_KEY_DATA_LEN;

	ret = ts_dev->hw_ops->read_trans(ts_dev, ts_dev->reg.gesture,
					 temp_data, key_data_len);
	if (ret < 0 || ((temp_data[0] & GOODIX_GESTURE_EVENT)  == 0)) {
		ts_debug("invalid gesture event, ret=%d, temp_data[0]=0x%x",
			 ret, temp_data[0]);
		goto re_send_ges_cmd;
	}

	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE) {
		checksum = checksum_u8_ys(temp_data, 6 + 2);
		/*ts_err("checksum: %d\n", checksum);*/

		checksum += checksum_u8_ys(&(temp_data[8]), 32 + 2);
		/*ts_err("checksum: %d\n", checksum);*/
	} else
		checksum = checksum_u8(temp_data, key_data_len);
	if (checksum) {
		ts_err("Gesture data checksum error:0x%x", checksum);
		ts_info("Gesture data %*ph",
			(int)sizeof(temp_data), temp_data);
		goto re_send_ges_cmd;
	}
/*
	ts_debug("Gesture data:");
	ts_debug("data[0-4]0x%x, 0x%x, 0x%x, 0x%x, 0x%x", temp_data[0], temp_data[1],
		 temp_data[2], temp_data[3], temp_data[4]);
*/
	write_lock(&gsx_gesture->rwlock);
	memcpy(gsx_gesture->gesture_data, temp_data, key_data_len);
	write_unlock(&gsx_gesture->rwlock);

	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE)
		gsx_type = temp_data[4];
	else
		gsx_type = temp_data[2];

	if (core_data->zc.is_single_tap) {
		if (gsx_type == GSX_FP_TAP_AOD) {
			report_ufp_uevent(UFP_FP_DOWN);
			goto gesture_ist_exit;
		} else if (gsx_type == GSX_FP_UP_AOD) {
			report_ufp_uevent(UFP_FP_UP);
			goto gesture_ist_exit;
		}
	}

	if (QUERYBIT(gsx_gesture->gesture_type, gsx_type)) {
		if (gsx_type == GSX_DOUBLE_TAP &&
			core_data->zc.is_wakeup_gesture) { /* double tap */
			ufp_report_gesture_uevent(DOUBLE_TAP_GESTURE);
		} else if (gsx_type == GSX_SINGLE_TAP &&
				core_data->zc.is_single_tap) {
			ufp_report_gesture_uevent(SINGLE_TAP_GESTURE);
			goodix_print_coor(temp_data);
		} else {
			ts_info("Unsupported type:%x", gsx_type);
		}
		goto gesture_ist_exit;
	} else {
		ts_info("Unsupported gesture:%x", temp_data[2]);
	}

re_send_ges_cmd:
	__gsx_gesture_before_suspend_gt9886(core_data);

gesture_ist_exit:
	ts_dev->hw_ops->write_trans(ts_dev, ts_dev->reg.gesture,
				    &clear_reg, 1);
	return EVT_CANCEL_IRQEVT;
}

/**
 * gsx_gesture_before_suspend_gt9886 - execute gesture suspend routine
 * This functions is excuted to set ic into doze mode
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_IRQCANCLED  stop execute
 */
static int gsx_gesture_before_suspend_gt9886(struct goodix_ts_core *core_data,
	struct goodix_ext_module *module)
{
	int ret = 0;

	ret = __gsx_gesture_before_suspend_gt9886(core_data);

	ts_info("Set IC in gesture mode");
	atomic_set(&core_data->suspended, 1);

	return ret;
}

static int __gsx_set_edge_suppress(int *cmd_start,
	struct goodix_ts_core *core_data)
{
	int ret = -EINVAL;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	if (!ts_dev->reg.command) {
		ts_err("command reg can not be null");
		return -EINVAL;
	}

	mutex_lock(&gsx_gesture->cmd_lock);
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE) {
		gsx_gesture->cmd.cmd_reg = ts_dev->reg.command;
		gsx_gesture->cmd.length = 5;
		gsx_gesture->cmd.cmds[0] = cmd_start[0];
		gsx_gesture->cmd.cmds[1] = cmd_start[1];
		gsx_gesture->cmd.cmds[2] = cmd_start[2];
		gsx_gesture->cmd.cmds[3] = cmd_start[3];
		gsx_gesture->cmd.cmds[4] = cmd_start[4];
		gsx_gesture->cmd.initialized = 1;
	}

	ret = ts_dev->hw_ops->send_cmd(ts_dev, &gsx_gesture->cmd);
	msleep(20);
	mutex_unlock(&gsx_gesture->cmd_lock);

	return ret;
}

static int gsx_set_edge_suppress(int *cmd_start,
	struct goodix_ts_core *core_data,
	struct goodix_ext_module *module)
{
	int ret = 0;

	ret = __gsx_set_edge_suppress(cmd_start, core_data);

	ts_info("Set edge suppress, ret %d", ret);

	return ret;
}

static struct goodix_ext_module_funcs gsx_gesture_funcs = {
	.irq_event = gsx_gesture_ist_gt9886,
	.init = gsx_gesture_init_gt9886,
	.exit = gsx_gesture_exit_gt9886,
	.before_suspend = gsx_gesture_before_suspend_gt9886,
	.set_edge_suppress = gsx_set_edge_suppress,
};

/*int goodix_gsx_gesture_init_gt9886(void)
{
	initialize core_data->ts_dev->gesture_cmd
	int result;
	if (is_exit_init) {
		return 0;
	}
	ts_info("gesture module init");
	gsx_gesture = kzalloc(sizeof(struct gesture_module), GFP_KERNEL);
	if (!gsx_gesture)
		result = -ENOMEM;
	gsx_gesture->module.funcs = &gsx_gesture_funcs;
	gsx_gesture->module.priority = EXTMOD_PRIO_GESTURE;
	gsx_gesture->module.name = "Goodix_gsx_gesture";
	gsx_gesture->module.priv_data = gsx_gesture;
	gsx_gesture->kobj_initialized = 0;
	atomic_set(&gsx_gesture->registered, 0);
	rwlock_init(&gsx_gesture->rwlock);
	mutex_init(&gsx_gesture->cmd_lock);
	result = goodix_register_ext_module_gt9886(&(gsx_gesture->module));
	if (result == 0)
		atomic_set(&gsx_gesture->registered, 1);
	is_gesture_init = true;
	return result;
}*/

/*void goodix_gsx_gesture_exit_gt9886(void)
{
	int i, ret;

	ts_info("gesture module exit");
	is_exit_init = true;
	if (!is_gesture_init) {
		return;
	}
	if (atomic_read(&gsx_gesture->registered)) {
		ret = goodix_unregister_ext_module_gt9886(&gsx_gesture->module);
		atomic_set(&gsx_gesture->registered, 0);
	}
	if (gsx_gesture->kobj_initialized) {
		for (i = 0; i < ARRAY_SIZE(gesture_attrs_gt9886); i++)
			sysfs_remove_file(&gsx_gesture->module.kobj,
					  &gesture_attrs_gt9886[i].attr);

		kobject_put(&gsx_gesture->module.kobj);
	}

	kfree(gsx_gesture);
}*/

/*module_init(goodix_gsx_gesture_init_gt9886);
module_exit(goodix_gsx_gesture_exit_gt9886);*/

MODULE_DESCRIPTION("Goodix gsx Touchscreen Gesture Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
