
/************************************************************************
*
* File Name: himax_firmware_config.h
*
*  *   Version: v1.0
*
************************************************************************/
#ifndef _HIMAX_FIRMWARE_CONFIG_H_
#define _HIMAX_FIRMWARE_CONFIG_H_

/********************** Upgrade ***************************

  auto upgrade, please keep enable
*********************************************************/
#define HX_AUTO_UPDATE_FW
#define HX_SMART_WAKEUP
/* #define HX_HIGH_SENSE */
#define HIMAX_GET_FW_BY_LCM
#define HX_USB_DETECT_GLOBAL
#define HEADLINE_MODE

#ifdef HEADLINE_MODE
	#define fw_headline_addr                       0x10007FE8
#endif
/* id[1:0]: 0x00 0x01 0x10 0x11 */

enum himax_vendor_id {
	HX_VENDOR_ID_0	= 0x00,
	HX_VENDOR_ID_1,
	HX_VENDOR_ID_2,
	HX_VENDOR_ID_3,
	HX_VENDOR_ID_MAX		= 0xFF,
};

#define HXTS_VENDOR_0_NAME	"tianma"
#define HXTS_VENDOR_1_NAME	"unknown"
#define HXTS_VENDOR_2_NAME	"unknown"
#define HXTS_VENDOR_3_NAME	"unknown"

#ifdef HIMAX_GET_FW_BY_LCM
/*
 * Numbers of modules support
 */
#define HXTS_MODULE_NUM	1
#else
/*
 *Himax_firmware.bin file for auto upgrade, you must replace it with your own
 * define your own fw_bin
 */
#define HX_UPGRADE_FW0                  "Himax_firmware_dijing.bin"

#define HX_UPGRADE_FW1                  "Himax_firmware.bin"

#define HX_UPGRADE_FW2                  "Himax_firmware.bin"

#define HX_UPGRADE_FW3                  "Himax_firmware.bin"
#endif
#endif
