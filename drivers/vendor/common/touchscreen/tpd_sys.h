/*
 * FILE:__TSP_FW_CLASS_H_INCLUDED
 *
 */
#ifndef __TPD_FW_H_INCLUDED
#define __TPD_FW_H_INCLUDED

#include <linux/device.h>
#include <linux/rwsem.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#ifdef TPD_DMESG
#undef TPD_DMESG
#endif
#define TPD_DMESG(a, arg...) pr_notice("tpd: " a, ##arg)

#define CONFIG_CREATE_TPD_SYS_INTERFACE

#define PROC_TOUCH_DIR					"touchscreen"
#define PROC_TOUCH_INFO					"ts_information"
#define PROC_TOUCH_FW_UPGRADE			"FW_upgrade"
#define PROC_TOUCH_GET_TPRAWDATA		"get_tprawdata"
#define PROC_TOUCH_RW_REG				"rw_reg"
#define PROC_TOUCH_SMART_COVER		"smart_cover"
#define PROC_TOUCH_GLOVE				"glove_mode"
#define PROC_TOUCH_WAKE_GESTURE		"wake_gesture"
#define PROC_TOUCH_SUSPEND		"suspend"
#define PROC_TOUCH_HEADSET_STATE		"headset_state"
#define PROC_TOUCH_MROTATION		"mRotation"
#define PROC_TOUCH_TP_SWITCH		"tp_switch"
#define PROC_TOUCH_TP_SINGLETAP		"single_tap"
#define PROC_TOUCH_GET_NOISE		"get_noise"
#define PROC_TOUCH_EDGE_REPORT_LIMIT		"edge_report_limit"
#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
#define PROC_TOUCH_SCREEN_STATE		"screen_state_interface"
#endif
#define PROC_TOUCH_ONEKEY			"one_key"
#define PROC_TOUCH_PLAY_GAME		"play_game"

/*gesture for tp, begin*/
#define  KEY_GESTURE_DOUBLE_CLICK 214
#define  MAX_VENDOR_NAME_LEN 20
#define  MAX_LIMIT_NOM 4
#define VENDOR_END 0xff

#ifdef CONFIG_TOUCHSCREEN_GOODIX_GTX8
#define RT_DATA_NUM	(5 << 3)
#else
#define RT_DATA_NUM	(5 << 2)
#endif

/* RT_DATA_LEN < 4096 */
#define RT_DATA_LEN	(4096 - 8)
#define EDGE_LIMIT_PIXEL_HEIGHT 900

struct tp_runtime_data {
	char *rt_data;
	bool is_empty;
	struct list_head list;
};

enum ts_chip {
	TS_CHIP_INDETER		= 0x00,
	TS_CHIP_SYNAPTICS	= 0x01,
	TS_CHIP_ATMEL		= 0x02,
	TS_CHIP_CYTTSP		= 0x03,
	TS_CHIP_FOCAL		= 0x04,
	TS_CHIP_GOODIX		= 0x05,
	TS_CHIP_MELFAS		= 0x06,
	TS_CHIP_MSTAR		= 0x07,
	TS_CHIP_HIMAX		= 0x08,
	TS_CHIP_NOVATEK		= 0x09,
	TS_CHIP_ILITEK		= 0x0A,
	TS_CHIP_TLSC		= 0x0B,
	TS_CHIP_CHIPONE	= 0x0C,
	TS_CHIP_HYNITRON	= 0x0D,

	TS_CHIP_MAX		= 0xFF,
};

struct tp_rwreg_t {
	int type;		/*  0: read, 1: write */
	int reg;		/*  register */
	int len;		/*  read/write length */
	int val;		/*  length = 1; read: return value, write: op return */
	int res;		/*  0: success, otherwise: fail */
	char *opbuf;	/*  length >= 1, read return value, write: op return */
};

enum tp_test_type {
	RAWDATA_TEST = 0,
	DELTA_TEST = 1,
};

enum {
	PROC_SUSPEND_NODE = 0,
	SYS_SUSPEND_NODE = 1,
};

enum {
	REG_OP_READ = 0,
	REG_OP_WRITE = 1,
};

enum {
	REG_CHAR_NUM_2 = 2,
	REG_CHAR_NUM_4 = 4,
	REG_CHAR_NUM_8 = 8,
};

