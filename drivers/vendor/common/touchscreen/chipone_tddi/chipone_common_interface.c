#define LOG_TAG "Com"

#include <linux/kernel.h>
#include "cts_core.h"
#include "cts_config.h"
#include "cts_selftest.h"
#include "chipone_common.h"
#include "cts_platform.h"
#include "cts_firmware.h"

char ini_file_path[MAX_FILE_PATH_LEN] = { 0 };
char ini_file_name[MAX_FILE_NAME_LEN] = { 0 };
char save_file_path[MAX_FILE_PATH_LEN] = { 0 };
char save_test_info_file_name[MAX_FILE_NAME_LEN] = { 0 };
char save_test_result_file_name[MAX_FILE_NAME_LEN] = { 0 };
char cts_vendor_name[MAX_NAME_LEN_20] = { 0 };
char cts_firmware_name[MAX_FILE_NAME_LEN] = { 0 };
char *cts_test_failed_node_buffer = NULL;
char *cts_test_temp_buffer = NULL;
u8 *cts_test_failed_node = NULL;
int cts_test_faied_buffer_length = 0;
int cts_test_failed_count = 0;

int cts_test_result = 0;
extern void set_tp_suspend(struct chipone_ts_data *cts_data, bool enable);
extern int cts_suspend(struct chipone_ts_data *cts_data);

struct tpvendor_t chipone_vendor_l[] = {
	{VENDOR_ID1, CTS_VENDOR1_NAME},
	{VENDOR_ID2, CTS_VENDOR2_NAME},
	{VENDOR_ID3, CTS_VENDOR3_NAME},
	{VENDOR_ID4, CTS_VENDOR4_NAME},
	{VENDOR_END, "Unknown"},
};

static int cts_get_chip_vendor(u8 vendor_id)
{
	int i = 0;
	const char *panel_name = NULL;

	for (i = 0; i < ARRAY_SIZE(chipone_vendor_l); i++) {
		if (chipone_vendor_l[i].vendor_id == vendor_id) {
			strlcpy(cts_vendor_name, chipone_vendor_l[i].vendor_name, sizeof(cts_vendor_name));
			snprintf(cts_firmware_name, sizeof(cts_firmware_name),
				 "chipone_firmware_%s.bin", cts_vendor_name);
			return 0;
		}
	}
	panel_name = get_lcd_panel_name();
	for (i = 0; i < ARRAY_SIZE(chipone_vendor_l); i++) {
		if (strnstr(panel_name, chipone_vendor_l[i].vendor_name, strlen(panel_name))) {
			strlcpy(cts_vendor_name, chipone_vendor_l[i].vendor_name, sizeof(cts_vendor_name));
			snprintf(cts_firmware_name, sizeof(cts_firmware_name),
				 "chipone_firmware_%s.bin", cts_vendor_name);
			return 0;
		}
	}
	strlcpy(cts_vendor_name, "Unknown", sizeof(cts_vendor_name));
	snprintf(cts_firmware_name, sizeof(cts_firmware_name),
		 "chipone_firmware_%s.bin", cts_vendor_name);
	return -EIO;
}

