/* SPDX-License-Identifier: GPL-2.0 */

/**
 * The definition for Fortsense's fingerprint sensor.
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

#ifndef __SF_DEF_H__
#define __SF_DEF_H__


/* platform lists */

/*************************************************
SF_REE_MTK		  联发科平台，android6以上版本
SF_REE_QUALCOMM	 高通平台
SF_REE_SPREAD	   展讯平台
SF_REE_HIKEY9600	华为麒麟960
SF_REE_MTK_L5_X	 联发科平台，android5版本

SF_TEE_BEANPOD	  豆荚TEE
SF_TEE_TRUSTKERNEL  甁砵TEE
SF_TEE_QSEE		 高通TEE
SF_TEE_TRUSTONIC	trustonic TEE
SF_TEE_RONGCARD	 融卡TEE
SF_TEE_TRUSTY	   展讯TEE
*************************************************/

#define SF_REE_MTK				  1
#define SF_REE_QUALCOMM			 2
#define SF_REE_SPREAD			   3
#define SF_REE_HIKEY9600			4
#define SF_REE_MTK_L5_X			 5

#define SF_TEE_BEANPOD			  80
#define SF_TEE_TRUSTKERNEL		  81
#define SF_TEE_QSEE				 82
#define SF_TEE_TRUSTONIC			83
#define SF_TEE_RONGCARD			 84
#define SF_TEE_TRUSTY			   85


/* COMPATIBLE mode lists */
#define SF_COMPATIBLE_NOF		   0	   /* 不做兼容，注意：不适用于豆荚 270 和 280 版 */
#define SF_COMPATIBLE_NOF_BP_V2_7   1	   /* 不做兼容，注意：只适用于豆荚 270 和 280 版 */
#define SF_COMPATIBLE_REE		   100	 /* REE 兼容 */
#define SF_COMPATIBLE_BEANPOD_V1	200	 /* 豆荚 V1 兼容 */
#define SF_COMPATIBLE_BEANPOD_V2	201	 /* 豆荚 V2 兼容 */
#define SF_COMPATIBLE_BEANPOD_V2_7  202	 /* 豆荚 270 和 280 兼容 */
#define SF_COMPATIBLE_TRUSTKERNEL   300	 /* 瓶钵兼容 */
#define SF_COMPATIBLE_QSEE		  400	 /* QSEE 兼容 */
#define SF_COMPATIBLE_TRUSTY		500	 /* 展讯兼容 */
#define SF_COMPATIBLE_RONGCARD	  600	 /* 融卡兼容 */
#define SF_COMPATIBLE_TRUSTONIC	 700	 /* trustonic 兼容 */


/* power mode lists */
#define PWR_MODE_NOF				0
#define PWR_MODE_GPIO			   1
#define PWR_MODE_REGULATOR		  2


#endif
