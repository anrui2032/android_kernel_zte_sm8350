/************************************************************************
*
* File Name: nt36xxx_common_interface.c
*
*  *   Version: v1.0
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/

#include "nt36xxx.h"
#include <linux/kernel.h>
#include <linux/power_supply.h>
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

#define MAX_FILE_NAME_LEN       64
#define MAX_FILE_PATH_LEN  64
#define MAX_NAME_LEN_20  20
#define NORMAL_MODE 0x00
#define TEST_MODE_2 0x22

int novatek_tptest_result = 0;

extern int32_t nvt_ts_resume(struct device *dev);
extern int32_t nvt_ts_suspend(struct device *dev);
extern int32_t nvt_selftest_open(struct inode *inode, struct file *file);
extern uint8_t nvt_get_fw_pipe(void);
extern void nvt_read_mdata(uint32_t xdata_addr, uint32_t xdata_btn_addr);
extern void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num);
extern void nvt_change_mode(uint8_t mode);

enum nvt_tp_cmd_type {
	HEADSET = 0,
	MROTATION = 1,
	EDGE_LIMIT = 2,
	CHARGER = 3,
};

char nt36xx_vendor_name[MAX_NAME_LEN_20] = { 0 };
char nt36xx_firmware_name[MAX_FILE_NAME_LEN] = {0};
char nt36xx_firmware_mp_name[MAX_FILE_NAME_LEN] = {0};
int nt36xx_vendor_id = 0;

struct tpvendor_t nt36xx_vendor_l[] = {
	{NVT_VENDOR_ID_0, NVT_VENDOR_0_NAME},
	{NVT_VENDOR_ID_1, NVT_VENDOR_1_NAME},
	{NVT_VENDOR_ID_2, NVT_VENDOR_2_NAME},
	{NVT_VENDOR_ID_3, NVT_VENDOR_3_NAME},
	{VENDOR_END, "Unknown"},
};

int nt36xx_get_fw(void)
{
	int i = 0;
	int ret = 0;
	const char *panel_name = NULL;

	if (NVT_MODULE_NUM == 1) {
		nt36xx_vendor_id = NVT_VENDOR_ID_0;
		strlcpy(nt36xx_vendor_name, NVT_VENDOR_0_NAME, sizeof(nt36xx_vendor_name));
		ret = 0;
		goto out;
	}

	panel_name = get_lcd_panel_name();
	for (i = 0; i < ARRAY_SIZE(nt36xx_vendor_l); i++) {
		if (strnstr(panel_name, nt36xx_vendor_l[i].vendor_name, strlen(panel_name))) {
			nt36xx_vendor_id = nt36xx_vendor_l[i].vendor_id;
			strlcpy(nt36xx_vendor_name, nt36xx_vendor_l[i].vendor_name,
				sizeof(nt36xx_vendor_name));
			ret = 0;
			goto out;
		}
	}
	strlcpy(nt36xx_vendor_name, "Unknown", sizeof(nt36xx_vendor_name));
	ret = -EIO;
out:
	snprintf(nt36xx_firmware_name, sizeof(nt36xx_firmware_name),
			"nt36xx_firmware_%s.bin", nt36xx_vendor_name);
	snprintf(nt36xx_firmware_mp_name, sizeof(nt36xx_firmware_mp_name),
			"nt36xx_firmware_mp_%s.bin", nt36xx_vendor_name);
	return ret;
}

static int tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	strlcpy(cdev->ic_tpinfo.tp_name, "novatek", sizeof(cdev->ic_tpinfo.tp_name));
	strlcpy(cdev->ic_tpinfo.vendor_name, nt36xx_vendor_name, sizeof(cdev->ic_tpinfo.vendor_name));
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_NOVATEK;
	cdev->ic_tpinfo.firmware_ver = ts->fw_ver;
	cdev->ic_tpinfo.module_id = nt36xx_vendor_id;
	cdev->ic_tpinfo.i2c_type = 1;
	return 0;
}

int nt36xx_cmd_ctrl(enum nvt_tp_cmd_type cmd, uint8_t ctrl)
{
	uint8_t buf[8] = { 0 };
	int32_t ret = 0;

	/* ---set xdata index to EVENT BUF ADDR--- */
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		return -EINVAL;
	}

	switch (cmd) {
	case HEADSET:
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = EVENT_MAP_HEASET;
		buf[2] = ctrl ? 1 : 0;
		NVT_LOG("headset cmd, set %d.\n", ctrl);
		break;
	case MROTATION:
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = EVENT_MAP_MROTATION;
		buf[2] = ctrl;
		NVT_LOG("mration cmd, set %d.\n", ctrl);
		break;
	case EDGE_LIMIT:
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = EVENT_MAP_EDGE_LIMIT;
		buf[2] = ctrl;
		NVT_LOG("edge limit cmd, set %d.\n", ctrl);
		break;
	case CHARGER:
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = EVENT_MAP_CHARGER;
		buf[2] = ctrl ? 1 : 0;
		NVT_LOG("charge cmd, set %d.\n", ctrl);
		break;
	default:
		NVT_ERR("Invalid cmd!\n");
		return -EINVAL;
	}
	/* ---set mode--- */
	CTP_SPI_WRITE(ts->client, buf, 3);
	return 0;
}

