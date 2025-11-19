/************************************************************************
*
* File Name: ilitek_common_interface.c
*
*  *   Version: v1.0
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/

#include "ilitek.h"
#include <linux/kernel.h>
#include <linux/power_supply.h>
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

#define MAX_FILE_NAME_LEN       64
#define MAX_FILE_PATH_LEN  64
#define MAX_NAME_LEN_20  20
#define FW_TP_STATUS_BUFFER 9

int ilitek_vendor_id = 0;
int ilitek_tptest_result = 0;
char ilitek_vendor_name[MAX_NAME_LEN_20] = { 0 };
char ilitek_firmware_name[MAX_FILE_NAME_LEN] = {0};
char fw_filp_path[MAX_FILE_NAME_LEN] = { 0 };
char ini_filp_path[MAX_FILE_NAME_LEN] = { 0 };
char ini_rq_path[MAX_FILE_NAME_LEN] = { 0 };
char g_ilitek_save_file_path[MAX_FILE_PATH_LEN] = { 0 };

struct tpvendor_t ilitek_vendor_l[] = {
	{ILI_VENDOR_ID_0, ILI_VENDOR_0_NAME},
	{ILI_VENDOR_ID_1, ILI_VENDOR_1_NAME},
	{ILI_VENDOR_ID_2, ILI_VENDOR_2_NAME},
	{ILI_VENDOR_ID_3, ILI_VENDOR_2_NAME},
	{VENDOR_END, "Unknown"},
};

int ilitek_get_fw_by_lcminfo(void)
{
	int i = 0;
	int ret = 0;
	const char *panel_name = NULL;

	if (ILI_MODULE_NUM == 1) {
		ilitek_vendor_id = ILI_VENDOR_ID_0;
		strlcpy(ilitek_vendor_name, ILI_VENDOR_0_NAME, sizeof(ilitek_vendor_name));
		ret = 0;
		goto out;
	}

	panel_name = get_lcd_panel_name();
	for (i = 0; i < ARRAY_SIZE(ilitek_vendor_l); i++) {
		if (strnstr(panel_name, ilitek_vendor_l[i].vendor_name, strlen(panel_name))) {
			ilitek_vendor_id = ilitek_vendor_l[i].vendor_id;
			strlcpy(ilitek_vendor_name, ilitek_vendor_l[i].vendor_name,
				sizeof(ilitek_vendor_name));
			ret = 0;
			goto out;
		}
	}
	strlcpy(ilitek_vendor_name, "Unknown", sizeof(ilitek_vendor_name));
	ret = -EIO;
out:
	snprintf(ilitek_firmware_name, sizeof(ilitek_firmware_name),
			"ilitek_firmware_%s.hex", ilitek_vendor_name);
	snprintf(ini_rq_path, sizeof(ini_rq_path), "system/etc/ilitek_mp_%s.ini", ilitek_vendor_name);
	return ret;
}

void ilitek_update_module_info(void)
{
	idev->md_name = ilitek_vendor_name;
	idev->md_fw_rq_path = ilitek_firmware_name;
	idev->md_ini_rq_path = ini_rq_path;
	idev->md_save_file_path = 	CSV_LCM_ON_PATH;
	idev->md_fw_ili_size = 0;
	ipio_info("Found %s module\n", idev->md_name);
	idev->tp_module = ilitek_vendor_id;
}

static int tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	bool temp = false;

	/* Get current fw version. */
	temp = idev->info_from_hex;
	idev->info_from_hex = DISABLE;
	if (ilitek_tddi_ic_get_fw_ver() < 0)
		ipio_err("Get firmware ver failed before upgrade\n");
	idev->info_from_hex = temp;
	ipio_info("Firmware version = 0x%x.\n", idev->chip->fw_ver);
	ilitek_get_fw_by_lcminfo();
	ilitek_update_module_info();
	strlcpy(cdev->ic_tpinfo.tp_name, "ilitek", sizeof(cdev->ic_tpinfo.tp_name));
	strlcpy(cdev->ic_tpinfo.vendor_name, ilitek_vendor_name, sizeof(cdev->ic_tpinfo.vendor_name));
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_ILITEK;
	cdev->ic_tpinfo.firmware_ver =
		((idev->chip->fw_ver >> 8) & 0xffff0000) | (idev->chip->fw_ver & 0x0000ffff);
	cdev->ic_tpinfo.module_id = ilitek_vendor_id;
	cdev->ic_tpinfo.i2c_type = 0;
	cdev->ic_tpinfo.i2c_addr =  idev->i2c->addr;
	return 0;
}

