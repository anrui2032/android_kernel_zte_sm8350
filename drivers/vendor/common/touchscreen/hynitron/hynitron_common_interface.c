#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include "tpd_sys.h"
#include  "hynitron.h"

#define MAX_ALLOC_BUFF 256
#define TEST_PASS	0
#define TEST_BEYOND_MAX_LIMIT		0x0001
#define TEST_BEYOND_MIN_LIMIT		0x0002
#define TP_TEST_INIT		1
#define TP_TEST_START	2
#define TP_TEST_END		3
#define MAX_NAME_LEN_50  50
#define MAX_NAME_LEN_20  20

COMMON_INTERFACE_INFO g_itfs_ver_info;
char hyn_self_test_cfg_name[MAX_NAME_LEN_50] = {0};

char *g_cst8xx_sensor_result_file = NULL;
unsigned char *cst8xx_test_result_node = NULL;
char *cst8xx_file_path = NULL;
int cst8xx_test_failed_count = 0;
int cst8xx_tptest_result = 0;
char cst8xx_save_file_path[MAX_NAME_LEN_50] = { 0 };
char cst8xx_save_file_name[MAX_NAME_LEN_50] = { 0 };

extern int cst8xx_get_all_version(void);
extern int hyn_i2c_wr_pkt(char addr, u8 *data, int len);
extern int hyn_i2c_rd_pkt(char addr, u8 *data, int len);
extern int cst8xx_adb_update(u8 *dir, int name_len);
extern unsigned char get_suspend_flag(void);
extern int test_cst8xx_suspend(void);
extern int test_cst8xx_resume(void);
extern int hynitron_cst8xx_self_test(void);
extern int hyn_get_module_id(void);

static int cst8xx_tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	if (cst8xx_get_all_version() < 0) {
		cdev->ic_tpinfo.firmware_ver = 0x00;
		return -EIO;
	}
	snprintf(cdev->ic_tpinfo.tp_name, sizeof(cdev->ic_tpinfo.tp_name), "hynitron_cst836u");
	snprintf(cdev->ic_tpinfo.vendor_name, sizeof(cdev->ic_tpinfo.vendor_name), g_itfs_ver_info.pmodule_name);
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_HYNITRON;
	cdev->ic_tpinfo.chip_part_id = 0x13;
	cdev->ic_tpinfo.module_id	= g_itfs_ver_info.module_id;
	cdev->ic_tpinfo.chip_ver	 = 0x13;
	cdev->ic_tpinfo.firmware_ver = g_itfs_ver_info.fw_ver;
	cdev->ic_tpinfo.display_ver  = g_itfs_ver_info.lcd_id;
	cdev->ic_tpinfo.i2c_type	 = 0;
	cdev->ic_tpinfo.i2c_addr	 = 0x15;
	return 0;
}

static int cst8xx_i2c_reg_read(struct tpd_classdev_t *cdev, char addr, u8 *data, int len)
{
	if (hyn_i2c_rd_pkt(addr, data, len) < 0) {
		return -EIO;
	}
	return 0;
}

static int cst8xx_i2c_reg_write(struct tpd_classdev_t *cdev, char addr, u8 *data, int len)
{
	return hyn_i2c_wr_pkt(addr, data, len);
}

static int cst8xx_tp_fw_upgrade(struct tpd_classdev_t *cdev, char *fw_name, int fwname_len)
{
	char fileName[128] = { 0 };

	memset(fileName, 0, sizeof(fileName));
	snprintf(fileName, sizeof(fileName), "%s", fw_name);
	fileName[fwname_len - 1] = '\0';
	hyn_info("%s upgrade from file(%s) start!\n", __func__, fileName);
	return cst8xx_adb_update(fileName, fwname_len);
}


static int cst8xx_gpio_shutdown_config(void)
{
	return 0;
}