static int nt36xx_headset_state_show(struct tpd_classdev_t *cdev)
{
	cdev->headset_state = ts->headset_mode;
	return cdev->headset_state;
}

static int nt36xx_set_headset_state(struct tpd_classdev_t *cdev, int enable)
{
	ts->headset_mode = enable;
	NVT_LOG("%s: headset_state = %d.\n", __func__, ts->headset_mode);
	if (!ts->tp_suspend) {
		if (nt36xx_cmd_ctrl(HEADSET, enable) < 0)	/* headset plug in/out */
			NVT_ERR("%s:Write headset plug failed\n",  __func__);
		else
			NVT_LOG("%s: Write headset plug successful.\n", __func__);
	}
	return ts->headset_mode;
}

static int nt36xx_set_edge_limit_level(struct tpd_classdev_t *cdev, u8 level)
{
	ts->edge_limit_level = level;
	if (!ts->tp_suspend) {
		if (nt36xx_cmd_ctrl(EDGE_LIMIT, level) < 0)
			NVT_ERR("%s:Write edge limit failed\n",  __func__);
		else
			NVT_LOG("%s: Write edge limit successful.\n", __func__);
	}
	return level;
}

static int nt36xx_set_display_rotation(struct tpd_classdev_t *cdev, int mrotation)
{
	u8 edge_palm_para = 0;

	ts->display_rotation = mrotation;
	if (ts->tp_suspend)
		return 0;
	NVT_LOG("%s: display_rotation = %d.\n", __func__, ts->display_rotation);
	switch (ts->display_rotation) {
	case mRotatin_0:
		edge_palm_para = 2;
		break;
	case mRotatin_90:
		edge_palm_para = 1;
		break;
	case mRotatin_180:
		edge_palm_para = 2;
		break;
	case mRotatin_270:
		edge_palm_para = 3;
		break;
	default:
		break;
	};
	NVT_LOG("edge_palm_para : 0X%x.\n", edge_palm_para);
	if (nt36xx_cmd_ctrl(MROTATION, edge_palm_para) < 0)
		NVT_ERR("%s:Write rotation cmd failed\n",  __func__);
	else
		NVT_LOG("%s: Write  rotation cmd successful.\n", __func__);

	return ts->display_rotation;
}

static bool nt36xx_get_charger_ststus(void)
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
	NVT_LOG("charger status:%d", status);
	return status;
}

static void nt36xx_work_charger_detect_work(struct work_struct *work)
{
	bool charger_mode_old = ts->charger_mode;

	ts->charger_mode = nt36xx_get_charger_ststus();
	if (!ts->tp_suspend && (ts->charger_mode != charger_mode_old)) {
		if (nt36xx_cmd_ctrl(CHARGER, ts->charger_mode) < 0)
			NVT_ERR("%s:Write charger plug failed\n",  __func__);
		else
			NVT_LOG("%s: Write  charger plug successful.\n", __func__);

	}
}

static int nt36xx_charger_notify_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct power_supply *psy = data;

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if ((strcmp(psy->desc->name, "usb") == 0)
	    || (strcmp(psy->desc->name, "ac") == 0)) {
		if (delayed_work_pending(&ts->charger_work)) {
			return NOTIFY_DONE;
		}
		queue_delayed_work(ts->nt36xx_ts_workqueue, &ts->charger_work, msecs_to_jiffies(500));
	}

	return NOTIFY_DONE;
}

static int nt36xx_init_charger_notifier(void)
{
	int ret = 0;

	NVT_LOG("Init Charger notifier");

	ts->charger_notifier.notifier_call = nt36xx_charger_notify_call;
	ret = power_supply_reg_notifier(&ts->charger_notifier);
	return ret;
}