static int tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	u8 vendor_id1 = 0, vendor_id2 = 0, vendor_id3 = 0, access_flag = 0;
	struct cts_device *cts_dev;
	int ret = 0;
	u16 device_fw_ver = 0;

	cts_dev = (struct cts_device *)cdev->private;
	if (cts_dev->rtdata.suspended) {
		cts_err("cts tp in suspned");
		return -EIO;
	}
	ret = cts_hw_reg_readb(cts_dev, 0x3002C, &access_flag);
	if (ret) {
		cts_err("Read display access flag failed %d", ret);
		return ret;
	}

	ret = cts_hw_reg_writeb(cts_dev, CTS_CHIPID_CONCTROL_REG, access_flag | 0x01);
	if (ret) {
		cts_err("Write display access flag %02x failed %d", access_flag, ret);
		return ret;
	}
	cts_hw_reg_readb(cts_dev, CTS_VENDORID1, &vendor_id1);
	cts_hw_reg_readb(cts_dev, CTS_VENDORID2, &vendor_id2);
	cts_hw_reg_readb(cts_dev, CTS_VENDORID3, &vendor_id3);

	cts_info("Read driver id: 0x%02x, 0x%02x, 0x%02x", vendor_id1, vendor_id2, vendor_id3);

	ret = cts_hw_reg_writeb(cts_dev, CTS_CHIPID_CONCTROL_REG, access_flag);
	if (ret) {
		cts_err("Write display access flag %02x failed %d", access_flag, ret);
		return ret;
	}
	ret = cts_fw_reg_readw_retry(cts_dev, CTS_DEVICE_FW_REG_VERSION, &device_fw_ver, 5, 0);
	if (ret) {
		cts_err("Read firmware version failed %d", ret);
		device_fw_ver = 0;
	} else {
		device_fw_ver = be16_to_cpu(device_fw_ver);
		cts_info("Device firmware version: %04x", device_fw_ver);
	}
	cts_get_chip_vendor(vendor_id1);
	snprintf(cdev->ic_tpinfo.tp_name, sizeof(cdev->ic_tpinfo.tp_name), "chipone");
	strlcpy(cdev->ic_tpinfo.vendor_name, cts_vendor_name, sizeof(cdev->ic_tpinfo.vendor_name));
	cdev->ic_tpinfo.module_id = vendor_id1;
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_CHIPONE;
	cdev->ic_tpinfo.chip_ver = 0;
	cdev->ic_tpinfo.firmware_ver = device_fw_ver;
	cdev->ic_tpinfo.i2c_type = 0;
	cdev->ic_tpinfo.i2c_addr = cts_dev->rtdata.i2c_addr;
	return 0;
}

#ifdef CONFIG_CTS_GESTURE
static int tpd_get_wakegesture(struct tpd_classdev_t *cdev)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;

	cdev->b_gesture_enable = cts_is_gesture_wakeup_enabled(cts_dev);

	return 0;
}

static int tpd_enable_wakegesture(struct tpd_classdev_t *cdev, int enable)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	if (cts_dev->rtdata.suspended) {
		cdev->tp_suspend_write_gesture = true;
	}

	if (enable) {
		cts_enable_gesture_wakeup(cts_dev);
	} else {
		cts_disable_gesture_wakeup(cts_dev);
	}
	return enable;
}
#endif

static bool cts_suspend_need_awake(struct tpd_classdev_t *cdev)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;

#ifdef CONFIG_CTS_GESTURE
	if (!cdev->tp_suspend_write_gesture &&
		(cts_dev->rtdata.updating || cts_dev->rtdata.gesture_wakeup_enabled)) {
		cts_info("tp suspend need awake.\n");
		return true;
	}
#else
	if (cts_dev->rtdata.updating) {
		cts_info("tp suspend need awake.\n");
		return true;
	}
#endif
	else {
		cdev->tp_suspend_write_gesture = false;
		cts_info("tp suspend dont need awake.\n");
		return false;
	}
}


static int cts_i2c_reg_read(struct tpd_classdev_t *cdev, u32 addr, u8 *data, int len)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	return cts_fw_reg_readsb(cts_dev, addr, data, len);
}

static int cts_i2c_reg_write(struct tpd_classdev_t *cdev, u32 addr, u8 *data, int len)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	return cts_fw_reg_writesb(cts_dev, addr, data, len);
}