static int ilitek_tp_fw_upgrade(struct tpd_classdev_t *cdev, char *fw_name, int fwname_len)
{
	char fwname[MAX_FILE_NAME_LEN] = { 0 };
	bool esd_en = idev->wq_esd_ctrl, bat_en = idev->wq_bat_ctrl;

	ipio_info("Preparing to upgarde firmware\n");
	if ((fwname_len <= 1) || (fwname_len >= MAX_FILE_NAME_LEN)) {
		ipio_err("fw hex name's length(%d) fail.\n", fwname_len);
		return -EINVAL;
	}
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, sizeof(fwname), "%s", fw_name);
	fwname[fwname_len - 1] = '\0';
	ipio_info("fwname is %s.\n", fwname);
	idev->md_fw_filp_path = kzalloc(MAX_FILE_NAME_LEN, GFP_KERNEL);
	if (idev->md_fw_filp_path == NULL) {
		ipio_err("md_fw_filp_path kzalloc fail.\n");
		return -ENOMEM;
	}
	snprintf(idev->md_fw_filp_path, MAX_FILE_NAME_LEN, "/sdcard/%s", fwname);
	ipio_info("md_fw_filp_path is %s.\n", idev->md_fw_filp_path);
	mutex_lock(&idev->touch_mutex);
	if (esd_en)
		ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);

	idev->force_fw_update = ENABLE;
	idev->node_update = true;
	idev->fw_open = FILP_OPEN;
	ilitek_tddi_fw_upgrade_handler(NULL);
	idev->force_fw_update = DISABLE;
	idev->node_update = false;
	idev->fw_open = REQUEST_FIRMWARE;
	kfree(idev->md_fw_filp_path);
	idev->md_fw_filp_path = NULL;

	if (esd_en)
		ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);

	mutex_unlock(&idev->touch_mutex);
	return 0;
}

void ilitek_ex_mode_recovery(void)
{
	if (idev->charger_mode) {
		if (ilitek_tddi_ic_func_ctrl("plug", ENABLE) < 0)
			ipio_err("Write plug out failed\n");
	}
	if (idev->headset_mode) {
		if (ilitek_tddi_ic_func_ctrl("headset_en", ENABLE) < 0)
			ipio_err("Write headset plug in failed\n");
	}
}

static int ilitek_headset_state_show(struct tpd_classdev_t *cdev)
{
	cdev->headset_state = idev->headset_mode;
	return cdev->headset_state;
}

static int ilitek_set_headset_state(struct tpd_classdev_t *cdev, int enable)
{
	idev->headset_mode = enable;
	ipio_info("%s: headset_state = %d.\n", __func__, idev->headset_mode);
	if (!idev->tp_suspend) {
		if (ilitek_tddi_ic_func_ctrl("headset_en", enable) < 0)	/* headset plug in/out */
			ipio_err("Write headset plug in failed\n");
	}
	return idev->headset_mode;
}


