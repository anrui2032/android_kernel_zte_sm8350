/*
 * novatek TouchScreen oem config.
 */

 #ifndef _ILITEK_CONFIG_H_
 #define _ILITEK_CONFIG_H_

 /* ---GPIO number--- */
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943

/* ---INT trigger mode--- */
/* #define IRQ_TYPE_EDGE_RISING 1 */
/* #define IRQ_TYPE_EDGE_FALLING 2 */
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_FALLING
 /* ---Touch info.--- */
#define TOUCH_DEFAULT_MAX_WIDTH 1080
#define TOUCH_DEFAULT_MAX_HEIGHT 2400
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 0

/* ---Customerized func.--- */
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 1

#define BOOT_UPDATE_FIRMWARE 1
#define BOOT_UPDATE_FIRMWARE_NAME nt36xx_firmware_name
#define MP_UPDATE_FIRMWARE_NAME   nt36xx_firmware_mp_name
#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65

/* ---ESD Protect.--- */
#define NVT_TOUCH_ESD_PROTECT 1
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#define NVT_REPORT_BY_ZTE_ALGO
#ifdef NVT_REPORT_BY_ZTE_ALGO
#define nvt_left_edge_limit_v		6
#define nvt_right_edge_limit_v		6
#define nvt_left_edge_limit_h		6
#define nvt_right_edge_limit_h		6
#define nvt_left_edge_long_pess_v		30
#define nvt_right_edge_long_pess_v	30
#define nvt_left_edge_long_pess_h		50
#define nvt_right_edge_long_pess_h	30
#define nvt_long_press_max_count		80
#define nvt_edge_long_press_check 1
#endif

#define NVT_MODULE_NUM	1

#define NVT_VENDOR_ID_0 0x5E0A
#define NVT_VENDOR_ID_1 0
#define NVT_VENDOR_ID_2 0
#define NVT_VENDOR_ID_3 0

#define NVT_VENDOR_0_NAME                         "tianma"
#define NVT_VENDOR_1_NAME                         "unknown"
#define NVT_VENDOR_2_NAME                         "unknown"
#define NVT_VENDOR_3_NAME                         "unknown"

extern char nt36xx_firmware_name[];
extern char nt36xx_firmware_mp_name[];
#endif