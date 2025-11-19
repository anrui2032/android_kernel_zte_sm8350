/************************************************************************
*
* File Name: ili9881x_common_interface.c
*
*  *   Version: v1.0
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/

#include "ili9881x.h"
#include "ili9881x_fw.h"
#include <linux/kernel.h>
#include <linux/power_supply.h>
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

#define MAX_FILE_NAME_LEN       64
#define MAX_FILE_PATH_LEN  64
#define MAX_NAME_LEN_20  20
#define FW_TP_STATUS_BUFFER 9

int ili9881x_vendor_id = 0;
int ilitek_tptest_result = 0;
char ili9881x_vendor_name[MAX_NAME_LEN_20] = { 0 };
char ili9881x_firmware_name[MAX_FILE_NAME_LEN] = {0};
char fw_filp_path[MAX_FILE_NAME_LEN] = { 0 };
char ini_filp_path[MAX_FILE_NAME_LEN] = { 0 };
char ini_rq_path[MAX_FILE_NAME_LEN] = { 0 };
char g_ili9881x_save_file_path[MAX_FILE_PATH_LEN] = { 0 };
#ifdef ILITEK_EDGE_LEVEL
static void ili9881x_set_edge_limit_level(void);
#endif

struct tpvendor_t ili9881x_vendor_l[] = {
	{ILI_VENDOR_ID_0, ILI_VENDOR_0_NAME},
	{ILI_VENDOR_ID_1, ILI_VENDOR_1_NAME},
	{ILI_VENDOR_ID_2, ILI_VENDOR_2_NAME},
	{ILI_VENDOR_ID_3, ILI_VENDOR_2_NAME},
	{VENDOR_END, "Unknown"},
};

int ili9881x_get_fw(u8 vendor_id)
{
	int i = 0;
	int ret = 0;
	const char *panel_name = NULL;

	if (ILI_MODULE_NUM == 1) {
		ili9881x_vendor_id = ILI_VENDOR_ID_0;
		strlcpy(ili9881x_vendor_name, ILI_VENDOR_0_NAME, sizeof(ili9881x_vendor_name));
		ret = 0;
		goto out;
	}

#ifndef GET_VENDOR_NAME_BY_LCDINFO
	for (i = 0; i < ARRAY_SIZE(ili9881x_vendor_l) && i < ILI_MODULE_NUM; i++) {
		if (ili9881x_vendor_l[i].vendor_id == vendor_id) {
			ili9881x_vendor_id = ili9881x_vendor_l[i].vendor_id;
			strlcpy(ili9881x_vendor_name, ili9881x_vendor_l[i].vendor_name,
				sizeof(ili9881x_vendor_name));
			ret = 0;
			goto out;
		}
	}
#endif
	panel_name = get_lcd_panel_name();
	for (i = 0; i < ARRAY_SIZE(ili9881x_vendor_l); i++) {
		if (strnstr(panel_name, ili9881x_vendor_l[i].vendor_name, strlen(panel_name))) {
			ili9881x_vendor_id = ili9881x_vendor_l[i].vendor_id;
			strlcpy(ili9881x_vendor_name, ili9881x_vendor_l[i].vendor_name,
				sizeof(ili9881x_vendor_name));
			ret = 0;
			goto out;
		}
	}
	strlcpy(ili9881x_vendor_name, "Unknown", sizeof(ili9881x_vendor_name));
	ret = -EIO;
out:
	snprintf(ili9881x_firmware_name, sizeof(ili9881x_firmware_name),
			"ili9881x_firmware_%s.hex", ili9881x_vendor_name);
	snprintf(ini_rq_path, sizeof(ini_rq_path), "system/etc/ili9881x_mp_%s.ini", ili9881x_vendor_name);
	return ret;
}

void ili9881x_update_module_info(void)
{
	ilits->md_name = ili9881x_vendor_name;
	ilits->md_fw_rq_path = ili9881x_firmware_name;
	ilits->md_ini_rq_path = ini_rq_path;
	ilits->md_ini_path = ini_rq_path;
	ilits->md_save_file_path = CSV_LCM_ON_PATH;
	ilits->md_fw_ili_size = 0;
	ILI_INFO("Found %s module\n", ilits->md_name);
	ilits->tp_module = ili9881x_vendor_id;
}

static int tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	u8 vendor_id = 0;

#if (TDDI_INTERFACE == BUS_I2C)
	ilits->info_from_hex = DISABLE;
#endif
	if (atomic_read(&ilits->ice_stat))
		ili_ice_mode_ctrl(DISABLE, OFF);
	ili_ic_get_fw_ver();
 #if (TDDI_INTERFACE == BUS_I2C)
	ilits->info_from_hex = ENABLE;
#endif
	ILI_INFO("Firmware version = 0x%x.\n", ilits->chip->fw_ver);
	vendor_id = (ilits->chip->fw_ver >> 24) & 0xff;
	ili9881x_get_fw(vendor_id);
	ili9881x_update_module_info();
	strlcpy(cdev->ic_tpinfo.tp_name, "ilitek", sizeof(cdev->ic_tpinfo.tp_name));
	strlcpy(cdev->ic_tpinfo.vendor_name, ili9881x_vendor_name, sizeof(cdev->ic_tpinfo.vendor_name));
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_ILITEK;
	cdev->ic_tpinfo.firmware_ver = ilits->chip->fw_ver;
	cdev->ic_tpinfo.module_id = ili9881x_vendor_id;
	cdev->ic_tpinfo.i2c_type = 0;
	cdev->ic_tpinfo.i2c_addr =  ilits->i2c->addr;
	return 0;
}

static int ili9881x_tp_fw_upgrade(struct tpd_classdev_t *cdev, char *fw_name, int fwname_len)
{
	char fwname[MAX_FILE_NAME_LEN] = { 0 };
	bool esd_en = ilits->wq_esd_ctrl, bat_en = ilits->wq_bat_ctrl;

	ILI_INFO("Preparing to upgarde firmware\n");
	if ((fwname_len <= 1) || (fwname_len >= MAX_FILE_NAME_LEN)) {
		ILI_ERR("fw hex name's length(%d) fail.\n", fwname_len);
		return -EINVAL;
	}
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, sizeof(fwname), "%s", fw_name);
	fwname[fwname_len - 1] = '\0';
	ILI_INFO("fwname is %s.\n", fwname);
	ilits->md_fw_filp_path = kzalloc(MAX_FILE_NAME_LEN, GFP_KERNEL);
	if (ilits->md_fw_filp_path == NULL) {
		ILI_ERR("md_fw_filp_path kzalloc fail.\n");
		return -ENOMEM;
	}
	snprintf(ilits->md_fw_filp_path, MAX_FILE_NAME_LEN, "/sdcard/%s", fwname);
	ILI_INFO("md_fw_filp_path is %s.\n", ilits->md_fw_filp_path);
	mutex_lock(&ilits->touch_mutex);
	if (esd_en)
		ili_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, DISABLE);

	ilits->force_fw_update = ENABLE;
	ilits->node_update = true;
	ilits->fw_open = FILP_OPEN;
	ili_fw_upgrade_handler(NULL);
	ilits->force_fw_update = DISABLE;
	ilits->node_update = false;
	ilits->fw_open = REQUEST_FIRMWARE;
	kfree(ilits->md_fw_filp_path);
	ilits->md_fw_filp_path = DEF_FW_FILP_PATH;

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, ENABLE);

	mutex_unlock(&ilits->touch_mutex);
	return 0;
}

void ili_touch_reset_and_clean_point(void)
{
	ili_tp_reset();
	ili_touch_release_all_point();
}

void ili9881x_ex_mode_recovery(void)
{
	if (ilits->charger_mode) {
		if (ili_ic_func_ctrl("plug", ENABLE) < 0)
			ILI_ERR("Write plug out failed\n");
	}
	if (ilits->headset_mode) {
		if (ili_ic_func_ctrl("ear_phone", ENABLE) < 0)
			ILI_ERR("Write headset plug in failed\n");
	}
#ifdef ILITEK_EDGE_LEVEL
	ili9881x_set_edge_limit_level();
#endif
}

static int ili9881x_headset_state_show(struct tpd_classdev_t *cdev)
{
	cdev->headset_state = ilits->headset_mode;
	return cdev->headset_state;
}

static int ili9881x_set_headset_state(struct tpd_classdev_t *cdev, int enable)
{
	ilits->headset_mode = enable;
	ILI_INFO("%s: headset_state = %d.\n", __func__, ilits->headset_mode);
	if (!ilits->tp_suspend) {
		ili_touch_reset_and_clean_point();
		if (ili_ic_func_ctrl("ear_phone", enable) < 0)	/* headset plug in/out */
			ILI_ERR("Write headset plug in failed\n");
	}
	return ilits->headset_mode;
}