enum {
	mRotatin_0 = 0,
	mRotatin_90 = 1,
	mRotatin_180 = 2,
	mRotatin_270 = 3,
};

enum {
	edge_limit_level_0 = 0,
	edge_limit_level_1 = 1,
	edge_limit_level_2 = 2,
	edge_limit_level_3 = 3,
	edge_limit_level_4 = 4,
	edge_limit_level_5 = 5,
	edge_limit_level_6 = 6,
	edge_limit_level_7 = 7,
	edge_limit_level_8 = 8,
	edge_limit_level_9 = 9,
	edge_limit_level_10 = 10,
};

enum {
	single_tap,
	double_tap,
};

struct tpvendor_t {
	int vendor_id;
	char *vendor_name;
};

struct tp_point_log {
	unsigned int x;
	unsigned int y;
};

#ifdef CONFIG_TP_DETECT_BY_LCDINFO
struct tp_ic_vendor_info {
	u8 tp_chip_id;
	char *tp_ic_vendor_name;
};
#endif

/*chip_model_id synaptics 1,atmel 2,cypress 3,focal 4,goodix 5,mefals 6,mstar
7,himax 8;
 *
 */
struct tpd_tpinfo_t {
	unsigned int chip_model_id;
	unsigned int chip_part_id;
	unsigned int chip_ver;
	unsigned int module_id;
	unsigned int firmware_ver;
	unsigned int config_ver;
	unsigned int display_ver;
	unsigned int i2c_addr;
	unsigned int i2c_type;
	char tp_name[MAX_VENDOR_NAME_LEN];
	char vendor_name[MAX_VENDOR_NAME_LEN];
	char chip_batch[MAX_VENDOR_NAME_LEN];
};

struct ts_firmware {
	u8 *data;
	int size;
};

struct tpd_classdev_t {
	const char *name;
	int b_force_upgrade;
	int fw_compare_result;
	int b_gesture_enable;
	int b_smart_cover_enable;
	int b_glove_enable;
	int display_rotation;
	bool TP_have_registered;
	bool tp_suspend;
	bool sys_set_tp_suspend_flag;
	bool headset_state;
	bool lcd_reset_processing;
	bool tp_suspend_write_gesture;
	int reg_char_num;
	u8 edge_report_limit[MAX_LIMIT_NOM];
	u16 user_edge_limit[MAX_LIMIT_NOM];
	u8 long_pess_suppression[MAX_LIMIT_NOM];
	u8 edge_limit_level;
	u8 edge_limit_pixel_level;
	u16 long_press_max_count;
	bool edge_long_press_check;
	u16 max_x;
	u16 max_y;
#ifdef CONFIG_TP_DETECT_BY_LCDINFO
	u8 tp_chip_id;
#endif
	int b_single_tap_enable;
	int screen_is_on;
	int one_key_enable;
	int play_game_enable;
	int (*tp_i2c_reg_read)(struct tpd_classdev_t *cdev, char addr, u8 *data, int len);
	int (*tp_i2c_reg_write)(struct tpd_classdev_t *cdev, char addr, u8 *data, int len);
	int (*tp_i2c_16bor32b_reg_read)(struct tpd_classdev_t *cdev, u32 addr, u8 *data, int len);
	int (*tp_i2c_16bor32b_reg_write)(struct tpd_classdev_t *cdev, u32 addr, u8 *data, int len);
	int (*tp_fw_upgrade)(struct tpd_classdev_t *cdev, char *fwname, int fwname_len);
	int (*read_block)(struct tpd_classdev_t *cdev, u16 addr, u8 *buf, int len);
	int (*write_block)(struct tpd_classdev_t *cdev, u16 addr, u8 *buf, int len);
	int (*get_tpinfo)(struct tpd_classdev_t *cdev);
	int (*get_gesture)(struct tpd_classdev_t *cdev);
	int (*wake_gesture)(struct tpd_classdev_t *cdev, int enable);
	int (*get_smart_cover)(struct tpd_classdev_t *cdev);
	int (*set_smart_cover)(struct tpd_classdev_t *cdev, int enable);
	int (*get_glove_mode)(struct tpd_classdev_t *cdev);
	int (*set_glove_mode)(struct tpd_classdev_t *cdev, int enable);
	int (*tp_suspend_show)(struct tpd_classdev_t *cdev);
	int (*set_tp_suspend)(struct tpd_classdev_t *cdev, u8 suspend_node, int enable);
	bool (*tpd_suspend_need_awake)(struct tpd_classdev_t *cdev);
	int (*set_headset_state)(struct tpd_classdev_t *cdev, int enable);
	int (*headset_state_show)(struct tpd_classdev_t *cdev);
	int (*set_display_rotation)(struct tpd_classdev_t *cdev, int mrotation);
	int (*set_edge_limit_level)(struct tpd_classdev_t *cdev, u8 level);
	bool (*tpd_esd_check)(struct tpd_classdev_t *cdev);
	void (*tpd_report_uevent)(u8 gesture_key);
	int (*tp_hw_reset)(void);
	void (*tp_reset_gpio_output)(bool value);
	int (*set_tp_state)(struct tpd_classdev_t *cdev, int enable);
	int (*get_singletap)(struct tpd_classdev_t *cdev);
	int (*set_singletap)(struct tpd_classdev_t *cdev, int enable);
	int (*get_noise)(struct tpd_classdev_t *cdev, struct list_head *head);
#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
	int (*get_screen_state)(struct tpd_classdev_t *cdev);
	int (*set_screen_state)(struct tpd_classdev_t *cdev, int enable);
#endif
	int (*get_one_key)(struct tpd_classdev_t *cdev);
	int (*set_one_key)(struct tpd_classdev_t *cdev, int enable);
	int (*get_play_game)(struct tpd_classdev_t *cdev);
	int (*set_play_game)(struct tpd_classdev_t *cdev, int enable);
	struct platform_device *platform_device;
	struct workqueue_struct *tpd_report_wq;
	struct delayed_work tpd_report_work0;
	struct delayed_work tpd_report_work1;
	struct delayed_work tpd_report_work2;
	struct delayed_work tpd_report_work3;
	struct delayed_work tpd_report_work4;
	struct delayed_work tpd_report_work5;
	struct delayed_work tpd_report_work6;
	struct delayed_work tpd_report_work7;
	struct delayed_work tpd_report_work8;
	struct delayed_work tpd_report_work9;
#ifdef CONFIG_TPD_UFP_MAC
	struct delayed_work tpd_report_lcd_state_work;
#endif
	void *private;
	void *test_node;    /*added for tp test.*/