void novatek_resume_work(struct work_struct *work)
{
	nvt_ts_resume(&ts->client->dev);
}

static int nt36xx_tp_suspend_show(struct tpd_classdev_t *cdev)
{
	cdev->tp_suspend = ts->tp_suspend;
	return cdev->tp_suspend;
}

static int nt36xx_set_tp_suspend(struct tpd_classdev_t *cdev, u8 suspend_node, int enable)
{
	if (enable) {
		nvt_ts_suspend(&ts->client->dev);
	} else {
		queue_work(ts->nt36xx_ts_workqueue, &ts->resume_work);
	}
	cdev->tp_suspend = enable;
	return cdev->tp_suspend;
}

static int tpd_get_wakegesture(struct tpd_classdev_t *cdev)
{
	cdev->b_gesture_enable = ts->gesture;
	return 0;
}

static int tpd_enable_wakegesture(struct tpd_classdev_t *cdev, int enable)
{
	if (ts->tp_suspend) {
		cdev->tp_suspend_write_gesture = true;
	}
	ts->gesture = enable;
	return enable;
}

static bool tpd_suspend_need_awake(struct tpd_classdev_t *cdev)
{
	if (!cdev->tp_suspend_write_gesture && ts->gesture) {
		NVT_LOG("tp suspend need awake.\n");
		return true;
	}
	NVT_LOG("tp suspend dont need awake.\n");
	return false;
}

void nt36xx_ex_mode_recovery(void)
{
	nt36xx_cmd_ctrl(HEADSET, ts->headset_mode);
	nt36xx_cmd_ctrl(EDGE_LIMIT, ts->edge_limit_level);
	nt36xx_cmd_ctrl(CHARGER, ts->charger_mode);
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	NVT_LOG("%s:enter, useless\n", __func__);
	return 0;
}

static int tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int i_len = 0;

	novatek_tptest_result = 0;
	nvt_selftest_open(NULL, NULL);
	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", novatek_tptest_result, ts->x_num, ts->y_num, 0);

	NVT_LOG("tpd  test:%s.\n", buf);
	num_read_chars = i_len;
	return num_read_chars;
}

static int nt36xx_print_data2buffer(char *buff_arry[], unsigned int cols, unsigned int rows,
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

	return count;
}

static int nt36xx_data_request(s16 *frame_data_words, enum tp_test_type  test_type)
{
	int index = 0, ret = 0;
	int read_length = 0;
	int32_t *data = NULL;
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint8_t x = 0;
	uint8_t y = 0;

	read_length = ts->x_num * ts->y_num + TOUCH_KEY_NUM;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}
	NVT_LOG("read length = %d\n", read_length);
	data = kcalloc(read_length + 1, sizeof(int32_t), GFP_KERNEL);
	if (data == NULL) {
		NVT_ERR("Failed to allocate data mem\n");
		ret = -1;
		goto out;
	}
	switch (test_type) {
	case  RAWDATA_TEST:
		if (nvt_get_fw_pipe() == 0)
			nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_ADDR);
		else
			nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_ADDR);
		break;
	case  DELTA_TEST:
		if (nvt_get_fw_pipe() == 0)
			nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
		else
			nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);
		break;
	default:
		NVT_ERR("err command,\n");
		ret = -1;
		goto out;
	}
	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	msleep(20);
	nvt_get_mdata(data, &x_num, &y_num);
	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			index = y * x_num + x;
			frame_data_words[index] = (int16_t) data[index];
		}
	}
out:
	kfree(data);
	return ret;
}

