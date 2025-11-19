#ifndef __UFP_MAC_H
#define __UFP_MAC_H

/*define POINT_SIMULAT_UF*/
#define ZTE_ONE_KEY

/* zte: add for fp uevent report */
#define SINGLE_TAP_GESTURE "single_tap=true"
#define DOUBLE_TAP_GESTURE "double_tap=true"
#define AOD_AREAMEET_DOWN "aod_areameet_down=true"
#define AREAMEET_DOWN "areameet_down=true"
#define AREAMEET_UP "areameet_up=true"

#define UFP_FP_DOWN 1
#define UFP_FP_UP 0

typedef enum lcdstate {
	SCREEN_ON = 0,
	SCREEN_OFF,
	DOZE,
	SCREEN_ON_TEMP,
} lcdstate;

typedef enum lcdchange {
	EXIT_LP = 0,
	ENTER_LP,
	ON,
	OFF,
} lcdchange;

struct ufp_ops {
	void *tp_data;
	struct platform_device *uevent_pdev;
	int (*tp_resume_func)(void *data);
	int (*tp_suspend_func)(void *data);
	atomic_t ato_is_single_tap;
#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
	atomic_t atoc_frame_is_light;
#endif
	struct delayed_work single_tap_work;
	struct workqueue_struct *single_tap_workqueue;
	struct work_struct suspend_work;
	struct work_struct resume_work;
	struct workqueue_struct *suspend_resume_workqueue;
};

/* Log define */
#define UFP_INFO(fmt, arg...)	pr_info("ufp_info: "fmt"\n", ##arg)
#define UFP_ERR(fmt, arg...)	pr_err("ufp_err: "fmt"\n", ##arg)

extern struct ufp_ops ufp_tp_ops;

#ifdef POINT_SIMULAT_UF
void uf_touch_report(int x, int y, int finger_id);
#endif
#ifdef ZTE_ONE_KEY
void one_key_report(int is_down, int x, int y, int finger_id);
#endif
void change_tp_state(lcdchange lcd_change);
int ufp_get_lcdstate(void);
void ufp_report_gesture_uevent(char *str);
void report_ufp_uevent(int enable);

#endif /* __UFP_MAC_H */