#ifdef ILITEK_EDGE_LEVEL
static void ili9881x_set_edge_limit_level(void)
{
	u8 edge_palm_para = 0;

	ILI_INFO("edge_limit_level :%d,display_rotation:%d\n",
		ilits->edge_limit_level, ilits->display_rotation);
	switch (ilits->display_rotation) {
	case mRotatin_0:
		edge_palm_para = ilits->edge_limit_level << 4 | 0x01;
		break;
	case mRotatin_90:
		edge_palm_para = ilits->edge_limit_level << 4 | 0x02;
		break;
	case mRotatin_180:
		edge_palm_para = ilits->edge_limit_level << 4 | 0x01;
		break;
	case mRotatin_270:
		edge_palm_para = ilits->edge_limit_level << 4;
		break;
	default:
		break;
	}
	ILI_INFO("edge_palm_para : 0X%x.\n", edge_palm_para);
	if (ili_ic_func_ctrl("edge_palm", edge_palm_para) < 0)
		ILI_ERR("Write edge_palm failed\n");
}

static int tpd_set_edge_limit_level(struct tpd_classdev_t *cdev, u8 level)
{
	ilits->edge_limit_level = level;
	ili9881x_set_edge_limit_level();
	return level;
}
#endif

static int ili9881x_set_display_rotation(struct tpd_classdev_t *cdev, int mrotation)
{
	u8 edge_palm_para = 0;

	ilits->display_rotation = mrotation;
	if (ilits->tp_suspend)
		return 0;
	ILI_INFO("%s: display_rotation = %d.\n", __func__, ilits->display_rotation);
#ifdef ILITEK_EDGE_LEVEL
	switch (ilits->display_rotation) {
	case mRotatin_0:
		edge_palm_para = ilits->edge_limit_level << 4 | 0x01;
		break;
	case mRotatin_90:
		edge_palm_para = ilits->edge_limit_level << 4 | 0x02;
		break;
	case mRotatin_180:
		edge_palm_para = ilits->edge_limit_level << 4 | 0x01;
		break;
	case mRotatin_270:
		edge_palm_para = ilits->edge_limit_level << 4;
		break;
	default:
		break;
	}
#else
	switch (ilits->display_rotation) {
	case mRotatin_0:
		edge_palm_para = 1;
		break;
	case mRotatin_90:
		edge_palm_para = 2;
		break;
	case mRotatin_180:
		edge_palm_para = 1;
		break;
	case mRotatin_270:
		edge_palm_para = 0;
		break;
	default:
		break;
	}
#endif
	ili_touch_reset_and_clean_point();
	ILI_INFO("edge_palm_para : 0X%x.\n", edge_palm_para);
	if (ili_ic_func_ctrl("edge_palm", edge_palm_para) < 0)
		ILI_ERR("Write edge_palm failed\n");

	return ilits->display_rotation;
}