static int ilitek_set_display_rotation(struct tpd_classdev_t *cdev, int mrotation)
{
	cdev->display_rotation = mrotation;
	if (idev->tp_suspend)
		return 0;
	ipio_info("%s: display_rotation = %d.\n", __func__, cdev->display_rotation);
	switch (cdev->display_rotation) {
		case mRotatin_0:
			if (!idev->tp_suspend) {
				if (ilitek_tddi_ic_func_ctrl("edge_palm", 1) < 0)
					ipio_err("Write edge_palm failed\n");
			}
			break;
		case mRotatin_90:
			if (!idev->tp_suspend) {
				if (ilitek_tddi_ic_func_ctrl("edge_palm", 2) < 0)
					ipio_err("Write edge_palm failed\n");
			}
			break;
		case mRotatin_180:
			if (!idev->tp_suspend) {
				if (ilitek_tddi_ic_func_ctrl("edge_palm", 1) < 0)
				ipio_err("Write edge_palm failed\n");
			}
			break;
		case mRotatin_270:
			if (!idev->tp_suspend) {
				if (ilitek_tddi_ic_func_ctrl("edge_palm", 0) < 0)
				ipio_err("Write edge_palm failed\n");
			}
			break;
		default:
			break;
	}
	return cdev->display_rotation;
}

static bool ilitek_get_charger_ststus(void)
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
	ipio_info("charger status:%d", status);
	return status;
}

static void ilitek_work_charger_detect_work(struct work_struct *work)
{
	bool charger_mode_old = idev->charger_mode;

	idev->charger_mode = ilitek_get_charger_ststus();
	if (!idev->tp_suspend && (idev->charger_mode != charger_mode_old)) {
		if (ilitek_tddi_ic_func_ctrl("plug", idev->charger_mode) < 0)	/* charger plug in/out */
			ipio_err("Write charger plug in failed\n");
	}
}

static int ilitek_charger_notify_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct power_supply *psy = data;

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if ((strcmp(psy->desc->name, "usb") == 0)
	    || (strcmp(psy->desc->name, "ac") == 0)) {
		if (delayed_work_pending(&idev->charger_work)) {
			return NOTIFY_DONE;
		}
		queue_delayed_work(idev->ilitek_ts_workqueue, &idev->charger_work, msecs_to_jiffies(500));
	}

	return NOTIFY_DONE;
}

static int ilitek_init_charger_notifier(void)
{
	int ret = 0;

	ipio_info("Init Charger notifier");

	idev->charger_notifier.notifier_call = ilitek_charger_notify_call;
	ret = power_supply_reg_notifier(&idev->charger_notifier);
	return ret;
}

static int ilitek_tp_suspend_show(struct tpd_classdev_t *cdev)
{
	cdev->tp_suspend = idev->tp_suspend;
	return cdev->tp_suspend;
}

static void ilitek_tp_reset_gpio_output(bool value)
{
	ipio_info("ilitek tp reset gpio set value: %d", value);
	gpio_direction_output(idev->tp_rst, value);
}

static int ilitek_set_tp_suspend(struct tpd_classdev_t *cdev, u8 suspend_node, int enable)
{
	if (enable) {
		if (ilitek_tddi_sleep_handler(TP_DEEP_SLEEP) < 0)
			ipio_err("TP suspend failed\n");
	} else {
		if (ilitek_tddi_reset_ctrl(idev->reset) < 0)
			ipio_err("TP Reset failed during init\n");
		if (ilitek_tddi_sleep_handler(TP_RESUME) < 0)
			ipio_err("TP resume failed\n");
	}
	cdev->tp_suspend = idev->tp_suspend;
	return cdev->tp_suspend;
}

static int tpd_get_wakegesture(struct tpd_classdev_t *cdev)
{
	cdev->b_gesture_enable = idev->gesture;
	return 0;
}

static int tpd_enable_wakegesture(struct tpd_classdev_t *cdev, int enable)
{
	if (idev->tp_suspend) {
		cdev->tp_suspend_write_gesture = true;
	}
	idev->gesture = enable;
	return enable;
}

static bool tpd_suspend_need_awake(struct tpd_classdev_t *cdev)
{
	if (!cdev->tp_suspend_write_gesture &&
		(atomic_read(&idev->fw_stat) || idev->gesture)) {
		ipio_info("tp suspend need awake.\n");
		return true;
	} else {
		cdev->tp_suspend_write_gesture = false;
		ipio_info("tp suspend dont need awake.\n");
		return false;
	}
}

