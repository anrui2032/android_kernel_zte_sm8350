/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <cam_sensor_cmn_header.h>
#include "cam_sensor_core.h"
#include "cam_sensor_io.h"
#include "cam_sensor_i2c.h"
#include "zte_camera_sensor_util.h"

#define CONFIG_ZTE_CAMERA_UTIL_DEBUG
#undef CDBG
#ifdef CONFIG_ZTE_CAMERA_UTIL_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


typedef struct {
	struct cam_sensor_ctrl_t *s_ctrl;
	enum camera_sensor_i2c_type msm_sensor_reg_data_type;
	enum camera_sensor_i2c_type msm_sensor_reg_addr_type;
	uint64_t address;
} msm_sensor_debug_info_t;

static int sensor_debugfs_datatype_s(void *data, u64 val)
{
	msm_sensor_debug_info_t *ptr = (msm_sensor_debug_info_t *) data;

	if (val < CAMERA_SENSOR_I2C_TYPE_MAX
		&& val >= CAMERA_SENSOR_I2C_TYPE_BYTE)
		ptr->msm_sensor_reg_data_type = val;

	CAM_DBG(CAM_SENSOR, "%s:%d: msm_sensor_reg_data_type = %d",
		__func__, __LINE__, ptr->msm_sensor_reg_data_type);

	return 0;
}

static int sensor_debugfs_datatype_g(void *data, u64 *val)
{
	msm_sensor_debug_info_t *ptr = (msm_sensor_debug_info_t *) data;

	*val = ptr->msm_sensor_reg_data_type;

	CAM_DBG(CAM_SENSOR, "%s:%d: msm_sensor_reg_data_type = %d",
		__func__, __LINE__, ptr->msm_sensor_reg_data_type);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_datatype, sensor_debugfs_datatype_g,
			sensor_debugfs_datatype_s, "%llx\n");

static int sensor_debugfs_addrtype_s(void *data, u64 val)
{
	msm_sensor_debug_info_t *ptr = (msm_sensor_debug_info_t *) data;

	if (val < CAMERA_SENSOR_I2C_TYPE_MAX
		&& val >= CAMERA_SENSOR_I2C_TYPE_BYTE)
		ptr->msm_sensor_reg_addr_type = val;

	CAM_DBG(CAM_SENSOR, "%s:%d: msm_sensor_reg_data_type = %d",
		__func__, __LINE__, ptr->msm_sensor_reg_data_type);
	return 0;
}

static int sensor_debugfs_addrtype_g(void *data, u64 *val)
{
	msm_sensor_debug_info_t *ptr = (msm_sensor_debug_info_t *) data;

	*val = ptr->msm_sensor_reg_addr_type;

	CAM_DBG(CAM_SENSOR, "%s:%d: msm_sensor_reg_addr_type = %d",
		__func__, __LINE__, ptr->msm_sensor_reg_addr_type);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_addrtype, sensor_debugfs_addrtype_g,
			sensor_debugfs_addrtype_s, "%llx\n");

static int sensor_debugfs_setaddr(void *data, u64 val)
{

	msm_sensor_debug_info_t *ptr = (msm_sensor_debug_info_t *) data;

	ptr->address = val;

	CAM_DBG(CAM_SENSOR, "%s:%d: address = 0x%llx",
		__func__, __LINE__, ptr->address);

	return 0;
}

static int sensor_debugfs_getaddr(void *data, u64 *val)
{
	msm_sensor_debug_info_t *ptr = (msm_sensor_debug_info_t *) data;

	*val = ptr->address;

	CAM_DBG(CAM_SENSOR, "%s:%d: address = 0x%llx",
		__func__, __LINE__, ptr->address);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_address, sensor_debugfs_getaddr,
			sensor_debugfs_setaddr, "%llx\n");