static int cst8xx_test_init(void)
{

	hyn_info("%s\n", __func__);

	cst8xx_test_result_node = kzalloc(HYN_MAX_NODE_SIZE, GFP_KERNEL);
	cst8xx_file_path = kzalloc(MAX_ALLOC_BUFF, GFP_KERNEL);
	if (cst8xx_test_result_node != NULL) {
		memset(cst8xx_test_result_node, 0x00, HYN_MAX_NODE_SIZE);
	}

	if ((cst8xx_test_result_node == NULL) || (cst8xx_file_path == NULL)) {
		if (cst8xx_test_result_node != NULL) {
			kfree(cst8xx_test_result_node);
			cst8xx_test_result_node = NULL;
		}
		if (cst8xx_file_path != NULL) {
			kfree(cst8xx_file_path);
			cst8xx_file_path = NULL;
		}
		hyn_info("alloc memory failde!\n");
		return -ENOMEM;
	}
	cst8xx_tptest_result = 0;
	cst8xx_test_failed_count = 0;

	return 0;
}

static void cst8xx_test_buffer_free(void)
{
	hyn_info("%s\n", __func__);

	if (cst8xx_test_result_node != NULL) {
		kfree(cst8xx_test_result_node);
		cst8xx_test_result_node = NULL;
	}
	if (cst8xx_file_path != NULL) {
		kfree(cst8xx_file_path);
		cst8xx_file_path = NULL;
	}
}

static int cst8xx_tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	int val = 0;
	int retval = 0;
	unsigned long command = 0;

	hyn_info("%s enter\n", __func__);

	retval = kstrtoul(buf, 10, &command);
	if (retval) {
		hyn_err("invalid param:%s\n", buf);
		return -EIO;
	}

	if (command == TP_TEST_INIT) {
		retval = cst8xx_test_init();
		if (retval < 0) {
			hyn_err("alloc memory failde!\n");
			return -ENOMEM;
		}
	} else if (command == TP_TEST_START) {

		hyn_info("%s:start TP test.\n", __func__);

		if (cst8xx_file_path != NULL) {
			snprintf(cst8xx_file_path, MAX_ALLOC_BUFF, "%s%s", cst8xx_save_file_path,
				 cst8xx_save_file_name);
			g_cst8xx_sensor_result_file = cst8xx_file_path;
		}
		val = hynitron_cst8xx_self_test();
		if (val == 0x00) {
			cst8xx_tptest_result = TEST_PASS;
			hyn_info("Self_Test Pass\n");
		} else if (val >= 0x01 && val <= 0x03) {
			cst8xx_tptest_result = cst8xx_tptest_result | TEST_BEYOND_MAX_LIMIT | TEST_BEYOND_MIN_LIMIT;
			hyn_err("Self_Test Fail\n");
		} else {
			cst8xx_tptest_result = TEST_PASS;
			hyn_info("self test data init Fail\n");
		}

	} else if (command == TP_TEST_END) {
		cst8xx_test_buffer_free();
	} else {
		hyn_info("invalid command %ld", command);
	}

	return 0;
}

static int cst8xx_tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	int k;
	int i_len;
	char tmpbuf[8];
	ssize_t num_read_chars = 0;

	hyn_info("%s:enter\n", __func__);
	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", cst8xx_tptest_result, 0, (HYN_ALL_CHANNEL_NUM & 0xff),
			 cst8xx_test_failed_count);

	hyn_info("tpd test result:%d && rawdata node failed count:%d.\n", cst8xx_tptest_result,
		   cst8xx_test_failed_count);

	if (cst8xx_test_result_node != NULL) {
		for (k = 0; k < HYN_ALL_CHANNEL_NUM; k++) {
			if (cst8xx_test_result_node[k]) {
				snprintf(tmpbuf, 7, ",0,%d", k);
				i_len += snprintf(buf + i_len, PAGE_SIZE - i_len, tmpbuf);
			}
		}
		/* snprintf(tmpbuf,2,"\n"); */
		/* tmpbuf[2] = '\0'; */
		/* i_len += snprintf(buf+i_len,PAGE_SIZE - i_len, tmpbuf); */
	}

	hyn_info("tpd  test:i_len=%d,buf=%s.\n", i_len, buf);
	num_read_chars = i_len;
	return num_read_chars;
}

static int cst8xx_tpd_test_channel_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%d %d", 4, 9);

	return num_read_chars;
}

static int cst8xx_tpd_test_save_file_path_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(cst8xx_save_file_path, 0, sizeof(cst8xx_save_file_path));
	snprintf(cst8xx_save_file_path, sizeof(cst8xx_save_file_path), "%s", buf);
	hyn_info("save file path:%s.\n", cst8xx_save_file_path);
	return 0;
}

