/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MacBook (Pro) SPI keyboard and touchpad driver
 *
 * Copyright (c) 2015-2019 Federico Lorenzi
 * Copyright (c) 2017-2019 Ronald Tschal鐩瞨
 */

#ifndef __CYPRESS8C40141_TOUCHKEY_STRINGIMAGE_H__
#define __CYPRESS8C40141_TOUCHKEY_STRINGIMAGE_H__

#ifdef TARGET_NUBIA_NX629J_V1S
#define CURRENT_NEW_FIRMWARE_VER    0x06

#define LINE_CNT_0 78

const char *stringImage_0[] = {
#include "NX666_CapSense_with_right_0306.cyacd"
};

#else
#define CURRENT_NEW_FIRMWARE_VER    0x06

#define LINE_CNT_0 78

const char *stringImage_0[] = {
#include "NX666_CapSense_with_right_0306.cyacd"
};

#endif

#endif
