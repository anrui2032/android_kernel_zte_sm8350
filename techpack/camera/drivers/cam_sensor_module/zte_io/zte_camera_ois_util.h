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



#ifndef ZTE_CAMERA_OIS_UTIL_H

#define ZTE_CAMERA_OIS_UTIL_H

typedef struct {
	int cal_success;
	uint16_t reg1_cal_val;
	uint16_t reg2_cal_val;
	uint16_t reg3_cal_val;
} msm_cal_info_t;

typedef struct {
	struct cam_ois_ctrl_t *s_ctrl;
	enum camera_sensor_i2c_type msm_ois_reg_data_type;
	enum camera_sensor_i2c_type msm_ois_reg_addr_type;
	uint64_t address;
	msm_cal_info_t cal_info;
} msm_ois_debug_info_t;



void msm_ois_creat_debugfs(void);

int msm_ois_enable_debugfs(struct cam_ois_ctrl_t *s_ctrl);


#endif