static int cts_tp_fw_upgrade(struct tpd_classdev_t *cdev, char *fw_name, int fwname_len)
{
	struct cts_device *cts_dev = (struct cts_device *)cdev->private;
	const struct cts_firmware *firmware;
	char fwname[MAX_FILE_NAME_LEN] = { 0 };
	bool to_flash = true;
	int ret;

	if ((fwname_len <= 1) || (fwname_len >= MAX_FILE_NAME_LEN)) {
		cts_err("fw bin name's length(%d) fail", fwname_len);
		return -EINVAL;
	}
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, sizeof(fwname), "%s", fw_name);
	fwname[fwname_len - 1] = '\0';
	cts_info("Update firmware from file '%s'", fwname);

	firmware = cts_request_firmware_from_fs(cts_dev, fwname);
	if (firmware) {
		ret = cts_stop_device(cts_dev);
		if (ret) {
			cts_err("Stop device failed %d", ret);
			cts_release_firmware(firmware);
			return ret;
		}

		ret = cts_update_firmware(cts_dev, firmware, to_flash);
		if (ret) {
			cts_err("Update firmware failed %d", ret);
		}

		ret = cts_start_device(cts_dev);
		if (ret) {
			cts_err("Start device failed %d", ret);
			cts_release_firmware(firmware);
			return ret;
		}

		cts_release_firmware(firmware);
	} else {
		cts_err("Request firmware from file '%s' failed", fw_name);
		return -ENOENT;
	}

	return 0;
}

static int cts_tp_suspend_show(struct tpd_classdev_t *cdev)
{
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	cdev->tp_suspend = cts_is_device_suspended(cts_dev);
	return cdev->tp_suspend;
}

static int cts_set_tp_suspend(struct tpd_classdev_t *cdev, u8 suspend_node, int enable)
{
	struct cts_device *cts_dev;
	struct chipone_ts_data *cts_data = NULL;

	cts_dev = (struct cts_device *)cdev->private;
	cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);
	if (suspend_node == PROC_SUSPEND_NODE) {
		set_tp_suspend(cts_data, enable);
	} else {
		if (enable) {
			cts_suspend(cts_data);
		} else {
			queue_work(cts_data->workqueue, &cts_data->ts_resume_work);
		}
	}

	return 0;
}

