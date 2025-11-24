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

#include "cam_ois_dev.h"

#include "cam_sensor_io.h"
#include "cam_sensor_i2c.h"
#include "zte_camera_ois_util.h"
#include "ois_bu63169.h"


#define CONFIG_ZTE_OIS_UTIL_DEBUG

#undef CDBG
#ifdef CONFIG_ZTE_OIS_UTIL_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


#define CAL_TIMES 16
#define CAL_THRES_MIN  -1444
#define CAL_THRES_MAX   1444

int ois_bu63169_debugfs_cal_s(void *data)
{
	msm_ois_debug_info_t *ptr = (msm_ois_debug_info_t *) data;
	enum camera_sensor_i2c_type reg_data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	enum camera_sensor_i2c_type reg_addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	int32_t rc = 0;
	int i;
	uint32_t temp = 0;
	uint32_t reg1_addr = 0x8455;
	uint32_t reg2_addr = 0x8456;
	int16_t reg1_val[CAL_TIMES];
	int32_t reg1_cal_sum = 0;
	int16_t reg2_val[CAL_TIMES];
	int32_t reg2_cal_sum = 0;

	ptr->cal_info.cal_success = 0;
	for (i = 0; i < CAL_TIMES; i++) {
		rc = camera_io_dev_read(&(ptr->s_ctrl->io_master_info),
			reg1_addr, &(temp), reg_addr_type, reg_data_type);
		if (rc < 0) {
			pr_err("%s:%d: i2c read addr[%d] = 0x%x  failed",
				__func__, __LINE__, i, reg1_addr);
			break;
		}
		if (temp & 0x8000)
			reg1_val[i] = (int16_t)(temp | 0xffff0000);
		else
			reg1_val[i] =  (int16_t)temp;
		if (reg1_val[i] < CAL_THRES_MIN || reg1_val[i] > CAL_THRES_MAX) {
			pr_err("%s:%d: i2c read 0x%x[%d] = 0x%x  %d outside error",
				__func__, __LINE__, reg1_addr, i, temp, reg1_val[i]);
			break;
		}
		reg1_cal_sum += reg1_val[i];
		pr_info("%s:%d: i2c read 0x%x[%d] = 0x%x  %d",
			__func__, __LINE__, reg1_addr, i, temp, reg1_val[i]);

		rc = camera_io_dev_read(&(ptr->s_ctrl->io_master_info),
			reg2_addr, &(temp), reg_addr_type, reg_data_type);
		if (rc < 0) {
			pr_err("%s:%d: i2c read addr[%d] = 0x%x  failed",
				__func__, __LINE__, i, reg2_val[i]);
			break;
		}
		if (temp & 0x8000)
			reg2_val[i] = (int16_t)(temp | 0xffff0000);
		else
			reg2_val[i] =  (int16_t)temp;
		if (reg2_val[i] < CAL_THRES_MIN || reg2_val[i] > CAL_THRES_MAX) {
			pr_err("%s:%d: i2c read 0x%x[%d] = 0x%x  %d outside error",
				__func__, __LINE__, reg2_addr, i, temp, reg2_val[i]);
			break;
		}
		reg2_cal_sum += reg2_val[i];
		pr_info("%s:%d: i2c read 0x%x[%d] = 0x%x  %d",
			__func__, __LINE__, reg2_addr, i, temp, reg2_val[i]);

		if (i == (CAL_TIMES - 1))
			ptr->cal_info.cal_success = 1;
	}
	if (ptr->cal_info.cal_success == 1) {
		for (i = 0; i < CAL_TIMES - 1; i++) {
			if (reg1_val[i] != reg1_val[CAL_TIMES - 1])
				break;
			if (i == (CAL_TIMES - 2)) {
				ptr->cal_info.cal_success = 0;
				pr_err("%s:%d: 0x%x reg1 all value same, error",
					__func__, __LINE__, reg1_val[i]);
			}
		}
		for (i = 0; i < CAL_TIMES - 1; i++) {
			if (reg2_val[i] != reg2_val[CAL_TIMES - 1])
				break;
			if (i == (CAL_TIMES - 2)) {
				ptr->cal_info.cal_success = 0;
				pr_err("%s:%d: 0x%x reg1 all value same, error",
					__func__, __LINE__, reg1_val[i]);
			}
		}
	}

	ptr->cal_info.reg1_cal_val  = (uint16_t)(reg1_cal_sum / CAL_TIMES);
	ptr->cal_info.reg2_cal_val  = (uint16_t)(reg2_cal_sum / CAL_TIMES);
	pr_info("%s:%d: cal_success = %d reg1_cal_val = 0x%x  reg2_cal_val = 0x%x",
		__func__, __LINE__, ptr->cal_info.cal_success, ptr->cal_info.reg1_cal_val,
		ptr->cal_info.reg2_cal_val);

	return 0;
}

int ois_bu63169_debugfs_cal_g(void *data, u64 *val)
{
	msm_ois_debug_info_t *ptr = (msm_ois_debug_info_t *) data;

	ois_bu63169_debugfs_cal_s(data);

	if (ptr->cal_info.cal_success == 1) {
		*val = ptr->cal_info.reg1_cal_val;
		*val = (*val << 16);
		*val |= ptr->cal_info.reg2_cal_val;
		*val |= 0x100000000;
	} else {
		*val = 0;
	}
	pr_info("%s:%d: cal_success = %d  0x%llx",
		__func__, __LINE__, ptr->cal_info.cal_success, *val);
	return 0;
}

