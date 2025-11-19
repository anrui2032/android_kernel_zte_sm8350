#ifndef __LINUX_HYNITRON_TS_H__
#define __LINUX_HYNITRON_TS_H__

#include  "hynitron_config.h"
#include  "tpd_sys.h"

#define HYN8xx_REG_FW_VER		0xA6
#define PRESS_MAX				255
#define HYN8xx_REG_POINT_RATE	0x88
/* #define TOUCH_VIRTUAL_KEYS */

/*register address*/
#define HYN_REG_CHIP_ID				0xA3    /* chip ID */
#define HYN_REG_FW_VER				0xA6    /* FW  version */
#define HYN_REG_VENDOR_ID			0xA8    /* TP vendor ID */
#define HYN_REG_PS_CTL				0xB0
#define TPD_MAX_POINTS_2                        2
#define TPD_MAX_POINTS_5                        5
#define TPD_MAXPOINTS_10                        10
#define AUTO_CLB_NEED                              1
#define AUTO_CLB_NONEED                          0

typedef unsigned char u8;

struct cst8xx_ts_platform_data {
	int irq_gpio_number;
	int reset_gpio_number;
	int power_gpio_number;
	const char *vdd_name;
#ifdef TOUCH_VIRTUAL_KEYS
	u32 virtualkeys[12];
#endif
	int TP_MAX_X;
	int TP_MAX_Y;
};

struct ts_event {
	u16	x1;
	u16	y1;
	u16	x2;
	u16	y2;
	u16	x3;
	u16	y3;
	u16	x4;
	u16	y4;
	u16	x5;
	u16	y5;
	u16	pressure;
	u8  touch_point;
};

struct cst8xx_ts_data {
	struct input_dev	*input_dev;
	struct input_dev    *ps_input_dev;
	struct i2c_client	*client;
	struct ts_event	event;
	struct device *dev;
#if USE_WORK_QUEUE
	struct work_struct	pen_event_work;
	struct workqueue_struct	*ts_workqueue;
#endif

	struct work_struct		 resume_work;
	struct workqueue_struct *ts_resume_workqueue;

	int    fw_update_flag;
	struct work_struct		 fwupdate_work;
	struct workqueue_struct *fwupdate_workqueue;

#if defined(CONFIG_ADF) || defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend	early_suspend;
#endif
	struct regulator *reg_vdd;
	struct cst8xx_ts_platform_data	*platform_data;
	spinlock_t irq_lock;
	s32 irq_is_disable;
	bool suspended;
};
typedef struct _COMMON_INTERFACE_VER_INFO_ {
	unsigned char  module_id;
	unsigned char  lcd_id;
	unsigned short fw_ver;
	unsigned short ic_checksum;
	unsigned char *pmodule_name;
} COMMON_INTERFACE_INFO;

#define HYN8XX_REG_PMODE    0xa5

#define PMODE_ACTIVE        0x00
#define PMODE_MONITOR       0x01
#define PMODE_STANDBY       0x02
#define PMODE_HIBERNATE     0x03

#ifndef ABS_MT_TOUCH_MAJOR
#define ABS_MT_TOUCH_MAJOR	0x30	/* touching ellipse */
#define ABS_MT_TOUCH_MINOR	0x31	/* (omit if circular) */
#define ABS_MT_WIDTH_MAJOR	0x32	/* approaching ellipse */
#define ABS_MT_WIDTH_MINOR	0x33	/* (omit if circular) */
#define ABS_MT_ORIENTATION	0x34	/* Ellipse orientation */
#define ABS_MT_POSITION_X	0x35	/* Center X ellipse position */
#define ABS_MT_POSITION_Y	0x36	/* Center Y ellipse position */
#define ABS_MT_TOOL_TYPE	0x37	/* Type of touching device */
#define ABS_MT_BLOB_ID		0x38	/* Group set of pkts as blob */
#define ABS_MT_PRESSURE		0x3a	/* Pressure on contact area */
#define ABS_MT_TRACKING_ID      0x39	/* Unique ID of initiated contact */
#endif /* ABS_MT_TOUCH_MAJOR */

#define hyn_info(x...)  pr_notice("[HYN] " x)
#define hyn_err(x...)   pr_err("[HYN][error] " x)

#endif