static int tpd_test_save_file_path_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_ilitek_save_file_path, 0, sizeof(g_ilitek_save_file_path));
	snprintf(g_ilitek_save_file_path, sizeof(g_ilitek_save_file_path), "%s", buf);
	idev->md_save_file_path = g_ilitek_save_file_path;
	ipio_info("save file path:%s.", idev->md_save_file_path);

	return 0;
}

static int tpd_test_save_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", idev->md_save_file_path);

	return num_read_chars;
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	ipio_info("%s:enter, useless\n", __func__);
	return 0;
}

static int tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int i_len = 0;
	int ret = 0;
	bool esd_en = idev->wq_esd_ctrl, bat_en = idev->wq_bat_ctrl;
	unsigned char *g_user_buf = NULL;

	ipio_info("Run MP test with LCM on\n");

	mutex_lock(&idev->touch_mutex);

	if (esd_en)
		ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);

	g_user_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (ERR_ALLOC_MEM(g_user_buf)) {
		ipio_err("Failed to allocate g_user_buf.\n");
		mutex_unlock(&idev->touch_mutex);
		return 0;
	}
	ilitek_tptest_result = 0;
	ret = ilitek_tddi_mp_test_handler(g_user_buf, ON);
	ipio_info("MP TEST %s, Error code = %d\n", (ret < 0) ? "FAIL" : "PASS", ret);
	if (esd_en)
		ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);

	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", ilitek_tptest_result, idev->stx, idev->srx, 0);

	ipio_info("tpd  test:%s.\n", buf);
	kfree(g_user_buf);
	num_read_chars = i_len;
	mutex_unlock(&idev->touch_mutex);
	return num_read_chars;
}

static int ilitek_print_data2buffer(char *buff_arry[], unsigned int cols, unsigned int rows,
		 s16 *frame_data_words, int idex, int idx, char *name)
{
	int count = 0;
	unsigned int x;
	unsigned int y;

	count += snprintf(buff_arry[idex] + count, RT_DATA_LEN - count,
				"\n%s image[%d]:\n", name, idx);
	for (y = 0; y < rows; y++) {
		count += snprintf(buff_arry[idex] + count, RT_DATA_LEN - count, "[%2d]", (y + 1));
		for (x = 0; x < cols; x++) {
			count += snprintf(buff_arry[idex] + count, RT_DATA_LEN - count,
						"%5d,", frame_data_words[y * cols + x]);
		}
		count += snprintf(buff_arry[idex] + count, RT_DATA_LEN - count, "\n");
	}
	if (strnstr(name, "Rawdata", strlen(name))) {
		count += snprintf(buff_arry[idex] + count, RT_DATA_LEN - count, "TP status:");
		for (x = 0; x < FW_TP_STATUS_BUFFER; x++) {
			count += snprintf(buff_arry[idex] + count, RT_DATA_LEN - count,
						"0x%x,", frame_data_words[cols * rows + x]);
		}
	}
	return count;
}