static int cts_print_data2buffer(char *buff_arry[], unsigned int cols, unsigned int rows,
		 s16 *frame_data_words, int idex, int idx, char *name)
{
	int count = 0;
	unsigned int x = 0;
	unsigned int y = 0;

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

static int cts_data_request(struct tpd_classdev_t *cdev,
	s16 *frame_data_words, enum tp_test_type  test_type)
{
	int  ret = 0;
	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int col = 0;
	unsigned int row = 0;
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	row = cts_dev->fwdata.rows;
	col = cts_dev->fwdata.cols;
	ret = cts_enable_get_rawdata(cts_dev);
	if (ret) {
		cts_err("Enable read raw data failed %d\n", ret);
		goto out;
	}

	ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
	if (ret) {
		cts_err("Send cmd QUIT_GESTURE_MONITOR failed %d\n", ret);
		goto out;
	}
	msleep(50);
	switch (test_type) {
	case RAWDATA_TEST:
		ret = cts_get_rawdata(cts_dev, frame_data_words);
		if (ret) {
			cts_err("Get raw data failed %d\n", ret);
			goto out;
		}
		break;
	case DELTA_TEST:
		ret = cts_get_diffdata(cts_dev, frame_data_words);
		if (ret) {
			cts_err("Get diff data failed %d\n", ret);
			goto out;
		}
		break;
	default:
		cts_err("%s:the Para is error!\n", __func__);
		ret = -1;
		goto out;
	}

	ret = cts_disable_get_rawdata(cts_dev);
	if (ret) {
		cts_err("Disable read raw data failed %d\n", ret);
		goto out;
	}
	for (y = 0; y < row; y++) {
		pr_cont("CTP[%2d]", (y + 1));
		for (x = 0; x < col; x++) {
			pr_cont("%5d,", frame_data_words[y * col + x]);
		}
		pr_cont("\n");
	}
out:
	return ret;
}

static int  cts_testing_delta_raw_report(struct tpd_classdev_t *cdev,
	char *buff_arry[], unsigned int num_of_reports)
{

	s16 *frame_data_words = NULL;
	unsigned int col = 0;
	unsigned int row = 0;
	unsigned int idx = 0, idex = 0;
	int retval = 0;
	struct cts_device *cts_dev;

	if (cdev->tp_suspend) {
		cts_info("In suspend, no test, return now");
		return -EINVAL;
	}
	cts_dev = (struct cts_device *)cdev->private;

	row = cts_dev->fwdata.rows;
	col = cts_dev->fwdata.cols;
	cts_plat_disable_irq(cts_dev->pdata);
	frame_data_words = kcalloc((row * col), sizeof(s16), GFP_KERNEL);
	if (frame_data_words ==  NULL) {
		cts_err("Failed to allocate frame_data_words mem\n");
		retval = -1;
		goto MEM_ALLOC_FAILED;
	}
	for (idx = 0; idx < num_of_reports; idx++) {
		retval = cts_data_request(cdev, frame_data_words, RAWDATA_TEST);
		if (retval < 0) {
			cts_err("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}

		idex = idx << 1;
		retval = cts_print_data2buffer(buff_arry, col, row,  frame_data_words, idex, idx, "Rawdata");
		if (retval <= 0) {
			cts_err("print_data2buffer rawdata failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		retval = cts_data_request(cdev, frame_data_words, DELTA_TEST);
		if (retval < 0) {
			cts_err("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		idex += 1;
		retval = cts_print_data2buffer(buff_arry, col, row,  frame_data_words, idex, idx, "Delta");
		if (retval <= 0) {
			cts_err("print_data2buffer Delta failed!\n");
			goto DATA_REQUEST_FAILED;
		}
	}

	retval = 0;
	msleep(20);
	cts_info("get tp delta raw data end!\n");
DATA_REQUEST_FAILED:
	kfree(frame_data_words);
	frame_data_words = NULL;
MEM_ALLOC_FAILED:
	cts_plat_enable_irq(cts_dev->pdata);
	return retval;
}

static int cts_tpd_get_noise(struct tpd_classdev_t *cdev, struct list_head *head)
{
	int retval;
	int i = 0;
	char *buf_arry[RT_DATA_NUM];
	struct tp_runtime_data *tp_rt;
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	if (cts_dev->rtdata.suspended)
		return -EIO;

	list_for_each_entry(tp_rt, head, list) {
		buf_arry[i++] = tp_rt->rt_data;
		tp_rt->is_empty = false;
	}
	retval = cts_testing_delta_raw_report(cdev, buf_arry, RT_DATA_NUM >> 2);
	if (retval < 0) {
		cts_err("%s: get_raw_noise failed!\n",  __func__);
		return retval;
	}
	return 0;
}

static int cts_test_buffer_init(struct cts_device *cts_dev)
{
	cts_info("%s:enter\n", __func__);
	cts_test_failed_node_buffer = kzalloc((TP_NODE_NUMBER * TEST_TEMP_LENGTH), GFP_KERNEL);
	cts_test_temp_buffer = kzalloc(TEST_TEMP_LENGTH, GFP_KERNEL);
	cts_test_failed_node = kzalloc(TP_NODE_NUMBER, GFP_KERNEL);
	if (cts_test_failed_node_buffer == NULL || cts_test_temp_buffer == NULL ||
		cts_test_failed_node == NULL) {
		kfree(cts_test_failed_node_buffer);
		kfree(cts_test_temp_buffer);
		kfree(cts_test_failed_node);
		cts_err("%s:alloc memory failde!\n", __func__);
		return -ENOMEM;
	}
	cts_test_faied_buffer_length = 0;
	cts_test_failed_count = 0;
	cts_test_result = 0;
	return 0;
}

static void cts_test_buffer_free(void)
{
	cts_info("%s:enter\n", __func__);
	kfree(cts_test_failed_node_buffer);
	kfree(cts_test_temp_buffer);
	kfree(cts_test_failed_node);
}

void cts_clean_test_buffer(struct cts_device *cts_dev)
{
	memset(cts_test_failed_node_buffer, 0, (TP_NODE_NUMBER * TEST_TEMP_LENGTH));
	memset(cts_test_failed_node, 0, TP_NODE_NUMBER);
	cts_test_faied_buffer_length = 0;
	cts_test_failed_count = 0;
}

static int cts_save_failed_node_to_buffer(struct cts_device *cts_dev, char *tmp_buffer, int length)
{

	if (cts_test_failed_node_buffer == NULL) {
		cts_err("warning:cts_test_failed_node_buffer is null.");
		return -EPERM;
	}

	cts_test_faied_buffer_length +=
		snprintf(cts_test_failed_node_buffer + cts_test_faied_buffer_length,
		 ((TP_NODE_NUMBER * TEST_TEMP_LENGTH) - cts_test_faied_buffer_length), tmp_buffer);
	cts_test_failed_count++;

	return 0;
}

int cts_save_failed_node(struct cts_device *cts_dev, int failed_node)
{
	int i_len = 0;
	int tx = 0;
	int rx = 0;

	tx = failed_node / cts_dev->fwdata.rows;
	rx = failed_node % cts_dev->fwdata.rows;
	if (cts_test_failed_node == NULL)
		return -EPERM;
	if (cts_test_failed_node[failed_node] == 0) {
		if (cts_test_temp_buffer != NULL) {
			i_len = snprintf(cts_test_temp_buffer, TEST_TEMP_LENGTH, ",%d,%d", tx, rx);
			cts_save_failed_node_to_buffer(cts_dev, cts_test_temp_buffer, i_len);
			cts_test_failed_node[failed_node] = 1;
			return 0;
		} else {
			return -EPERM;
		}
	} else {
		return 0;
	}
}

static int tpd_test_save_file_path_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(save_file_path, 0, sizeof(save_file_path));
	snprintf(save_file_path, sizeof(save_file_path), "%s", buf);
	cts_info("set save_file_path:%s", save_file_path);
	return 0;
}

static int tpd_test_save_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", save_file_path);
}

static int tpd_test_save_file_name_store(struct tpd_classdev_t *cdev, const char *buf)
{

	memset(save_test_result_file_name, 0, sizeof(save_test_result_file_name));
	snprintf(save_test_result_file_name, sizeof(save_test_result_file_name), "%s.txt", buf);
	memset(save_test_info_file_name, 0, sizeof(save_test_info_file_name));
	snprintf(save_test_info_file_name, sizeof(save_test_info_file_name), "%s.csv", buf);
	cts_info("set save_test_result_file_name: %s", save_test_result_file_name);
	cts_info("set save_test_info_file_name: %s", save_test_info_file_name);
	return 0;
}

static int tpd_test_save_file_name_show(struct tpd_classdev_t *cdev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", save_test_result_file_name);
}

static int tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int i_len = 0;
	struct cts_device *cts_dev;

	cts_dev = (struct cts_device *)cdev->private;
	cts_info("%s:enter\n", __func__);
	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", cts_test_result, cts_dev->fwdata.cols,
		cts_dev->fwdata.rows, cts_test_failed_count);
	cts_info("tpd test result:%d && rawdata node failed count:%d.\n", cts_test_result, cts_test_failed_count);

	if (cts_test_failed_node_buffer != NULL) {
		i_len += snprintf(buf + i_len, PAGE_SIZE - i_len, cts_test_failed_node_buffer);
	}
	cts_info("tpd  test:%s.\n", buf);
	num_read_chars = i_len;
	return num_read_chars;
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	unsigned long command = 0;
	int ret;
	struct cts_device *cts_dev;

	if (cdev->tp_suspend) {
		cts_info("In suspend, no test, return now");
		return -EINVAL;
	}
	cts_dev = (struct cts_device *)cdev->private;
	ret = kstrtoul(buf, 10, &command);
	if (ret) {
		cts_err("invalid param:%s", buf);
		return -EIO;
	}

	if (command == TP_TEST_INIT) {
		ret = cts_init_selftest(cts_dev);
		if (ret) {
			cts_err("cts init selftest err");
			return ret;
		}
		ret = cts_test_buffer_init(cts_dev);
		if (ret) {
			cts_err("cts test buffer init err");
			cts_deinit_selftest(cts_dev);
			return ret;
		}
	} else if (command == TP_TEST_START) {
		msleep(200);
		cts_plat_disable_irq(cts_dev->pdata);
		ret = cts_start_selftest(cts_dev);
		cts_plat_enable_irq(cts_dev->pdata);
		cts_plat_release_all_touch(cts_dev->pdata);
		if (ret & (1 << FIRMWARE_VERSION_TEST_CODE))
			cts_test_result = cts_test_result | TEST_VERSION_ERR;
		if (ret & (1 << RAWDATA_TEST_CODE))
			cts_test_result = cts_test_result | TEST_BEYOND_MAX_LIMIT
						      | TEST_BEYOND_MIN_LIMIT;
		if (ret & (1 << OPEN_CIRCUITE_TEST_CODE))
			cts_test_result = cts_test_result | TEST_GT_OPEN;
		if (ret & (1 << SHORT_CIRCUITE_TEST_CODE))
			cts_test_result = cts_test_result | TEST_GT_SHORT;

	} else if (command == TP_TEST_END) {
		cts_deinit_selftest(cts_dev);
		cts_test_buffer_free();
	}
	return 0;
}

#ifdef CFG_CTS_HEADSET_DETECT
static int cts_headset_state_show(struct tpd_classdev_t *cdev)
{
	struct cts_device *cts_dev = (struct cts_device *)cdev->private;
	struct chipone_ts_data *cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);

	cdev->headset_state = cts_data->headset_mode;
	return cdev->headset_state;
}

static int cts_set_headset_state(struct tpd_classdev_t *cdev, int enable)
{
	struct cts_device *cts_dev = (struct cts_device *)cdev->private;
	struct chipone_ts_data *cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);

	cts_data->headset_mode = enable;
	cts_info("%s: headset_state = %d.\n", __func__, cts_data->headset_mode);
	if (!cts_dev->rtdata.suspended) {
		if (cts_data->headset_mode) {
			cts_send_command(&cts_data->cts_dev, CTS_CMD_EP_PLUG_IN);
		} else {
			cts_send_command(&cts_data->cts_dev, CTS_CMD_EP_PLUG_OUT);
		}
	}
	return cts_data->headset_mode;
}
#endif

#ifdef CFG_CTS_ROTATION
static int cts_set_display_rotation(struct tpd_classdev_t *cdev, int mrotation)
{
	int ret = 0;
	struct cts_device *cts_dev = (struct cts_device *)cdev->private;

	cdev->display_rotation = mrotation;
	cts_info("%s: display_rotation = %d.\n", __func__, cdev->display_rotation);
	if (cts_dev->rtdata.suspended)
		return 0;
	switch (cdev->display_rotation) {
	case mRotatin_0:
		ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_CMD, 0xFE);
		break;
	case mRotatin_90:
		ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_CMD, 0xFD);
		break;
	case mRotatin_180:
		ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_CMD, 0xFE);
		break;
	case mRotatin_270:
		ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_CMD, 0xFC);
		break;
	default:
		break;
	}
	if (ret < 0) {
		cts_err("%s write display_rotation fail", __func__);
	}
	return cdev->display_rotation;
}
#endif

