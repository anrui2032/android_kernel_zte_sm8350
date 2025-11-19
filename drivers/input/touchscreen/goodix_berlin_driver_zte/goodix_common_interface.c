#include "goodix_ts_core.h"
#include "tpd_ufp_mac.h"
#include "tpd_sys.h"
#include <linux/i2c.h>

#ifdef GOODIX_USB_DETECT_GLOBAL
#include <linux/power_supply.h>
#endif

#ifdef GOODIX_USB_DETECT_GLOBAL
extern bool USB_detect_flag;
#endif

static atomic_t ato_ver = ATOMIC_INIT(0);
#define GOODIX_PLAYGAME_START	7
#define GOODIX_PLAYGAME_END		8

static int tpd_init_tpinfo_gt9897(struct tpd_classdev_t *cdev)
{
	int firmware;
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct i2c_client *i2c = container_of(core_data->bus->dev, struct i2c_client, dev);
	struct goodix_fw_version chip_ver;
	int r = 0;
	char *cfg_buf;

	if (ufp_get_lcdstate() != SCREEN_ON)
		return -EIO;

	if (atomic_cmpxchg(&ato_ver, 0, 1)) {
		ts_err("busy, wait!");
		return -EIO;
	}

	cfg_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!cfg_buf)
		return -ENOMEM;

	ts_info("%s: enter!", __func__);

	if (hw_ops->read_version) {
		r = hw_ops->read_version(core_data, &chip_ver);
		if (!r) {
			snprintf(cdev->ic_tpinfo.tp_name, MAX_VENDOR_NAME_LEN, "GT%s",
				chip_ver.patch_pid);

			firmware = (unsigned int)chip_ver.patch_vid[3] +
					((unsigned int)chip_ver.patch_vid[2] << 8) +
					((unsigned int)chip_ver.patch_vid[1] << 16) +
					((unsigned int)chip_ver.patch_vid[0] << 24);
			cdev->ic_tpinfo.firmware_ver = firmware;

			cdev->ic_tpinfo.chip_model_id = TS_CHIP_GOODIX;

			cdev->ic_tpinfo.module_id = chip_ver.sensor_id;

			cdev->ic_tpinfo.i2c_addr = i2c->addr;
		}
	} else {
		ts_err("%s: read_version failed!", __func__);
		goto exit;
	}

	if (hw_ops->read_config) {
		r = hw_ops->read_config(core_data, cfg_buf, PAGE_SIZE);
		if (r <= 0)
			goto exit;

		cdev->ic_tpinfo.config_ver = cfg_buf[34];
	}

	ts_info("%s: end!", __func__);

exit:
	kfree(cfg_buf);
	atomic_cmpxchg(&ato_ver, 1, 0);

	return r;
}

static int tpd_get_singleaodgesture(struct tpd_classdev_t *cdev)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->b_single_aod_enable = core_data->ztec.is_single_aod;

	return 0;
}

static int tpd_set_singleaodgesture(struct tpd_classdev_t *cdev, int enable)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	core_data->ztec.is_single_aod = enable;
	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		core_data->ztec.is_single_aod = enable;
		core_data->ztec.is_single_tap = (core_data->ztec.is_single_aod || core_data->ztec.is_single_fp) ? 5 : 0;
	}

	return 0;
}

static int tpd_get_singlefpgesture(struct tpd_classdev_t *cdev)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->b_single_tap_enable = core_data->ztec.is_single_fp;

	return 0;
}

static int tpd_set_singlefpgesture(struct tpd_classdev_t *cdev, int enable)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	core_data->ztec.is_single_fp = enable;
	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		core_data->ztec.is_single_fp = enable;
		core_data->ztec.is_single_tap = (core_data->ztec.is_single_aod || core_data->ztec.is_single_fp) ? 5 : 0;
	}

	return 0;
}

static int tpd_get_wakegesture(struct tpd_classdev_t *cdev)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->b_gesture_enable = core_data->ztec.is_wakeup_gesture;

	return 0;
}

static int tpd_enable_wakegesture(struct tpd_classdev_t *cdev, int enable)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	core_data->ztec.is_set_wakeup_in_suspend = enable;
	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		core_data->ztec.is_wakeup_gesture = enable;
	}

	return 0;
}

static int tpd_set_one_key(struct tpd_classdev_t *cdev, int enable)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	core_data->ztec.is_set_onekey_in_suspend = enable;
	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		core_data->ztec.is_one_key = enable;
	}

	return 0;
}