	/**for tpd test*/
	int (*tpd_test_set_save_filepath)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_save_filepath)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_set_save_filename)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_save_filename)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_set_ini_filepath)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_ini_filepath)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_set_filename)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_filename)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_set_cmd)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_cmd)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_set_node_data_type)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_node_data)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_get_channel_info)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_get_result)(struct tpd_classdev_t *cdev, char *buf);
	int (*tpd_test_get_tp_rawdata)(char *buffer, int length);
	int (*tpd_gpio_shutdown)(void);
#ifdef CONFIG_TP_BSC_CALIBRATION
	int (*tpd_test_set_bsc_calibration)(struct tpd_classdev_t *cdev, const char *buf);
	int (*tpd_test_get_bsc_calibration)(struct tpd_classdev_t *cdev, char *buf);
#endif

	struct mutex cmd_mutex;
	struct tpd_tpinfo_t ic_tpinfo;
	struct device		*dev;
	struct list_head	 node;
};

extern struct tpd_classdev_t tpd_fw_cdev;
extern int tpd_classdev_register(struct device *parent, struct tpd_classdev_t
*tsp_fw_cdev);

#ifdef CONFIG_ZTE_LCD_COMMON_FUNCTION
extern const char *zte_get_lcd_panel_name(void);
#endif
extern void set_lcd_reset_processing(bool enable);
extern const char *get_lcd_panel_name(void);
#ifdef CONFIG_TP_DETECT_BY_LCDINFO
int get_tp_chip_id(void);
#endif
int tpd_uevent_init(void);
void tpd_uevent_exit(void);
void tpd_touch_press(struct input_dev *input, u16 x, u16 y, u16 id, u8 touch_major, u8  pressure);
void tpd_touch_release(struct input_dev *input, u16 id);
void tpd_clean_all_event(void);
void tpd_workquue_init(void);
void tpd_workquue_deinit(void);
#endif	/* __TSP_FW_CLASS_H_INCLUDED */

