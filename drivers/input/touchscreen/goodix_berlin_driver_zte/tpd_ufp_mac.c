#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/notifier.h>
#include "tpd_ufp_mac.h"

#define SINGLE_TAP_DELAY	600

#if defined(ZTE_FINGERAREA_A30)
#define ZTE_CIRCLE_CENTER_X 540
#define ZTE_CIRCLE_CENTER_Y 2068
#define ZTE_CIRCLE_RADIUS 108
#elif defined(ZTE_FINGERAREA_A30PRO)
#define ZTE_CIRCLE_CENTER_X 540
#define ZTE_CIRCLE_CENTER_Y 1691
#define ZTE_CIRCLE_RADIUS 108
#else
#define ZTE_CIRCLE_CENTER_X 540
#define ZTE_CIRCLE_CENTER_Y 2068
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
static struct drm_panel *active_panel;

struct notifier_block ufp_nb;

static atomic_t current_lcd_state = ATOMIC_INIT(SCREEN_ON);

extern int ufp_aod_notifier_register(struct notifier_block *nb);
extern int ufp_aod_notifier_unregister(struct notifier_block *nb);
#ifdef CONFIG_DRM_PANEL_NOTIFIER
static int __ufp_drm_register(struct device *dev)
{
	int retval = -1;
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(dev->of_node, "panel", NULL);
	if (count <= 0)
		return -ENODEV;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(dev->of_node, "panel", i);
		if (node != NULL)
			UFP_INFO("%s: node = %s", __func__, node->name);

		panel = of_drm_find_panel(node);
		if (!IS_ERR(panel)) {
			of_node_put(node);
			active_panel = panel;
			return 0;
		}

		if (PTR_ERR(panel) == -ENODEV) {
			UFP_INFO("%s: no device!", __func__);
			retval = -ENODEV;
		} else if (PTR_ERR(panel) == -EPROBE_DEFER) {
			UFP_INFO("%s: device has not been probed yet", __func__);
			retval = -EPROBE_DEFER;
		}

		of_node_put(node);
	}

	return retval;
}

void ufp_drm_register(notifier_fn_t ts_fb_notifier_callback)
{
	ufp_tp_ops.fb_notifier.notifier_call = ts_fb_notifier_callback;

	UFP_INFO("%s: start", __func__);

	active_panel = NULL;
	if (__ufp_drm_register(ufp_tp_ops.tp_dev) == 0) {
		if (drm_panel_notifier_register(active_panel, &ufp_tp_ops.fb_notifier) < 0) {
			UFP_INFO("%s: Failed to register FB notifier client", __func__);
		}
	}
}

int ufp_drm_unregister(void)
{
	if (active_panel)
		return drm_panel_notifier_unregister(active_panel, &ufp_tp_ops.fb_notifier);

	return 0;
}
#endif

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

void ufp_report_gesture_uevent_nubiya(struct input_dev *dev, char *str)
{
	if (!strcmp(str, SINGLE_TAP_GESTURE)) {
		input_report_key(dev, KEY_F9, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F9, 0);
		input_sync(dev);
		atomic_set(&ufp_tp_ops.ato_is_single_tap, 1);
		mod_delayed_work(ufp_tp_ops.single_tap_workqueue,
				&ufp_tp_ops.single_tap_work, msecs_to_jiffies(SINGLE_TAP_DELAY));
	} else if (!strcmp(str, DOUBLE_TAP_GESTURE)) {
		input_report_key(dev, KEY_F10, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F10, 0);
		input_sync(dev);
		mod_delayed_work(ufp_tp_ops.single_tap_workqueue,
						&ufp_tp_ops.single_tap_work, 0);
	}

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
		if (atomic_read(&current_lcd_state) == SCREEN_ON) {
			__report_ufp_uevent(AREAMEET_DOWN);
		 } else {
			__report_ufp_uevent(AOD_AREAMEET_DOWN);
		}
	} else if (!enable && area_meet_down) {
			area_meet_down = 0;
			__report_ufp_uevent(AREAMEET_UP);
	}
}