static int ilitek_data_request(s16 *frame_data_words, enum tp_test_type  test_type)
{
	int row = 0, col = 0;
	int index = 0, ret = 0, i = 0;
	int read_length = 0;
	u8 cmd[2] = { 0 };
	u8 *data = NULL;

	row = idev->ych_num;
	col = idev->xch_num;
	read_length = 4 + 2 * row * col + 1;

	ipio_info("read length = %d\n", read_length);
	data = kcalloc(read_length + 1, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(data)) {
		ipio_err("Failed to allocate data mem\n");
		ret = -1;
		goto out;
	}
	switch (test_type) {
	case  RAWDATA_TEST:
		cmd[0] = 0xB7;
		cmd[1] = 0x2;		/*get rawdata*/
		read_length += FW_TP_STATUS_BUFFER;
		break;
	case  DELTA_TEST:
		cmd[0] = 0xB7;
		cmd[1] = 0x1;		/*get diffdata*/
		break;
	default:
		ipio_err("err command,\n");
		ret = -1;
		goto out;
	}
	ret = idev->write(cmd, sizeof(cmd));
	if (ret < 0) {
		ipio_err("Failed to write 0XB7 command, %d\n", ret);
		goto enter_normal_mode;
	}

	msleep(20);

	/* read debug packet header */
	ret = idev->read(data, read_length);
	if (ret < 0) {
		ipio_err("Read debug packet header failed, %d\n", ret);
		goto enter_normal_mode;
	}
	if (test_type == RAWDATA_TEST) {
		for (i = 4, index = 0; index < row * col + FW_TP_STATUS_BUFFER; i += 2, index++) {
			frame_data_words[index] = (data[i] << 8) + data[i + 1];
		}
	} else {
		for (i = 4, index = 0; index < row * col; i += 2, index++) {
			frame_data_words[index] = (data[i] << 8) + data[i + 1];
		}
	}
enter_normal_mode:
	cmd[1] = 0x03;		/*switch to normal mode*/
	ret = idev->write(cmd, sizeof(cmd));
	if (ret < 0) {
		ipio_err("Failed to write 0xB7,0x3 command, %d\n", ret);
		goto out;
	}
	msleep(20);
out:
	ipio_kfree((void **)&data);
	return ret;
}