static int  nt36xx_testing_delta_raw_report(char *buff_arry[], unsigned int num_of_reports)
{

	s16 *frame_data_words = NULL;
	unsigned int col = 0;
	unsigned int row = 0;
	unsigned int idx = 0, idex = 0;
	int retval = 0;

	row = ts->y_num;
	col = ts->x_num;
	frame_data_words = kcalloc((row * col + 1), sizeof(s16), GFP_KERNEL);
	if (frame_data_words == NULL) {
		NVT_ERR("Failed to allocate frame_data_words mem\n");
		retval = -1;
		goto MEM_ALLOC_FAILED;
	}
	for (idx = 0; idx < num_of_reports; idx++) {
		retval = nt36xx_data_request(frame_data_words, RAWDATA_TEST);
		if (retval < 0) {
			NVT_ERR("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}

		idex = idx << 1;
		retval = nt36xx_print_data2buffer(buff_arry, col, row,  frame_data_words, idex, idx, "Rawdata");
		if (retval <= 0) {
			NVT_ERR("print_data2buffer rawdata failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		retval = nt36xx_data_request(frame_data_words, DELTA_TEST);
		if (retval < 0) {
			NVT_ERR("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		idex += 1;
		retval = nt36xx_print_data2buffer(buff_arry, col, row,  frame_data_words, idex, idx, "Delta");
		if (retval <= 0) {
			NVT_ERR("print_data2buffer Delta failed!\n");
			goto DATA_REQUEST_FAILED;
		}
	}

	retval = 0;
	NVT_LOG("get tp delta raw data end!\n");
DATA_REQUEST_FAILED:
	kfree(frame_data_words);
	frame_data_words = NULL;
MEM_ALLOC_FAILED:
	return retval;
}

static int nt36xx_tpd_get_noise(struct tpd_classdev_t *cdev, struct list_head *head)
{
	int retval;
	int i = 0;
	char *buf_arry[RT_DATA_NUM];
	struct tp_runtime_data *tp_rt;

	if (ts->tp_suspend)
		return -EIO;

	list_for_each_entry(tp_rt, head, list) {
		buf_arry[i++] = tp_rt->rt_data;
		tp_rt->is_empty = false;
	}

	retval = nt36xx_testing_delta_raw_report(buf_arry, RT_DATA_NUM >> 2);
	if (retval < 0) {
		NVT_ERR("%s: get_raw_noise failed!\n",  __func__);
		return retval;
	}

	return 0;
}

int nt36xx_register_fw_class(void)
{
	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo;
	tpd_fw_cdev.tp_suspend_show = nt36xx_tp_suspend_show;
	tpd_fw_cdev.set_tp_suspend = nt36xx_set_tp_suspend;
	tpd_fw_cdev.headset_state_show = nt36xx_headset_state_show;
	tpd_fw_cdev.set_headset_state = nt36xx_set_headset_state;
	tpd_fw_cdev.set_display_rotation = nt36xx_set_display_rotation;
	tpd_fw_cdev.set_edge_limit_level =  nt36xx_set_edge_limit_level;
	tpd_fw_cdev.get_gesture = tpd_get_wakegesture;
	tpd_fw_cdev.wake_gesture = tpd_enable_wakegesture;
	tpd_fw_cdev.tpd_suspend_need_awake = tpd_suspend_need_awake;
	tpd_fw_cdev.tpd_test_set_cmd = tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = tpd_test_cmd_show;
	tpd_fw_cdev.get_noise = nt36xx_tpd_get_noise;
	ts->edge_limit_level = 1;
	ts->gesture = 0;
	ts->gesture_enter = 0;
#ifdef NVT_REPORT_BY_ZTE_ALGO
	tpd_fw_cdev.max_x = TOUCH_DEFAULT_MAX_WIDTH;
	tpd_fw_cdev.max_y = TOUCH_DEFAULT_MAX_HEIGHT;
	tpd_fw_cdev.edge_report_limit[0] = nvt_left_edge_limit_v;
	tpd_fw_cdev.edge_report_limit[1] = nvt_right_edge_limit_v;
	tpd_fw_cdev.edge_report_limit[2] = nvt_left_edge_limit_h;
	tpd_fw_cdev.edge_report_limit[3] = nvt_right_edge_limit_h;
	tpd_fw_cdev.long_pess_suppression[0] = nvt_left_edge_long_pess_v;
	tpd_fw_cdev.long_pess_suppression[1] = nvt_right_edge_long_pess_v;
	tpd_fw_cdev.long_pess_suppression[2] = nvt_left_edge_long_pess_h;
	tpd_fw_cdev.long_pess_suppression[3] = nvt_right_edge_long_pess_h;
	tpd_fw_cdev.long_press_max_count = nvt_long_press_max_count;
	tpd_fw_cdev.edge_long_press_check = nvt_edge_long_press_check;
#endif
	nt36xx_get_fw();
	tpd_init_tpinfo(&tpd_fw_cdev);
	ts->nt36xx_ts_workqueue = create_singlethread_workqueue("novatek ts workqueue");
	if (!ts->nt36xx_ts_workqueue) {
		NVT_LOG(" nt36xx ts workqueue failed\n");
	} else  {
		INIT_DELAYED_WORK(&ts->charger_work, nt36xx_work_charger_detect_work);
		INIT_WORK(&ts->resume_work, novatek_resume_work);
		queue_delayed_work(ts->nt36xx_ts_workqueue, &ts->charger_work, msecs_to_jiffies(1000));
		nt36xx_init_charger_notifier();
	}

	return 0;
}

