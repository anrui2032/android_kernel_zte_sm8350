#include "goodix_ts_core.h"
#include "../tpd_ufp_mac.h"
#include "../tpd_sys.h"

static atomic_t ato_ver = ATOMIC_INIT(0);
#define GOODIX_PLAYGAME_START	7
#define GOODIX_PLAYGAME_END		8

static int tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	int firmware;
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	struct i2c_client *i2c = to_i2c_client(ts_dev->dev);
	struct goodix_ts_version chip_ver;
	int r;
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

	if (ts_dev->hw_ops->read_version) {
		r = ts_dev->hw_ops->read_version(ts_dev, &chip_ver);
		if (!r && chip_ver.valid) {
			snprintf(cdev->ic_tpinfo.tp_name, MAX_VENDOR_NAME_LEN, "GTX8_GT%s",
				chip_ver.pid);

			firmware = (unsigned int)chip_ver.vid[3] +
					((unsigned int)chip_ver.vid[2] << 8) +
					((unsigned int)chip_ver.vid[1] << 16);
				cdev->ic_tpinfo.firmware_ver = firmware;

			cdev->ic_tpinfo.chip_model_id = TS_CHIP_GOODIX;

			cdev->ic_tpinfo.module_id = chip_ver.sensor_id;

			cdev->ic_tpinfo.i2c_addr = i2c->addr;
		}
	} else {
		ts_err("%s: read_version failed!", __func__);
		goto exit;
	}

	if (ts_dev->hw_ops->read_config) {
		r = ts_dev->hw_ops->read_config(ts_dev, cfg_buf);
		if (r <= 0)
			goto exit;

		cdev->ic_tpinfo.config_ver = cfg_buf[0];
	}

	ts_info("%s: end!", __func__);

exit:
	kfree(cfg_buf);
	atomic_cmpxchg(&ato_ver, 1, 0);

	return r;
}

static int tpd_get_singletapgesture(struct tpd_classdev_t *cdev)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->b_single_tap_enable = core_data->zc.is_single_tap;

	return 0;
}

static int tpd_set_singletapgesture(struct tpd_classdev_t *cdev, int enable)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	core_data->zc.is_set_single_in_suspend = enable;
	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		core_data->zc.is_single_tap = enable;
	}

	return 0;

}

static int tpd_get_wakegesture(struct tpd_classdev_t *cdev)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->b_gesture_enable = core_data->zc.is_wakeup_gesture;

	return 0;
}

static int tpd_enable_wakegesture(struct tpd_classdev_t *cdev, int enable)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	core_data->zc.is_set_wakeup_in_suspend = enable;
	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		core_data->zc.is_wakeup_gesture = enable;
	}

	return 0;
}

static int tpd_set_one_key(struct tpd_classdev_t *cdev, int enable)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	core_data->zc.is_set_onekey_in_suspend = enable;
	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		core_data->zc.is_one_key = enable;
	}

	return 0;
}

static int tpd_get_one_key(struct tpd_classdev_t *cdev)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->one_key_enable = core_data->zc.is_one_key;

	return 0;
}

static int tpd_set_play_game(struct tpd_classdev_t *cdev, int enable)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int ret;

	if (atomic_read(&core_data->suspended)) {
		/* we can not play game in black screen */
		ts_err("%s: error, change set in suspend!", __func__);
	} else {
		core_data->zc.is_play_game = enable;
		if (enable)
			/* We borrow ready-made interfaces here */
			ret = __goodix_set_edge_suppress(
				&(ts_bdata->edge_suppress_cmd[GOODIX_CMD_LEN * GOODIX_PLAYGAME_START]),
				core_data);
		else
			ret = __goodix_set_edge_suppress(
				&(ts_bdata->edge_suppress_cmd[GOODIX_CMD_LEN * GOODIX_PLAYGAME_END]),
				core_data);

		if (!ret)
			ts_info("%s: play_game %d success\n", __func__, enable);
		else
			ts_err("%s: play_game %d failed!\n", __func__, enable);
	}

	return 0;
}

static int tpd_get_play_game(struct tpd_classdev_t *cdev)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->play_game_enable = core_data->zc.is_play_game;

	return 0;
}