static bool ili9881x_get_charger_ststus(void)
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
	ILI_INFO("charger status:%d", status);
	return status;
}

static void ili9881x_work_charger_detect_work(struct work_struct *work)
{
	bool charger_mode_old = ilits->charger_mode;

	ilits->charger_mode = ili9881x_get_charger_ststus();
	if (!ilits->tp_suspend && (ilits->charger_mode != charger_mode_old)) {
		ili_touch_reset_and_clean_point();
		if (ili_ic_func_ctrl("plug", ilits->charger_mode) < 0)	/* charger plug in/out */
			ILI_ERR("Write charger plug in failed\n");
	}
}

static int ili9881x_charger_notify_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct power_supply *psy = data;

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if ((strcmp(psy->desc->name, "usb") == 0)
	    || (strcmp(psy->desc->name, "ac") == 0)) {
		if (delayed_work_pending(&ilits->charger_work)) {
			return NOTIFY_DONE;
		}
		queue_delayed_work(ilits->ili9881x_ts_workqueue, &ilits->charger_work, msecs_to_jiffies(500));
	}

	return NOTIFY_DONE;
}

static int ili9881x_init_charger_notifier(void)
{
	int ret = 0;

	ILI_INFO("Init Charger notifier");

	ilits->charger_notifier.notifier_call = ili9881x_charger_notify_call;
	ret = power_supply_reg_notifier(&ilits->charger_notifier);
	return ret;
}