#ifdef CFG_CTS_EDGE_LIMIT
static int cts_set_edge_limit_level(struct tpd_classdev_t *cdev, u8 level)
{
	int ret = 0;
	struct cts_device *cts_dev = (struct cts_device *)cdev->private;
	struct chipone_ts_data *cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);

	if (level > 8)
		level = 8;
	cts_data->edge_limit_level = level;
	cts_info("%s: edge limit level = %d.\n", __func__, level);
	if (cts_dev->rtdata.suspended)
		return 0;
	ret = cts_fw_reg_writeb(cts_dev, CTS_DEVICE_FW_REG_EDGE_PARAM, level);
	if (ret < 0) {
		cts_err("%s write edge limit level fail", __func__);
		return -EIO;
	}
	return level;
}
#endif

int cts_register_fw_class(struct cts_device *cts_dev)
{
#ifdef CFG_CTS_EDGE_LIMIT
	struct chipone_ts_data *cts_data = container_of(cts_dev, struct chipone_ts_data, cts_dev);
#endif

	tpd_fw_cdev.private = (void *)cts_dev;
	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo;
#ifdef CONFIG_CTS_GESTURE
	tpd_fw_cdev.get_gesture = tpd_get_wakegesture;
	tpd_fw_cdev.wake_gesture = tpd_enable_wakegesture;
#endif
#ifdef CFG_CTS_HEADSET_DETECT
	tpd_fw_cdev.headset_state_show = cts_headset_state_show;
	tpd_fw_cdev.set_headset_state = cts_set_headset_state;
#endif
#ifdef CFG_CTS_EDGE_LIMIT
	tpd_fw_cdev.set_edge_limit_level = cts_set_edge_limit_level;
	cts_data->edge_limit_level = 0;
#endif
#ifdef CTS_REPORT_BY_ZTE_ALGO
	tpd_fw_cdev.max_x = cts_dev->pdata->res_x;
	tpd_fw_cdev.max_y = cts_dev->pdata->res_y;
	tpd_fw_cdev.edge_report_limit[0] = cts_left_edge_limit_v;
	tpd_fw_cdev.edge_report_limit[1] = cts_right_edge_limit_v;
	tpd_fw_cdev.edge_report_limit[2] = cts_left_edge_limit_h;
	tpd_fw_cdev.edge_report_limit[3] = cts_right_edge_limit_h;
	tpd_fw_cdev.long_pess_suppression[0] = cts_left_edge_long_pess_v;
	tpd_fw_cdev.long_pess_suppression[1] = cts_right_edge_long_pess_v;
	tpd_fw_cdev.long_pess_suppression[2] = cts_left_edge_long_pess_h;
	tpd_fw_cdev.long_pess_suppression[3] = cts_right_edge_long_pess_h;
	tpd_fw_cdev.long_press_max_count = cts_long_press_max_count;
	tpd_fw_cdev.edge_long_press_check = cts_edge_long_press_check;
#endif
#ifdef CFG_CTS_ROTATION
	tpd_fw_cdev.set_display_rotation = cts_set_display_rotation;
#endif
	tpd_fw_cdev.reg_char_num = REG_CHAR_NUM_4;
	tpd_fw_cdev.tp_i2c_16bor32b_reg_read = cts_i2c_reg_read;
	tpd_fw_cdev.tp_i2c_16bor32b_reg_write = cts_i2c_reg_write;
	tpd_fw_cdev.tp_fw_upgrade = cts_tp_fw_upgrade;
	tpd_fw_cdev.tp_suspend_show = cts_tp_suspend_show;
	tpd_fw_cdev.set_tp_suspend = cts_set_tp_suspend;
	tpd_fw_cdev.tpd_suspend_need_awake = cts_suspend_need_awake;
	tpd_fw_cdev.get_noise = cts_tpd_get_noise;

	tpd_fw_cdev.tpd_test_set_save_filepath = tpd_test_save_file_path_store;
	tpd_fw_cdev.tpd_test_get_save_filepath = tpd_test_save_file_path_show;
	tpd_fw_cdev.tpd_test_set_save_filename = tpd_test_save_file_name_store;
	tpd_fw_cdev.tpd_test_get_save_filename = tpd_test_save_file_name_show;
	tpd_fw_cdev.tpd_test_set_cmd = tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = tpd_test_cmd_show;
	tpd_init_tpinfo(&tpd_fw_cdev);

	strlcpy(ini_file_path, DEFAULT_INI_FILE_PATH, sizeof(ini_file_path));
	strlcpy(save_file_path, DEFAULT_SAVE_FILE_PATH, sizeof(ini_file_path));
	strlcpy(save_test_result_file_name, DEFAULT_RESULT_FILE_NAME, sizeof(save_test_result_file_name));
	strlcpy(save_test_info_file_name, DEFAULT_INFO_FILE_NAME, sizeof(save_test_info_file_name));
	snprintf(ini_file_name, sizeof(ini_file_name), "cts_test_sensor_%s.cfg", cts_vendor_name);

	return 0;
}
