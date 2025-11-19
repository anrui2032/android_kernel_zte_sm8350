#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/module.h>
#include "tpd_ufp_mac.h"
#include "tpd_sys.h"

#define SINGLE_TAP_DELAY	600

#if defined(ZTE_ONE_KEY)  || defined(POINT_SIMULAT_UF)
#define ZTE_CIRCLE_CENTER_X 540
#define ZTE_CIRCLE_CENTER_Y 2163
#define ZTE_CIRCLE_RADIUS 108
#endif

#ifdef ZTE_ONE_KEY
#define MAX_POINTS_SUPPORT 10
#define FP_GESTURE_DOWN	"fp_gesture_down=true"
#define FP_GESTURE_UP	"fp_gesture_up=true"

static char *one_key_finger_id[] = {
	"finger_id=0",
	"finger_id=1",
	"finger_id=2",
	"finger_id=3",
	"finger_id=4",
	"finger_id=5",
	"finger_id=6",
	"finger_id=7",
	"finger_id=8",
	"finger_id=9",
};
#endif

static char *tppower_to_str[] = {
	"TP_POWER_STATUS=2",		/* TP_POWER_ON */
	"TP_POWER_STATUS=1",		/* TP_POWER_OFF */
	"TP_POWER_STATUS=3",		/* TP_POWER_AOD */
#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
	"TP_POWER_STATUS=4",		/* TP_POWER_TEMP */
#endif
};

static char *lcdstate_to_str[] = {
	"screen_on",
	"screen_off",
	"screen_in_doze",
#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
	"screen_on_temp ",
#endif
};

static char *lcdchange_to_str[] = {
	"lcd_exit_lp",
	"lcd_enter_lp",
	"lcd_on",
	"lcd_off",
};

DEFINE_MUTEX(ufp_mac_mutex);

struct ufp_ops ufp_tp_ops;

static atomic_t current_lcd_state = ATOMIC_INIT(SCREEN_ON);

int ufp_get_lcdstate(void)
{
	return atomic_read(&current_lcd_state);
}

static void ufp_single_tap_work(struct work_struct *work)
{
	atomic_set(&ufp_tp_ops.ato_is_single_tap, 0);
}

void ufp_report_gesture_uevent(char *str)
{
	char *envp[2];

	envp[0] = str;
	envp[1] = NULL;
	kobject_uevent_env(&(ufp_tp_ops.uevent_pdev->dev.kobj), KOBJ_CHANGE, envp);

	if (!strcmp(str, SINGLE_TAP_GESTURE)) {
		atomic_set(&ufp_tp_ops.ato_is_single_tap, 1);
		mod_delayed_work(ufp_tp_ops.single_tap_workqueue,
				&ufp_tp_ops.single_tap_work, msecs_to_jiffies(SINGLE_TAP_DELAY));
	} else if (!strcmp(str, DOUBLE_TAP_GESTURE))
		mod_delayed_work(ufp_tp_ops.single_tap_workqueue,
						&ufp_tp_ops.single_tap_work, 0);
	UFP_INFO("%s", str);
}

static inline void __report_ufp_uevent(char *str)
{
	char *envp[3];

	if (!ufp_tp_ops.uevent_pdev) {
		UFP_ERR("uevent pdev is null!\n");
		return;
	}

	if (!strcmp(str, AOD_AREAMEET_DOWN))
		ufp_report_gesture_uevent(SINGLE_TAP_GESTURE);

	envp[0] = str;
	envp[1] = tppower_to_str[atomic_read(&current_lcd_state)];
	envp[2] = NULL;
	kobject_uevent_env(&(ufp_tp_ops.uevent_pdev->dev.kobj), KOBJ_CHANGE, envp);
	UFP_INFO("%s", str);
}

void report_ufp_uevent(int enable)
{
	static int area_meet_down = 0;

	if (enable && !area_meet_down) {
		area_meet_down = 1;
		if (atomic_read(&current_lcd_state) == SCREEN_ON) {/* fp func enable is guaranted by user*/
			__report_ufp_uevent(AREAMEET_DOWN);
		 } else {
			__report_ufp_uevent(AOD_AREAMEET_DOWN);
		}
	} else if (!enable && area_meet_down) {
			area_meet_down = 0;
			__report_ufp_uevent(AREAMEET_UP);
	}
}

#if defined(ZTE_ONE_KEY)  || defined(POINT_SIMULAT_UF)
static inline int zte_in_zeon(int x, int y)
{
	int ret = 0;

	if ((ZTE_CIRCLE_CENTER_X - ZTE_CIRCLE_RADIUS < x) &&
		(ZTE_CIRCLE_CENTER_X + ZTE_CIRCLE_RADIUS > x) &&
		(ZTE_CIRCLE_CENTER_Y - ZTE_CIRCLE_RADIUS < y) &&
		(ZTE_CIRCLE_CENTER_Y + ZTE_CIRCLE_RADIUS > y)) {
			ret = 1;
	}

	return ret;
}
#endif