void ilitek_resume_work(struct work_struct *work)
{
	if (ili_sleep_handler(TP_RESUME) < 0)
		ILI_ERR("TP resume failed\n");
}

static int ili9881x_tp_suspend_show(struct tpd_classdev_t *cdev)
{
	cdev->tp_suspend = ilits->tp_suspend;
	return cdev->tp_suspend;
}

static void ili9881x_tp_reset_gpio_output(bool value)
{
	ILI_INFO("ilitek tp reset gpio set value: %d", value);
	if (gpio_is_valid(ilits->tp_rst))
		gpio_direction_output(ilits->tp_rst, value);
}

static int ili9881x_set_tp_suspend(struct tpd_classdev_t *cdev, u8 suspend_node, int enable)
{
	if (enable) {
		if (ili_sleep_handler(TP_DEEP_SLEEP) < 0)
			ILI_ERR("TP suspend failed\n");
	} else {
#ifndef CONFIG_SOC_SPRD
		if (ili_reset_ctrl(ilits->reset) < 0)
			ILI_ERR("TP Reset failed during init\n");
#endif
		queue_work(ilits->ili9881x_ts_workqueue, &ilits->resume_work);
	}
	cdev->tp_suspend = ilits->tp_suspend;
	return cdev->tp_suspend;
}

static int tpd_get_wakegesture(struct tpd_classdev_t *cdev)
{
	cdev->b_gesture_enable = ilits->gesture;
	return 0;
}

static int tpd_enable_wakegesture(struct tpd_classdev_t *cdev, int enable)
{
	if (ilits->tp_suspend) {
		cdev->tp_suspend_write_gesture = true;
	}
	ilits->gesture = enable;
	return enable;
}

static bool tpd_suspend_need_awake(struct tpd_classdev_t *cdev)
{
	if (!cdev->tp_suspend_write_gesture &&
		(atomic_read(&ilits->fw_stat) || ilits->gesture)) {
		ILI_INFO("tp suspend need awake.\n");
		return true;
	} else {
		ILI_INFO("tp suspend dont need awake.\n");
		return false;
	}
}