static int sensor_debugfs_setvalue(void *data, u64 val)
{
	msm_sensor_debug_info_t *ptr = (msm_sensor_debug_info_t *) data;
	int32_t rc = 0;

	CAM_DBG(CAM_SENSOR, "%s:%d: address = 0x%llx  value = 0x%llx",
		__func__, __LINE__, ptr->address, val);

	rc = zte_cam_cci_i2c_write(&(ptr->s_ctrl->io_master_info),
			ptr->address, val,
			ptr->msm_sensor_reg_addr_type,
			ptr->msm_sensor_reg_data_type);
	if (rc < 0) {
		pr_err("%s:%d: i2c write %llx failed", __func__, __LINE__, val);
		return rc;
	}

	return 0;
}

static int sensor_debugfs_getvalue(void *data, u64 *val)
{

	msm_sensor_debug_info_t *ptr = (msm_sensor_debug_info_t *) data;
	int32_t rc = 0;
	uint32_t temp;

	rc = camera_io_dev_read(
		&(ptr->s_ctrl->io_master_info),
		ptr->address, &temp,
		ptr->msm_sensor_reg_addr_type,
		ptr->msm_sensor_reg_data_type);

	if (rc < 0) {
		pr_err("%s:%d: i2c read %x failed", __func__, __LINE__, temp);
		return rc;
	}

	*val = temp;

	CAM_DBG(CAM_SENSOR, "%s:%d: address = 0x%llx  value = 0x%x\n",
		__func__, __LINE__, ptr->address, temp);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_value, sensor_debugfs_getvalue,
			sensor_debugfs_setvalue, "%llx\n");

struct dentry *debugfs_base = NULL;
int probe = 0;
#define BUF_SIZE 15

void msm_sensor_creat_debugfs(void)
{
	if (!debugfs_base) {
		debugfs_base = debugfs_create_dir("msm_sensor", NULL);

		if (!debugfs_base) {
			pr_err(": msm_sensor dir creat fail");
		}
	}
}

int msm_sensor_enable_debugfs(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct dentry  *sensor_dir;
	msm_sensor_debug_info_t *debug_ptr = NULL;
	char buf[BUF_SIZE];


	if (!debugfs_base) {
		debugfs_base = debugfs_create_dir("msm_sensor", NULL);
		if (!debugfs_base)
			return -ENOMEM;
	}

	if (probe & (1 << s_ctrl->soc_info.index)) {
		pr_err(": debug dir(sensor-%d) had creat before return ", s_ctrl->soc_info.index);
		return -ENOMEM;
	}

	debug_ptr = kzalloc(sizeof(msm_sensor_debug_info_t), GFP_KERNEL);
	if (!debug_ptr) {
		pr_err("failed: no memory s_ctrl %p", debug_ptr);
		return -ENOMEM;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf, BUF_SIZE, "sensor-%d", s_ctrl->soc_info.index);
	sensor_dir = debugfs_create_dir(buf, debugfs_base);
	if (!sensor_dir)
		goto debug_ptr_free;

	debug_ptr->s_ctrl = s_ctrl;
	debug_ptr->msm_sensor_reg_data_type = CAMERA_SENSOR_I2C_TYPE_WORD;

	if (!debugfs_create_file("datatype", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) debug_ptr, &sensor_debugfs_datatype))
		goto failed_create_file;

	if (!debugfs_create_file("addrtype", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) debug_ptr, &sensor_debugfs_addrtype))
		goto failed_create_file;

	if (!debugfs_create_file("address", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) debug_ptr, &sensor_debugfs_address))
		goto failed_create_file;

	if (!debugfs_create_file("value", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) debug_ptr, &sensor_debugfs_value))
		goto failed_create_file;

	probe |= (1 << s_ctrl->soc_info.index);

	return 0;

failed_create_file:
	debugfs_remove_recursive(sensor_dir);
	sensor_dir = NULL;
debug_ptr_free:
	kfree(debug_ptr);
	return -ENOMEM;
}