static int tpd_get_one_key(struct tpd_classdev_t *cdev)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->one_key_enable = core_data->ztec.is_one_key;

	return 0;
}
#if 0
static int tpd_get_smart_cover(struct tpd_classdev_t *cdev)
{
	int retval = 0;
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->b_smart_cover_enable = core_data->ztec.is_smart_cover;

	return retval;
}

static int tpd_set_smart_cover(struct tpd_classdev_t *cdev, int enable)
{
	int retval = 0;

	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	core_data->ztec.is_smart_cover = enable;
	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		if (enable) {
			/* send highsense_cfg to firmware */
			retval = ts_dev->hw_ops->send_config(ts_dev, &(ts_dev->highsense_cfg));
			if (retval < 0) {
				ts_info("failed send highsense config[ignore]");
			}
		} else {
			/* send normal-cfg to firmware */
			retval = ts_dev->hw_ops->send_config(ts_dev, &(ts_dev->normal_cfg));
			if (retval < 0) {
				ts_info("failed send normal config[ignore]");
			}
		}
	}

	return retval;
}

static int tpd_get_noise(struct tpd_classdev_t *cdev, struct list_head *head)
{
	int retval;
	int i = 0;
	char *buf_arry[RT_DATA_NUM];
	struct tp_runtime_data *tp_rt;
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	list_for_each_entry(tp_rt, head, list) {
		buf_arry[i++] = tp_rt->rt_data;
		tp_rt->is_empty = false;
	}

	mutex_lock(&(core_data->ztec.rawdata_read_lock));
	retval = get_goodix_ts_rawdata(core_data, buf_arry, RT_DATA_NUM, RT_DATA_LEN);
	if (retval < 0) {
		pr_err("%s: get_raw_noise failed!\n", __func__);
	}
	mutex_unlock(&(core_data->ztec.rawdata_read_lock));

	return retval;
}
#endif

#if defined(ZTE_FINGERAREA_A30PRO)
static int tpd_set_play_game(struct tpd_classdev_t *cdev, int enable)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;
	const struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;

	if (atomic_read(&core_data->suspended)) {
		/* we can not play game in black screen */
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		core_data->ztec.is_play_game = enable;
		if (enable) {
			ret = hw_ops->set_enter_play_game(core_data);
			ts_info("enter_play_game success\n");
		} else {
			ret = hw_ops->set_leave_play_game(core_data);
			ts_info("leave_play_game success!\n");
		}

		/*if (!ret)
			ts_info("%s: play_game %d success\n", __func__, enable);
		else
			ts_err("%s: play_game %d failed!\n", __func__, enable);*/
	}

	return 0;
}

static int tpd_get_play_game(struct tpd_classdev_t *cdev)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->play_game_enable = core_data->ztec.is_play_game;

	return 0;
}
#endif

static int tpd_get_sensibility(struct tpd_classdev_t *cdev)
{
	int retval = 0;
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->sensibility_enable = core_data->ztec.is_sensibility;

	return retval;
}

static int tpd_set_sensibility(struct tpd_classdev_t *cdev, int enable)
{
	int retval = 0;
	struct goodix_ic_config *config;

	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;
	/*struct goodix_ts_device *ts_dev = core_data->ts_dev;*/

	core_data->ztec.is_sensibility = enable;
	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		if (enable == 3) {
			/* send highsense_cfg to firmware */
				config = core_data->ic_configs[CONFIG_TYPE_HIGHSENSE];
				if (!config || config->len == 0) {
					ts_err("config %d is not exist", CONFIG_TYPE_HIGHSENSE);
					return 0;
				}
			retval = core_data->hw_ops->send_config(core_data, config->data, config->len);
			if (retval < 0) {
				ts_info("failed send highsense config[ignore]");
			}
		} else if (enable == 1) {
			/* send normal-cfg to firmware */
			config = core_data->ic_configs[CONFIG_TYPE_NORMAL];
				if (!config || config->len == 0) {
					ts_err("config %d is not exist", CONFIG_TYPE_NORMAL);
					return 0;
				}
			retval = core_data->hw_ops->send_config(core_data, config->data, config->len);
			if (retval < 0) {
				ts_info("failed send normal config[ignore]");
			}
		} else {
			ts_err("err sensibility data  %d.\n", enable);
		}

	}

	return retval;
}