#ifdef ZTE_ONE_KEY
static inline void report_one_key_uevent(char *str, int i)
{
	char *envp[3];

	envp[0] = str;
	envp[1] = one_key_finger_id[i];
	envp[2] = NULL;
	kobject_uevent_env(&(ufp_tp_ops.uevent_pdev->dev.kobj), KOBJ_CHANGE, envp);
	UFP_INFO("%s", str);
}

/* We only track the first finger in zeon */
void one_key_report(int is_down, int x, int y, int finger_id)
{
	int retval;
	static char one_key_finger[MAX_POINTS_SUPPORT] = {0};
	static int one_key_down = 0;

	if (is_down) {
		retval = zte_in_zeon(x, y);
		if (retval && !one_key_finger[finger_id] && !one_key_down) {
			one_key_finger[finger_id] = 1;
			one_key_down = 1;
			report_one_key_uevent(FP_GESTURE_DOWN, finger_id);
		}
	} else if (one_key_finger[finger_id]) {
			one_key_finger[finger_id] = 0;
			one_key_down = 0;
			report_one_key_uevent(FP_GESTURE_UP, finger_id);
	}
}
#endif

#ifdef POINT_SIMULAT_UF
/* We only track the first finger in zeon */
void uf_touch_report(int x, int y, int finger_id)
{
	int retval;
	static int fp_finger[MAX_POINTS_SUPPORT] = { 0 };
	static int area_meet_down = 0;

	retval = zte_in_zeon(x, y);
	if (retval) {
		if (!fp_finger[finger_id] && !area_meet_down) {
			fp_finger[finger_id] = 1;
			area_meet_down = 1;
			__report_ufp_uevent(AREAMEET_DOWN);
		}
	} else if (fp_finger[finger_id]) {
			fp_finger[finger_id] = 0;
			area_meet_down = 0;
			__report_ufp_uevent(AREAMEET_UP);
	}
}
#endif

static inline void report_lcd_uevent(struct kobject *kobj, char **envp)
{
	int retval;

	envp[0] = "aod=true";
	envp[1] = NULL;
	retval = kobject_uevent_env(kobj, KOBJ_CHANGE, envp);
	if (retval != 0)
		UFP_ERR("lcd state uevent send failed!\n");
}

void ufp_report_lcd_state(void)
{
	char *envp[2];

	if (!ufp_tp_ops.uevent_pdev) {
		UFP_ERR("uevent pdev is null!\n");
		return;
	}

	report_lcd_uevent(&(ufp_tp_ops.uevent_pdev->dev.kobj), envp);
}
EXPORT_SYMBOL(ufp_report_lcd_state);

/*for lcd low power mode*/
int ufp_notifier_cb(int in_lp)
{
	int retval = 0;

	if (!ufp_tp_ops.tp_data) {
		UFP_ERR("tp driver is failed, exit!\n");
		return 0;
	}

	UFP_INFO("in lp %d!\n", in_lp);

	if (in_lp)
		change_tp_state(ENTER_LP);
	else
		change_tp_state(EXIT_LP);

	return retval;
}
EXPORT_SYMBOL(ufp_notifier_cb);

#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
int ufp_frame_notifier_cb(int is_light)
{
	if (!ufp_tp_ops.tp_data) {
		UFP_ERR("tp driver is failed, exit!\n");
		return 0;
	}

	if (atomic_read(&ufp_tp_ops.ato_is_single_tap) &&
		atomic_read(&current_lcd_state) == SCREEN_ON_TEMP
		&& is_light)
			change_tp_state(ON);

	atomic_set(&ufp_tp_ops.atoc_frame_is_light, is_light);

	return 0;
}
EXPORT_SYMBOL(ufp_frame_notifier_cb);
#endif

static inline void lcd_on_thing(void)
{
	queue_work(ufp_tp_ops.suspend_resume_workqueue,
				&(ufp_tp_ops.resume_work));

	atomic_set(&ufp_tp_ops.ato_is_single_tap, 0);
}

static inline void lcd_off_thing(void)
{
	atomic_set(&ufp_tp_ops.ato_is_single_tap, 0);

	queue_work(ufp_tp_ops.suspend_resume_workqueue,
				&(ufp_tp_ops.suspend_work));
}

static inline void lcd_doze_thing(void)
{
	atomic_set(&ufp_tp_ops.ato_is_single_tap, 0);

	queue_work(ufp_tp_ops.suspend_resume_workqueue,
				&(ufp_tp_ops.suspend_work));
}

#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
static void lcd_on_temp_thing(void)
{
	if (atomic_read(&ufp_tp_ops.atoc_frame_is_light)) {
		/* change_tp_state(ON); */
		atomic_set(&current_lcd_state, SCREEN_ON);
		lcd_on_thing();
	}
}
#endif

static void screen_on(lcdchange lcd_change)
{
	switch (lcd_change) {
	case ENTER_LP:
		atomic_set(&current_lcd_state, DOZE);
		lcd_doze_thing();
		break;
	case OFF:
		atomic_set(&current_lcd_state, SCREEN_OFF);
		lcd_off_thing();
		break;
	default:
		UFP_ERR("ignore err lcd change");
	}
}