static int cst8xx_tpd_test_save_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", cst8xx_save_file_path);

	return num_read_chars;
}

static int cst8xx_tpd_test_save_file_name_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(cst8xx_save_file_name, 0, sizeof(cst8xx_save_file_name));
	snprintf(cst8xx_save_file_name, sizeof(cst8xx_save_file_name), "%s", buf);

	hyn_info("save file path:%s.\n", cst8xx_save_file_name);

	return 0;
}

static int cst8xx_tpd_test_save_file_name_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", cst8xx_save_file_name);

	return num_read_chars;
}

static int cst8xx_tp_suspend_show(struct tpd_classdev_t *cdev)
{
	cdev->tp_suspend = get_suspend_flag();
	return cdev->tp_suspend;
}

static int cst8xx_set_tp_suspend(struct tpd_classdev_t *cdev, u8 suspend_node, int enable)
{
	if (enable)
		test_cst8xx_suspend();
	else
		test_cst8xx_resume();
	cdev->tp_suspend = get_suspend_flag();

	return cdev->tp_suspend;
}

int cst8xx_save_failed_node(int failed_node)
{
	if (cst8xx_test_result_node == NULL) {
		return -EPERM;
	}
	if (cst8xx_test_result_node[failed_node] == 0) {
		cst8xx_test_failed_count++;
		cst8xx_test_result_node[failed_node] = 1;
	}
	return 0;
}

static void cst8xx_get_self_cfg_name(void)
{
	int id = 0;

	memset(hyn_self_test_cfg_name, 0, MAX_NAME_LEN_50);
	id = hyn_get_module_id();
	if (id < 0) {
		snprintf(hyn_self_test_cfg_name, sizeof(hyn_self_test_cfg_name),
			"hyn_cst836u.cfg");
	} else {
		snprintf(hyn_self_test_cfg_name, sizeof(hyn_self_test_cfg_name),
			"hyn_cst836u_%02x.cfg", id);
	}

	hyn_info("%s self_cfg_name %s", __func__, hyn_self_test_cfg_name);
}

void cst8xx_tpd_register_fw_class(void)
{
	hyn_info("%s\n", __func__);

	/* cst8xx_tpd_init_tpinfo(&tpd_fw_cdev); */

	cst8xx_test_result_node = NULL;
	cst8xx_get_self_cfg_name();

	tpd_fw_cdev.get_tpinfo		= cst8xx_tpd_init_tpinfo;
	tpd_fw_cdev.tp_i2c_reg_read   = cst8xx_i2c_reg_read;
	tpd_fw_cdev.tp_i2c_reg_write  = cst8xx_i2c_reg_write;
	tpd_fw_cdev.reg_char_num	  = REG_CHAR_NUM_2;
	tpd_fw_cdev.tp_fw_upgrade	 = cst8xx_tp_fw_upgrade;
	tpd_fw_cdev.tpd_gpio_shutdown = cst8xx_gpio_shutdown_config;
	tpd_fw_cdev.tp_suspend_show   = cst8xx_tp_suspend_show;
	tpd_fw_cdev.set_tp_suspend	= cst8xx_set_tp_suspend;

	tpd_fw_cdev.tpd_test_set_save_filepath = cst8xx_tpd_test_save_file_path_store;
	tpd_fw_cdev.tpd_test_get_save_filepath = cst8xx_tpd_test_save_file_path_show;
	tpd_fw_cdev.tpd_test_set_save_filename = cst8xx_tpd_test_save_file_name_store;
	tpd_fw_cdev.tpd_test_get_save_filename = cst8xx_tpd_test_save_file_name_show;
	tpd_fw_cdev.tpd_test_set_cmd = cst8xx_tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = cst8xx_tpd_test_cmd_show;
	tpd_fw_cdev.tpd_test_get_channel_info = cst8xx_tpd_test_channel_show;
	snprintf(cst8xx_save_file_path, sizeof(cst8xx_save_file_path), "%s", "/sdcard/");
	snprintf(cst8xx_save_file_name, sizeof(cst8xx_save_file_name), "%s", "cst8xx_test_result");
}

