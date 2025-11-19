/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*=========================================
*  Head Files :
* =========================================
*/
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <soc/qcom/subsystem_restart.h>

/*=========================================
*  Defines :
* =========================================
*/
#define SUBSYS_RESTART_VERSION  "1.0"
#define SYSFS_DEBUG_CONTROL_DIR_NAME "debug_control"

/*=========================================
*  Functions :
* =========================================
*/
static ssize_t modem_restart_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t enable;

	pr_info("%s: e\n", __func__);

	if (sscanf(buf, "%d\n", &enable) != 1)
		return -EINVAL;

	pr_info("enable = %d\n", enable);

	if (enable == 1) {
		pr_info("restart modem ...\n");
		subsystem_restart("modem");
	}
	return count;
}

static DEVICE_ATTR_WO(modem_restart);

static struct attribute *subsys_restart_attrs[] = {
	&dev_attr_modem_restart.attr,
	NULL,
};

static struct attribute_group subsys_restart_attr_grp = {
	.attrs = subsys_restart_attrs
};

static int32_t __init subsys_restart_init(void)
{
	int ret = -1;
	struct kobject *kobj = NULL;

	pr_info("%s: e\n", __func__);

	kobj = kobject_create_and_add(SYSFS_DEBUG_CONTROL_DIR_NAME, NULL);
	if (!kobj) {
		pr_err("%s:unable to create kobject\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_group(kobj, &subsys_restart_attr_grp);
	if (ret) {
		pr_err("cannot create sysfs group err: %d\n", ret);
		kobject_put(kobj);
		return ret;
	}

	pr_info("%s: x\n", __func__);

	return ret;
}

late_initcall(subsys_restart_init);

MODULE_DESCRIPTION("Vendor Subsys Restart Ver %s" SUBSYS_RESTART_VERSION);
MODULE_LICENSE("GPL v2");