static void screen_off(lcdchange lcd_change)
{
	switch (lcd_change) {
	case ON:
#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
		if (atomic_read(&ufp_tp_ops.ato_is_single_tap)) {
			atomic_set(&current_lcd_state, SCREEN_ON_TEMP);
			lcd_on_temp_thing();
		} else
#endif
		{
			atomic_set(&current_lcd_state, SCREEN_ON);
			lcd_on_thing();
		}
		break;
	case ENTER_LP:
		if (!atomic_read(&ufp_tp_ops.ato_is_single_tap))
			lcd_on_thing();

		atomic_set(&current_lcd_state, DOZE);
		lcd_doze_thing();
		break;
	case OFF:
		atomic_set(&current_lcd_state, SCREEN_OFF);
		lcd_off_thing();
		UFP_ERR("err lcd off change");
		break;
	default:
		UFP_ERR("ignore err lcd change");
	}
}

static void doze(lcdchange lcd_change)
{
	switch (lcd_change) {
	case EXIT_LP:
		atomic_set(&current_lcd_state, SCREEN_ON);
		lcd_on_thing();
		break;
	case OFF:
		atomic_set(&current_lcd_state, SCREEN_OFF);
		lcd_off_thing();
		break;
	case ON:
		atomic_set(&current_lcd_state, SCREEN_ON);
		lcd_on_thing();
		break;
	default:
		UFP_ERR("ignore err lcd change");
	}
}

#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
static void screen_on_temp(lcdchange lcd_change)
{
	switch (lcd_change) {
/*
	case ON:
		current_lcd_state = SCREEN_ON;
		lcd_on_thing();
		break;
*/
	case ENTER_LP:
		atomic_set(&current_lcd_state, DOZE);
		lcd_doze_thing();
		break;
	case OFF:
		atomic_set(&current_lcd_state, SCREEN_OFF);
		lcd_off_thing();
		UFP_ERR("err lcd off change");
		break;
	default:
		UFP_ERR("ignore err lcd change");
	}
}
#endif

void change_tp_state(lcdchange lcd_change)
{
	mutex_lock(&ufp_mac_mutex);

	UFP_INFO("current_lcd_state:%s, lcd change:%s\n",
			lcdstate_to_str[atomic_read(&current_lcd_state)],
							lcdchange_to_str[lcd_change]);
	switch (atomic_read(&current_lcd_state)) {
	case SCREEN_ON:
		screen_on(lcd_change);
		break;
	case SCREEN_OFF:
		screen_off(lcd_change);
		break;
	case DOZE:
		doze(lcd_change);
		break;
#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
	case SCREEN_ON_TEMP:
		screen_on_temp(lcd_change);
		break;
#endif
	default:
		atomic_set(&current_lcd_state, SCREEN_ON);
		lcd_on_thing();
		UFP_ERR("err lcd light change");
	}

	mutex_unlock(&ufp_mac_mutex);
}

static void ufp_resume_work(struct work_struct *work)
{
	if (ufp_tp_ops.tp_resume_func)
		ufp_tp_ops.tp_resume_func(ufp_tp_ops.tp_data);
}

static void ufp_suspend_work(struct work_struct *work)
{
	if (ufp_tp_ops.tp_suspend_func)
		ufp_tp_ops.tp_suspend_func(ufp_tp_ops.tp_data);
}

static int __init ufp_mac_init(void)
{
	ufp_tp_ops.single_tap_workqueue =
			create_singlethread_workqueue("single_tap_cancel");
	INIT_DELAYED_WORK(&ufp_tp_ops.single_tap_work, ufp_single_tap_work);

	ufp_tp_ops.suspend_resume_workqueue =
			create_singlethread_workqueue("ufp_resume_suspend");
	INIT_WORK(&ufp_tp_ops.resume_work, ufp_resume_work);
	INIT_WORK(&ufp_tp_ops.suspend_work, ufp_suspend_work);

	ufp_tp_ops.tp_data = NULL;
	ufp_tp_ops.tp_resume_func = NULL;
	ufp_tp_ops.tp_suspend_func = NULL;

	atomic_set(&ufp_tp_ops.ato_is_single_tap, 0);
#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
	atomic_set(&ufp_tp_ops.atoc_frame_is_light, 0);
#endif
	ufp_tp_ops.uevent_pdev = tpd_fw_cdev.platform_device;

	return 0;
}

static void __exit ufp_mac_exit(void)
{
	cancel_delayed_work_sync(&ufp_tp_ops.single_tap_work);
	flush_workqueue(ufp_tp_ops.single_tap_workqueue);
	destroy_workqueue(ufp_tp_ops.single_tap_workqueue);

	cancel_work_sync(&ufp_tp_ops.resume_work);
	cancel_work_sync(&ufp_tp_ops.suspend_work);
	flush_workqueue(ufp_tp_ops.suspend_resume_workqueue);
	destroy_workqueue(ufp_tp_ops.suspend_resume_workqueue);

	ufp_tp_ops.uevent_pdev = NULL;
}

module_init(ufp_mac_init);
module_exit(ufp_mac_exit);

MODULE_AUTHOR("zte");
MODULE_DESCRIPTION("under fingerprint machine");
MODULE_LICENSE("GPL");