#ifdef GOODIX_USB_DETECT_GLOBAL
static bool goodix_get_charger_status(void)
{
	static struct power_supply *batt_psy;
	union power_supply_propval val = { 0, };
	bool status = false;

	if (batt_psy == NULL)
		batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_STATUS, &val);
	}
	if ((val.intval == POWER_SUPPLY_STATUS_CHARGING) ||
		(val.intval == POWER_SUPPLY_STATUS_FULL)) {
		status = true;
	} else {
		status = false;
	}
	ts_info("charger status:%d", status);
	return status;
}

static void goodix_work_charger_detect_work(struct work_struct *work)
{
	int ret = -EINVAL;
	struct delayed_work *charger_work_delay = container_of(work, struct delayed_work, work);
	struct goodix_ts_core *core_data = container_of(charger_work_delay, struct goodix_ts_core, charger_work);
	const struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	static int status = 0;

	ts_info("into charger detect");
	USB_detect_flag = goodix_get_charger_status();
	if (USB_detect_flag && !atomic_read(&core_data->suspended) && !status) {
		ret = hw_ops->set_enter_charger(core_data);
		status = 1;
	} else if (!USB_detect_flag && !atomic_read(&core_data->suspended) && status) {
		ret = hw_ops->set_leave_charger(core_data);
		status = 0;
	} else if (!USB_detect_flag && atomic_read(&core_data->suspended) && status) {
		status = 0;
	} else if (USB_detect_flag && atomic_read(&core_data->suspended) && !status) {
		status = 1;
	}

}

static int goodix_charger_notify_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct goodix_ts_core *core_data = container_of(nb, struct goodix_ts_core, charger_notifier);

	/*ts_info("into charger notify");*/
	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if ((strcmp(psy->desc->name, "usb") == 0)
	    || (strcmp(psy->desc->name, "ac") == 0)) {
		if (delayed_work_pending(&core_data->charger_work)) {
			return NOTIFY_DONE;
		}
		queue_delayed_work(core_data->charger_wq, &core_data->charger_work, msecs_to_jiffies(500));
	}

	return NOTIFY_DONE;
}

static int goodix_init_charger_notifier(struct goodix_ts_core *core_data)
{
	int ret = 0;

	ts_info("Init Charger notifier");

	core_data->charger_notifier.notifier_call = goodix_charger_notify_call;
	ret = power_supply_reg_notifier(&core_data->charger_notifier);
	return ret;
}

#endif

void goodix_tpd_register_fw_class_gt9897(struct goodix_ts_core *core_data)
{
	ts_info("%s: entry\n", __func__);

#ifdef GOODIX_USB_DETECT_GLOBAL
	core_data->charger_wq = create_singlethread_workqueue("GOODIX_charger_detect");
	if (!core_data->charger_wq) {
		ts_err(" allocate charger_wq failed\n");
	} else  {
		USB_detect_flag = goodix_get_charger_status();
		INIT_DELAYED_WORK(&core_data->charger_work, goodix_work_charger_detect_work);
		goodix_init_charger_notifier(core_data);
	}
#endif

	tpd_fw_cdev.private = (void *)core_data;
	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo_gt9897;

	tpd_fw_cdev.get_gesture = tpd_get_wakegesture;
	tpd_fw_cdev.wake_gesture = tpd_enable_wakegesture;

	tpd_fw_cdev.get_singletap = tpd_get_singlefpgesture;
	tpd_fw_cdev.set_singletap = tpd_set_singlefpgesture;

	tpd_fw_cdev.get_singleaod = tpd_get_singleaodgesture;
	tpd_fw_cdev.set_singleaod = tpd_set_singleaodgesture;

/*	tpd_fw_cdev.get_smart_cover = tpd_get_smart_cover;
	tpd_fw_cdev.set_smart_cover = tpd_set_smart_cover;*/

	tpd_fw_cdev.set_one_key = tpd_set_one_key;
	tpd_fw_cdev.get_one_key = tpd_get_one_key;

	/*tpd_fw_cdev.get_noise = tpd_get_noise;*/

#if defined(ZTE_FINGERAREA_A30PRO)
	tpd_fw_cdev.get_play_game = tpd_get_play_game;
	tpd_fw_cdev.set_play_game = tpd_set_play_game;
#endif

	tpd_fw_cdev.get_sensibility = tpd_get_sensibility;
	tpd_fw_cdev.set_sensibility = tpd_set_sensibility;
	ts_info("%s: end\n", __func__);
}