static int ili9881x_print_data2buffer(char *buff_arry[], unsigned int cols, unsigned int rows,
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

static int ili9881x_data_request(s16 *frame_data_words, enum tp_test_type  test_type)
{
	int row = 0, col = 0;
	int index = 0, ret = 0, i = 0;
	int read_length = 0;
	u8 cmd[2] = { 0 };
	u8 *data = NULL;

	row = ilits->ych_num;
	col = ilits->xch_num;
	read_length = 4 + 2 * row * col + 1;

	ILI_INFO("read length = %d\n", read_length);
	data = kcalloc(read_length + 1, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(data)) {
		ILI_ERR("Failed to allocate data mem\n");
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
		ILI_ERR("err command,\n");
		ret = -1;
		goto out;
	}
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, OFF, OFF);
	if (ret < 0) {
		ILI_ERR("Failed to write 0XB7 command, %d\n", ret);
		goto enter_normal_mode;
	}

	msleep(20);

	/* read debug packet header */
	ret = ilits->wrapper(NULL, 0, data, read_length, OFF, OFF);
	if (ret < 0) {
		ILI_ERR("Read debug packet header failed, %d\n", ret);
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
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, ON, OFF);
	if (ret < 0) {
		ILI_ERR("Failed to write 0xB7,0x3 command, %d\n", ret);
		goto out;
	}
	msleep(20);
out:
	ipio_kfree((void **)&data);
	return ret;
}


static int  ili9881x_testing_delta_raw_report(char *buff_arry[], unsigned int num_of_reports)
{

	s16 *frame_data_words = NULL;
	unsigned int col = 0;
	unsigned int row = 0;
	unsigned int idx = 0, idex = 0;
	int retval = 0;

	ili_wq_ctrl(WQ_ESD, DISABLE);
	ili_wq_ctrl(WQ_BAT, DISABLE);
	mutex_lock(&ilits->touch_mutex);
	ili_irq_disable();
	row = ilits->ych_num;
	col = ilits->xch_num;

	frame_data_words = kcalloc((row * col + FW_TP_STATUS_BUFFER), sizeof(s16), GFP_KERNEL);
	if (ERR_ALLOC_MEM(frame_data_words)) {
		ILI_ERR("Failed to allocate frame_data_words mem\n");
		retval = -1;
		goto MEM_ALLOC_FAILED;
	}
	for (idx = 0; idx < num_of_reports; idx++) {
		retval = ili9881x_data_request(frame_data_words, RAWDATA_TEST);
		if (retval < 0) {
			ILI_ERR("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}

		idex = idx << 1;
		retval = ili9881x_print_data2buffer(buff_arry, col, row,  frame_data_words, idex, idx, "Rawdata");
		if (retval <= 0) {
			ILI_ERR("print_data2buffer rawdata failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		retval = ili9881x_data_request(frame_data_words, DELTA_TEST);
		if (retval < 0) {
			ILI_ERR("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		idex += 1;
		retval = ili9881x_print_data2buffer(buff_arry, col, row,  frame_data_words, idex, idx, "Delta");
		if (retval <= 0) {
			ILI_ERR("print_data2buffer Delta failed!\n");
			goto DATA_REQUEST_FAILED;
		}
	}

	retval = 0;
	ILI_INFO("get tp delta raw data end!\n");
DATA_REQUEST_FAILED:
	kfree(frame_data_words);
	frame_data_words = NULL;
MEM_ALLOC_FAILED:
	ILI_INFO("TP HW RST\n");
	ili_tp_reset();
	ili_irq_enable();
	mutex_unlock(&ilits->touch_mutex);
	ili_wq_ctrl(WQ_ESD, ENABLE);
	ili_wq_ctrl(WQ_BAT, ENABLE);
	return retval;
}

static int ili9881x_tpd_get_noise(struct tpd_classdev_t *cdev, struct list_head *head)
{
	int retval;
	int i = 0;
	char *buf_arry[RT_DATA_NUM];
	struct tp_runtime_data *tp_rt;

	if (ilits->tp_suspend)
		return -EIO;

	list_for_each_entry(tp_rt, head, list) {
		buf_arry[i++] = tp_rt->rt_data;
		tp_rt->is_empty = false;
	}

	retval = ili9881x_testing_delta_raw_report(buf_arry, RT_DATA_NUM >> 2);
	if (retval < 0) {
		ILI_ERR("%s: get_raw_noise failed!\n",  __func__);
		return retval;
	}

	return 0;
}

static int tpd_test_save_file_path_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_ili9881x_save_file_path, 0, sizeof(g_ili9881x_save_file_path));
	snprintf(g_ili9881x_save_file_path, sizeof(g_ili9881x_save_file_path), "%s", buf);
	ilits->md_save_file_path = g_ili9881x_save_file_path;
	ILI_INFO("save file path:%s.", ilits->md_save_file_path);

	return 0;
}

static int tpd_test_save_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", ilits->md_save_file_path);

	return num_read_chars;
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	ILI_INFO("%s:enter, useless\n", __func__);
	return 0;
}

static int tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int i_len = 0;
	int ret = 0;
	bool esd_en = ilits->wq_esd_ctrl, bat_en = ilits->wq_bat_ctrl;
	unsigned char *g_user_buf = NULL;

	ILI_INFO("Run MP test with LCM on\n");

	mutex_lock(&ilits->touch_mutex);

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, DISABLE);

	g_user_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (ERR_ALLOC_MEM(g_user_buf)) {
		ILI_ERR("Failed to allocate g_user_buf.\n");
		mutex_unlock(&ilits->touch_mutex);
		return 0;
	}
	ilitek_tptest_result = 0;
	ret = ili_mp_test_handler(g_user_buf, ON);
	ILI_INFO("MP TEST %s, Error code = %d\n", (ret < 0) ? "FAIL" : "PASS", ret);
	if (esd_en)
		ili_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, ENABLE);

	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", ilitek_tptest_result, ilits->stx, ilits->srx, 0);

	ILI_INFO("tpd  test:%s.\n", buf);
	kfree(g_user_buf);
	num_read_chars = i_len;
	mutex_unlock(&ilits->touch_mutex);
	return num_read_chars;
}

