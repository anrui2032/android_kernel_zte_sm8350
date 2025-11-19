
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
#define HX_BOOT_UPGRADE
#define HX_SMART_WAKEUP
/* #define HX_HIGH_SENSE */
#define HX_USB_DETECT_GLOBAL
#define HEADLINE_MODE
#define HX_DISPLAY_ROTATION
#define HX_EDGE_LIMIT
#define HX_REPORT_BY_ZTE_ALGO
#ifdef HX_REPORT_BY_ZTE_ALGO
#define hx_left_edge_limit_v		6
#define hx_right_edge_limit_v		6
#define hx_left_edge_limit_h		6
#define hx_right_edge_limit_h		6
#define hx_left_edge_long_pess_v		20
#define hx_right_edge_long_pess_v	20
#define hx_left_edge_long_pess_h		40
#define hx_right_edge_long_pess_h	20
#define hx_long_press_max_count		80
#define hx_edge_long_press_check 0
#endif

#if defined(HX_FIX_TOUCH_INFO)
enum fix_touch_info {
	FIX_HX_RX_NUM = 0,
	FIX_HX_TX_NUM = 0,
	FIX_HX_BT_NUM = 0,
	FIX_HX_MAX_PT = 0,
	FIX_HX_XY_REVERSE = false,
	FIX_HX_INT_IS_EDGE = true,
	FIX_HX_PEN_FUNC = false,
#if defined(HX_TP_PROC_2T2R)
	FIX_HX_RX_NUM_2 = 0,
	FIX_HX_TX_NUM_2 = 0,
#endif
};
#endif

enum himax_vendor_id {
	HX_VENDOR_ID_0	= 0x00,
	HX_VENDOR_ID_1,
	HX_VENDOR_ID_2,
	HX_VENDOR_ID_3,
	HX_VENDOR_ID_MAX		= 0xFF,
};

/*
 * Numbers of modules support
 */
#define HXTS_MODULE_NUM	1

#define HXTS_VENDOR_0_NAME	"unknown"
#define HXTS_VENDOR_1_NAME	"unknown"
#define HXTS_VENDOR_2_NAME	"unknown"
#define HXTS_VENDOR_3_NAME	"unknown"

#endif