static int  ilitek_testing_delta_raw_report(char *buff_arry[], unsigned int num_of_reports)
{

	s16 *frame_data_words = NULL;
	unsigned int col = 0;
	unsigned int row = 0;
	unsigned int idx = 0, idex = 0;
	int retval = 0;

	ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);
	mutex_lock(&idev->touch_mutex);
	ilitek_plat_irq_disable();
	row = idev->ych_num;
	col = idev->xch_num;

	frame_data_words = kcalloc((row * col + FW_TP_STATUS_BUFFER), sizeof(s16), GFP_KERNEL);
	if (ERR_ALLOC_MEM(frame_data_words)) {
		ipio_err("Failed to allocate frame_data_words mem\n");
		retval = -1;
		goto MEM_ALLOC_FAILED;
	}
	for (idx = 0; idx < num_of_reports; idx++) {
		retval = ilitek_data_request(frame_data_words, RAWDATA_TEST);
		if (retval < 0) {
			ipio_err("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}

		idex = idx << 1;
		retval = ilitek_print_data2buffer(buff_arry, col, row,  frame_data_words, idex, idx, "Rawdata");
		if (retval <= 0) {
			ipio_err("print_data2buffer rawdata failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		retval = ilitek_data_request(frame_data_words, DELTA_TEST);
		if (retval < 0) {
			ipio_err("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		idex += 1;
		retval = ilitek_print_data2buffer(buff_arry, col, row,  frame_data_words, idex, idx, "Delta");
		if (retval <= 0) {
			ipio_err("print_data2buffer Delta failed!\n");
			goto DATA_REQUEST_FAILED;
		}
	}

	retval = 0;
	ipio_info("get tp delta raw data end!\n");
DATA_REQUEST_FAILED:
	kfree(frame_data_words);
	frame_data_words = NULL;
MEM_ALLOC_FAILED:
	ipio_info("TP HW RST\n");
	ilitek_plat_tp_reset();
	ilitek_plat_irq_enable();
	mutex_unlock(&idev->touch_mutex);
	ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
	return retval;
}

static int ilitek_tpd_get_noise(struct tpd_classdev_t *cdev, struct list_head *head)
{
	int retval;
	int i = 0;
	char *buf_arry[RT_DATA_NUM];
	struct tp_runtime_data *tp_rt;

	if (idev->tp_suspend)
		return -EIO;

	list_for_each_entry(tp_rt, head, list) {
		buf_arry[i++] = tp_rt->rt_data;
		tp_rt->is_empty = false;
	}

	retval = ilitek_testing_delta_raw_report(buf_arry, RT_DATA_NUM >> 2);
	if (retval < 0) {
		ipio_err("%s: get_raw_noise failed!\n",  __func__);
		return retval;
	}

	return 0;
}

static int tpd_hw_reset(void)
{
#ifdef RESET_LOW_SUSPEND
	ipio_info("tp reset");
	gpio_set_value(idev->tp_rst, 1);
	mdelay(4);
	gpio_set_value(idev->tp_rst, 0);
	mdelay(1);
	gpio_set_value(idev->tp_rst, 1);
	msleep(50);
#endif
	return 0;
}

int ilitek_register_fw_class(void)
{
	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo;
	tpd_fw_cdev.tp_fw_upgrade = ilitek_tp_fw_upgrade;
	tpd_fw_cdev.tp_suspend_show =ilitek_tp_suspend_show;
	tpd_fw_cdev.set_tp_suspend = ilitek_set_tp_suspend;
	tpd_fw_cdev.headset_state_show = ilitek_headset_state_show;
	tpd_fw_cdev.set_headset_state = ilitek_set_headset_state;
	tpd_fw_cdev.set_display_rotation = ilitek_set_display_rotation;
	tpd_fw_cdev.get_gesture = tpd_get_wakegesture;
	tpd_fw_cdev.wake_gesture = tpd_enable_wakegesture;
	tpd_fw_cdev.tpd_suspend_need_awake = tpd_suspend_need_awake;
	tpd_fw_cdev.tp_reset_gpio_output = ilitek_tp_reset_gpio_output;
	tpd_fw_cdev.get_noise = ilitek_tpd_get_noise;
	tpd_fw_cdev.tp_hw_reset = tpd_hw_reset;
#ifdef ILITEK_REPORT_BY_ZTE_ALGO
	tpd_fw_cdev.max_x = TOUCH_SCREEN_X_MAX;
	tpd_fw_cdev.max_y = TOUCH_SCREEN_Y_MAX;
	tpd_fw_cdev.edge_report_limit[0] = ili_left_edge_limit_v;
	tpd_fw_cdev.edge_report_limit[1] = ili_right_edge_limit_v;
	tpd_fw_cdev.edge_report_limit[2] = ili_left_edge_limit_h;
	tpd_fw_cdev.edge_report_limit[3] = ili_right_edge_limit_h;
	tpd_fw_cdev.long_pess_suppression[0] = ili_left_edge_long_pess_v;
	tpd_fw_cdev.long_pess_suppression[1] = ili_right_edge_long_pess_v;
	tpd_fw_cdev.long_pess_suppression[2] = ili_left_edge_long_pess_h;
	tpd_fw_cdev.long_pess_suppression[3] = ili_right_edge_long_pess_h;
	tpd_fw_cdev.long_press_max_count = ili_long_press_max_count;
	tpd_fw_cdev.edge_long_press_check = ili_edge_long_press_check;
#endif
	tpd_fw_cdev.tpd_test_set_save_filepath = tpd_test_save_file_path_store;
	tpd_fw_cdev.tpd_test_get_save_filepath = tpd_test_save_file_path_show;
	tpd_fw_cdev.tpd_test_set_cmd = tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = tpd_test_cmd_show;
	tpd_init_tpinfo(&tpd_fw_cdev);
	idev->ilitek_ts_workqueue = create_singlethread_workqueue("ilitek ts workqueue");
	if (!idev->ilitek_ts_workqueue) {
		ipio_info(" ilitek ts workqueue failed\n");
	} else  {
		INIT_DELAYED_WORK(&idev->charger_work, ilitek_work_charger_detect_work);
		queue_delayed_work(idev->ilitek_ts_workqueue, &idev->charger_work, msecs_to_jiffies(1000));
		ilitek_init_charger_notifier();
	}
	return 0;
}