int ili9881x_register_fw_class(void)
{
	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo;
	tpd_fw_cdev.tp_fw_upgrade = ili9881x_tp_fw_upgrade;
	tpd_fw_cdev.tp_suspend_show = ili9881x_tp_suspend_show;
	tpd_fw_cdev.set_tp_suspend = ili9881x_set_tp_suspend;
	tpd_fw_cdev.headset_state_show = ili9881x_headset_state_show;
	tpd_fw_cdev.set_headset_state = ili9881x_set_headset_state;
	tpd_fw_cdev.set_display_rotation = ili9881x_set_display_rotation;
	tpd_fw_cdev.get_gesture = tpd_get_wakegesture;
	tpd_fw_cdev.wake_gesture = tpd_enable_wakegesture;
	tpd_fw_cdev.tpd_suspend_need_awake = tpd_suspend_need_awake;
	tpd_fw_cdev.tp_reset_gpio_output = ili9881x_tp_reset_gpio_output;
	tpd_fw_cdev.get_noise = ili9881x_tpd_get_noise;
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
#ifdef ILITEK_EDGE_LEVEL
	tpd_fw_cdev.set_edge_limit_level = tpd_set_edge_limit_level;
	ilits->edge_limit_level = 0;
#endif

	tpd_fw_cdev.tpd_test_set_save_filepath = tpd_test_save_file_path_store;
	tpd_fw_cdev.tpd_test_get_save_filepath = tpd_test_save_file_path_show;
	tpd_fw_cdev.tpd_test_set_cmd = tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = tpd_test_cmd_show;
	tpd_init_tpinfo(&tpd_fw_cdev);
	ilits->ili9881x_ts_workqueue = create_singlethread_workqueue("ilitek ts workqueue");
	if (!ilits->ili9881x_ts_workqueue) {
		ILI_INFO(" ili9881x ts workqueue failed\n");
	} else  {
		INIT_DELAYED_WORK(&ilits->charger_work, ili9881x_work_charger_detect_work);
		INIT_WORK(&ilits->resume_work, ilitek_resume_work);
		queue_delayed_work(ilits->ili9881x_ts_workqueue, &ilits->charger_work, msecs_to_jiffies(1000));
		ili9881x_init_charger_notifier();
	}

	return 0;
}