static int tpd_get_smart_cover(struct tpd_classdev_t *cdev)
{
	int retval = 0;
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;

	cdev->b_smart_cover_enable = core_data->zc.is_smart_cover;

	return retval;
}

static int tpd_set_smart_cover(struct tpd_classdev_t *cdev, int enable)
{
	int retval = 0;

	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	core_data->zc.is_smart_cover = enable;
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

static int tpd_set_display_rotation(struct tpd_classdev_t *cdev, int mrotation)
{
	int ret = -1;
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);

	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
		return 0;
	}
	cdev->display_rotation = mrotation;
	ts_info("%s: display_rotation = %d.\n", __func__, cdev->display_rotation);
	switch (cdev->display_rotation) {
		case mRotatin_0:
			ret = __goodix_set_edge_suppress(
					&(ts_bdata->edge_suppress_cmd[GOODIX_CMD_LEN * GOODIX_MAX_EDGE_LEVEL]),
					core_data);
			break;
		case mRotatin_90:
			ret = __goodix_set_edge_suppress(
				&(ts_bdata->edge_suppress_cmd[GOODIX_CMD_LEN * GOODIX_HORIZONTAL_LEFT]),
				core_data);
			break;
		case mRotatin_180:
			ret = __goodix_set_edge_suppress(
					&(ts_bdata->edge_suppress_cmd[GOODIX_CMD_LEN * GOODIX_MAX_EDGE_LEVEL]),
					core_data);
			break;
		case mRotatin_270:
			ret = __goodix_set_edge_suppress(
				&(ts_bdata->edge_suppress_cmd[GOODIX_CMD_LEN * GOODIX_HORIZONTAL_RIGHT]),
				core_data);
			break;
		default:
			break;
	}
	if (ret) {
		ts_err("Write display rotation failed!\n");
	}
	return cdev->display_rotation;
}

static int tpd_set_edge_limit_level(struct tpd_classdev_t *cdev, u8 level)
{
	int ret = -1;
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)cdev->private;
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);

	if (atomic_read(&core_data->suspended)) {
		ts_err("%s: error, change set in suspend!", __func__);
		return 0;
	}

	if (level > GOODIX_MAX_EDGE_LEVEL)
		level = GOODIX_MAX_EDGE_LEVEL;
	core_data->zc.edge_limit_level = level;
	ret = __goodix_set_edge_suppress(&(ts_bdata->edge_suppress_cmd[GOODIX_CMD_LEN * level]), core_data);
	if (ret) {
		ts_err("Write edge suppress failed!\n");
	}
	return level;
}

void goodix_tpd_register_fw_class(struct goodix_ts_core *core_data)
{
#ifdef CTS_REPORT_BY_ZTE_ALGO
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
#endif

	ts_info("%s: entry\n", __func__);

	tpd_fw_cdev.private = (void *)core_data;
	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo;

	tpd_fw_cdev.get_gesture = tpd_get_wakegesture;
	tpd_fw_cdev.wake_gesture = tpd_enable_wakegesture;

	tpd_fw_cdev.get_singletap = tpd_get_singletapgesture;
	tpd_fw_cdev.set_singletap = tpd_set_singletapgesture;

	tpd_fw_cdev.get_smart_cover = tpd_get_smart_cover;
	tpd_fw_cdev.set_smart_cover = tpd_set_smart_cover;

	tpd_fw_cdev.set_one_key = tpd_set_one_key;
	tpd_fw_cdev.get_one_key = tpd_get_one_key;

	tpd_fw_cdev.get_play_game = tpd_get_play_game;
	tpd_fw_cdev.set_play_game = tpd_set_play_game;
	
	tpd_fw_cdev.set_edge_limit_level = tpd_set_edge_limit_level;
	tpd_fw_cdev.set_display_rotation = tpd_set_display_rotation;
	core_data->zc.edge_limit_level = DEFAULT_SUPPRESS_LEVEL;
#ifdef CTS_REPORT_BY_ZTE_ALGO
	tpd_fw_cdev.max_x = ts_bdata->panel_max_x;
	tpd_fw_cdev.max_y = ts_bdata->panel_max_y;
#endif
	ts_info("%s: end\n", __func__);
}