void report_ufp_uevent_nubiya(struct input_dev *dev, int enable)
{
	static int area_meet_down = 0;

	if (enable && !area_meet_down && atomic_read(&current_lcd_state) != SCREEN_ON) {
		area_meet_down = 1;
		input_report_key(dev, KEY_F11, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F11, 0);
		input_sync(dev);
		UFP_INFO("%s", AOD_AREAMEET_DOWN);
	} else if (!enable && area_meet_down) {
		area_meet_down = 0;
		input_report_key(dev, KEY_F12, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F12, 0);
		input_sync(dev);
		UFP_INFO("%s", AREAMEET_UP);
	}
}

static int create_uevent_dev(struct platform_device **pdev)
{
	int retval;

	if (*pdev) {
		UFP_ERR(" there already has a uevent dev");
		return 0;
	}

	*pdev = platform_device_alloc("zte_touch", -1);
	if (!(*pdev)) {
		UFP_ERR("Failed to allocate platform device\n");
		return -ENOMEM;
	}

	retval = platform_device_add(*pdev);
	if (retval < 0) {
		UFP_ERR("Failed to add platform device\n");
		platform_device_put(*pdev);
		*pdev = NULL;
		return retval;
	}

	return 0;
}

#if defined(ZTE_ONE_KEY)  || defined(CONFIG_POINT_SIMULAT_UF)
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

#ifdef CONFIG_POINT_SIMULAT_UF
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

static int ufp_aod_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	if (event) {
		ufp_notifier_cb(true);
		ufp_report_lcd_state();
	} else {
		ufp_notifier_cb(false);
	}

	return 0;
}

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
		/*if (!atomic_read(&ufp_tp_ops.ato_is_single_tap))
			lcd_on_thing();  for aod gesture success*/

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
		/* current_lcd_state = SCREEN_ON;
		lcd_on_thing();*/
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

static void tp_probe_work(struct work_struct *work)
{
	int r;
	tp_probe_struct *tpp = container_of(work, tp_probe_struct, delay_probe_work);

	r = tpp->ts_probe_func(tpp->dev);
	if (r)
		tpp->ts_release_func();

	kfree(tpp);
}

int tp_probe_on_queue(void *dev, ts_probe ts_probe_func, ts_release ts_release_func)
{
	tp_probe_struct *tpp = NULL;

	if (!dev || !ts_probe_func || !ts_release_func) {
		UFP_ERR("%s: dev or ts_probe_func is NULL!\n");
		return -ENOMEM;
	}

	tpp = kzalloc(sizeof(tp_probe_struct), GFP_KERNEL);
	if (!tpp) {
		UFP_ERR("%s: alloc failed!\n");
		return -ENOMEM;
	}

	tpp->dev = dev;
	tpp->ts_probe_func = ts_probe_func;
	tpp->ts_release_func = ts_release_func;
	INIT_WORK(&tpp->delay_probe_work, tp_probe_work);
	/* Let's borrow the resume queue */
	queue_work(ufp_tp_ops.suspend_resume_workqueue,
			&tpp->delay_probe_work);

	return 0;
}

int ufp_mac_init(void)
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
	ufp_tp_ops.uevent_pdev = NULL;
	create_uevent_dev(&(ufp_tp_ops.uevent_pdev));

	ufp_nb.notifier_call = ufp_aod_notifier_callback;
	ufp_aod_notifier_register(&ufp_nb);

	return 0;
}

void ufp_mac_exit(void)
{
	cancel_delayed_work_sync(&ufp_tp_ops.single_tap_work);
	flush_workqueue(ufp_tp_ops.single_tap_workqueue);
	destroy_workqueue(ufp_tp_ops.single_tap_workqueue);

	cancel_work_sync(&ufp_tp_ops.resume_work);
	cancel_work_sync(&ufp_tp_ops.suspend_work);
	flush_workqueue(ufp_tp_ops.suspend_resume_workqueue);
	destroy_workqueue(ufp_tp_ops.suspend_resume_workqueue);

	platform_device_unregister(ufp_tp_ops.uevent_pdev);
	ufp_aod_notifier_unregister(&ufp_nb);
	ufp_tp_ops.uevent_pdev = NULL;
}

MODULE_AUTHOR("zte");
MODULE_DESCRIPTION("under fingerprint machine");
MODULE_LICENSE("GPL");
