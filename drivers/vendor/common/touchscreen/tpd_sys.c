/***********************
 * file : tpd_fw.c
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include "tpd_sys.h"

struct tpd_classdev_t tpd_fw_cdev;
struct tp_rwreg_t tp_reg_rw;
static struct class *tsp_fw_class;
struct proc_dir_entry *tpd_proc_dir = NULL;
LIST_HEAD(rt_data_head);

struct tpvendor_t synaptics_vendor_l[] = {
	{0x31, "TPK"},
	{0x32, "Truly"},
	{0x33, "Success"},
	{0x34, "Ofilm"},
	{0x35, "Lead"},
	{0x36, "Wintek"},
	{0x37, "Laibao"},
	{0x38, "CMI"},
	{0x39, "Ecw"},
	{0x41, "Goworld"},
	{0x42, "BaoMing"},
	{0x43, "Eachopto"},
	{0x44, "Mutto"},
	{0x45, "Junda"},
	{0x46, "BOE"},
	{0x47, "TianMa"},
	{0x48, "Samsung"},
	{0x49, "DiJing"},
	{0x50, "LCE"},
	{0x52, "HeLiTai"},
	{0x53, "JDI"},
	{0x54, "HuaXingDa"},
	{0x55, "Toptouch"},
	{0x56, "GVO"},
	{0x57, "Wally_panel"},
	{0x58, "zgsaihua"},
	{VENDOR_END, "Unknown"},
};

struct tpvendor_t focal_vendor_l[] = {
	{0x11, "TeMeiKe"},
	{0x15, "ChuangWei"},
	{0x51, "Ofilm"},
	{0x55, "LaiBao"},
	{0x57, "Goworld"},
	{0x5a, "Truly"},
	{0x5c, "TPK"},
	{0x5d, "BaoMing"},
	{0x5f, "Success"},
	{0x60, "Lead"},
	{0x67, "DiJing"},
	{0x69, "Huaxingda"},
	{0x80, "Eachopto"},
	{0x82, "HeLiTai"},
	{0x85, "JunDa"},
	{0x87, "LianChuang"},
	{0xda, "DiJingDA"},
	{0xf0, "TongXingDa"},
	{VENDOR_END, "Unknown"},
};

struct tpvendor_t cypress_vendor_l[] = {
	{0x01, "TPK"},
	{0x02, "Truly"},
	{0x03, "Success"},
	{0x04, "Ofilm"},
	{0x05, "Lead"},
	{0x06, "Wintek"},
	{0x07, "Laibao"},
	{0x08, "CMI"},
	{0x09, "Ecw"},
	{0x0a, "Goworld"},
	{0x0b, "BaoMing"},
	{0x0c, "Eachopto"},
	{0x0d, "Mutto"},
	{0x0e, "Junda"},
	{VENDOR_END, "Unknown"},
};

struct tpvendor_t atmel_vendor_l[] = {
	{0x06, "Wintek"},
	{0x07, "Laibao"},
	{0x08, "CMI"},
	{0x09, "Ecw"},
	{VENDOR_END, "Unknown"},
};

static int get_chip_vendor(struct tpvendor_t *vendor_l, int count, int vendor_id, char *vendor_name)
{
	int i = 0;
	int vendorname_len = 0;

	pr_info("%s: count: %d.\n", __func__, count);

	for (i = 0; i < count; i++) {
		if (vendor_l[i].vendor_id == vendor_id || VENDOR_END == vendor_l[i].vendor_id) {
			vendorname_len = strlen(vendor_l[i].vendor_name);
			vendorname_len = vendorname_len >= MAX_VENDOR_NAME_LEN ?
				MAX_VENDOR_NAME_LEN - 1 : vendorname_len;
			strlcpy(vendor_name,  vendor_l[i].vendor_name, vendorname_len + 1);
			break;
		}
	}

	return 0;
}

static void tpd_get_tp_module_name(struct tpd_classdev_t *cdev)
{
	int size = 0;

	if (cdev == NULL) {
		pr_err("tpd: %s cdev is NULL.\n", __func__);
		return;
	}
	switch (cdev->ic_tpinfo.chip_model_id) {
	case TS_CHIP_SYNAPTICS:
		size = ARRAY_SIZE(synaptics_vendor_l);
		get_chip_vendor(synaptics_vendor_l, size, cdev->ic_tpinfo.module_id, cdev->ic_tpinfo.vendor_name);
		break;
	case TS_CHIP_ATMEL:
		size = ARRAY_SIZE(atmel_vendor_l);
		get_chip_vendor(atmel_vendor_l, size, cdev->ic_tpinfo.module_id, cdev->ic_tpinfo.vendor_name);
		break;
	case TS_CHIP_CYTTSP:
		size =  ARRAY_SIZE(cypress_vendor_l);
		get_chip_vendor(cypress_vendor_l, size, cdev->ic_tpinfo.module_id, cdev->ic_tpinfo.vendor_name);
		break;
	case TS_CHIP_FOCAL:
		size = ARRAY_SIZE(focal_vendor_l);
		get_chip_vendor(focal_vendor_l, size, cdev->ic_tpinfo.module_id, cdev->ic_tpinfo.vendor_name);
		break;
	case TS_CHIP_GOODIX:
	case TS_CHIP_MELFAS:
	case TS_CHIP_MSTAR:
	case TS_CHIP_ILITEK:
	case TS_CHIP_TLSC:
		break;
	default:
		pr_info("fun:%s chip_model_id Unknown.\n", __func__);
		break;
	}
	pr_info("fun:%s module name:%s.\n", __func__, cdev->ic_tpinfo.vendor_name);
}

static ssize_t tsp_fw_ic_tpinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);

	if (cdev->get_tpinfo) {
		cdev->get_tpinfo(cdev);
	}

	return snprintf(buf, 64, "%u 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x %s\n",
		cdev->ic_tpinfo.chip_part_id,   cdev->ic_tpinfo.chip_model_id,
		cdev->ic_tpinfo.chip_ver,        cdev->ic_tpinfo.module_id,
		cdev->ic_tpinfo.firmware_ver, cdev->ic_tpinfo.config_ver,
		cdev->ic_tpinfo.i2c_type,        cdev->ic_tpinfo.i2c_addr,
		cdev->ic_tpinfo.tp_name);
}

static ssize_t tsp_fw_ic_tpinfo_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

/**for tpd test*/
static ssize_t tsp_test_save_file_path_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_save_filepath) {
		retval = cdev->tpd_test_get_save_filepath(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_save_file_path_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_save_filepath) {
		retval = cdev->tpd_test_set_save_filepath(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_save_file_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_save_filename) {
		retval = cdev->tpd_test_get_save_filename(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_save_file_name_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_save_filename) {
		retval = cdev->tpd_test_set_save_filename(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_ini_file_path_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_ini_filepath) {
		retval = cdev->tpd_test_get_ini_filepath(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_ini_file_path_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_ini_filepath) {
		retval = cdev->tpd_test_set_ini_filepath(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_filename_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_filename) {
		retval = cdev->tpd_test_get_filename(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_filename_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_filename) {
		retval = cdev->tpd_test_set_filename(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_cmd) {
		retval = cdev->tpd_test_get_cmd(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_cmd) {
		retval = cdev->tpd_test_set_cmd(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_node_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_node_data) {
		retval = cdev->tpd_test_get_node_data(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_node_data_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_node_data_type) {
		retval = cdev->tpd_test_set_node_data_type(cdev, buf);
	}

	return count;
}
static ssize_t tsp_test_channel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_channel_info) {
		retval = cdev->tpd_test_get_channel_info(cdev, buf);
	}

	return retval;
}
static ssize_t tsp_test_result_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_result) {
		retval = cdev->tpd_test_get_result(cdev, buf);
	}

	return retval;
}

#ifdef CONFIG_TP_BSC_CALIBRATION
static ssize_t tsp_test_bsc_calibration_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_get_bsc_calibration) {
		retval = cdev->tpd_test_get_bsc_calibration(cdev, buf);
	}

	return retval;
}

static ssize_t tsp_test_bsc_calibration_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpd_classdev_t *cdev = dev_get_drvdata(dev);
	int retval = 0;

	if (cdev->tpd_test_set_bsc_calibration) {
		retval = cdev->tpd_test_set_bsc_calibration(cdev, buf);
	}

	return count;
}

static ssize_t tsp_test_bsc_calibration_start_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int retval = 0;

	retval = snprintf(buf, PAGE_SIZE, "%s\n", "do not read this node!");

	return retval;
}
#endif

static DEVICE_ATTR(tpinfo, 0644, tsp_fw_ic_tpinfo_show, tsp_fw_ic_tpinfo_store);
/**for tpd test*/
static DEVICE_ATTR(tpd_test_result, S_IRUGO|S_IRUSR, tsp_test_result_show, NULL);
static DEVICE_ATTR(tpd_test_channel_setting, S_IRUGO|S_IRUSR, tsp_test_channel_show, NULL);
static DEVICE_ATTR(tpd_test_save_file_path, S_IRUGO|S_IWUSR,
						tsp_test_save_file_path_show, tsp_test_save_file_path_store);
static DEVICE_ATTR(tpd_test_save_file_name, S_IRUGO|S_IWUSR,
						tsp_test_save_file_name_show, tsp_test_save_file_name_store);
static DEVICE_ATTR(tpd_test_ini_file_path, S_IRUGO|S_IWUSR, tsp_test_ini_file_path_show, tsp_test_ini_file_path_store);
static DEVICE_ATTR(tpd_test_filename, S_IRUGO|S_IWUSR, tsp_test_filename_show, tsp_test_filename_store);
static DEVICE_ATTR(tpd_test_cmd, S_IRUGO|S_IWUSR, tsp_test_cmd_show, tsp_test_cmd_store);
static DEVICE_ATTR(tpd_test_node_data, S_IRUGO|S_IWUSR, tsp_test_node_data_show, tsp_test_node_data_store);
#ifdef CONFIG_TP_BSC_CALIBRATION
static DEVICE_ATTR(tpd_test_bsc_calibration, S_IRUGO|S_IWUSR, tsp_test_bsc_calibration_show, NULL);
static DEVICE_ATTR(tpd_test_bsc_calibration_start, S_IRUGO|S_IWUSR,
						tsp_test_bsc_calibration_start_show, tsp_test_bsc_calibration_store);
#endif

static struct attribute *tsp_dev_attrs[] = {
	&dev_attr_tpinfo.attr,
	/**for tpd test*/
	&dev_attr_tpd_test_filename.attr,
	&dev_attr_tpd_test_node_data.attr,
	&dev_attr_tpd_test_cmd.attr,
	&dev_attr_tpd_test_ini_file_path.attr,
	&dev_attr_tpd_test_save_file_path.attr,
	&dev_attr_tpd_test_save_file_name.attr,
	&dev_attr_tpd_test_channel_setting.attr,
	&dev_attr_tpd_test_result.attr,
#ifdef CONFIG_TP_BSC_CALIBRATION
	&dev_attr_tpd_test_bsc_calibration.attr,
	&dev_attr_tpd_test_bsc_calibration_start.attr,
#endif
	NULL,
};

static const struct attribute_group tsp_dev_attribute_group = {
	.attrs = tsp_dev_attrs,
	/**.bin_attrs = tsp_dev_bin_attributes,*/
};

static const struct attribute_group *tsp_dev_attribute_groups[] = {
	&tsp_dev_attribute_group,
	NULL,
};

static int tp_reg_read(
		u8 addr,
		u8 *data,
		int len)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;
	int ret = 0;

	if (cdev->tp_i2c_reg_read) {
		ret = cdev->tp_i2c_reg_read(cdev, addr, data, len);
	} else {
		ret = -EPERM;
	}
	return ret;
}
static int tp_reg_write(
		u8 addr,
		u8 *data,
		int len)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;
	int ret = 0;

	if (cdev->tp_i2c_reg_write) {
		ret = cdev->tp_i2c_reg_write(cdev, addr, data, len);
	} else {
		ret = -EPERM;
	}
	return ret;
}
static int tp_16bor32b_reg_read(
		u32 addr,
		u8 *data,
		int len)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;
	int ret = 0;

	if (cdev->tp_i2c_16bor32b_reg_read) {
		ret = cdev->tp_i2c_16bor32b_reg_read(cdev, addr, data, len);
	} else {
		ret = -EPERM;
	}
	return ret;
}
static int tp_16bor32b_reg_write(
		u32 addr,
		u8 *data,
		int len)
{
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;
	int ret = 0;

	if (cdev->tp_i2c_16bor32b_reg_write) {
		ret = cdev->tp_i2c_16bor32b_reg_write(cdev, addr, data, len);
	} else {
		ret = -EPERM;
	}
	return ret;
}
static int str_to_int(const char *hex_buf, int size)
{
	int i = 0;
	int base = 1;
	int value = 0;
	char single = 0;

	for (i = size - 1; i >= 0; i--) {
		single = hex_buf[i];

		if ((single >= '0') && (single <= '9')) {
			value += (single - '0') * base;
		} else if ((single >= 'a') && (single <= 'z')) {
			value += (single - 'a' + 10) * base;
		} else if ((single >= 'A') && (single <= 'Z')) {
			value += (single - 'A' + 10) * base;
		} else {
			return -EINVAL;
		}

		base *= 16;
	}

	return value;
}

static u8 str_to_u8(const char *hex_buf, int size)
{
	return (u8) str_to_int(hex_buf, size);
}

static int tp_parse_buf(const char *buf, size_t cmd_len)
{
	int length = 0;
	int i = 0;

	tp_reg_rw.reg = str_to_int(buf + 1, tpd_fw_cdev.reg_char_num);
	length = str_to_int(buf + 1 + tpd_fw_cdev.reg_char_num, 2);

	if (buf[0] == '1') {
		tp_reg_rw.len = length;
		tp_reg_rw.type = REG_OP_READ;
		pr_info("read %02X, %d bytes", tp_reg_rw.reg, tp_reg_rw.len);
	} else {
		if (cmd_len < (length * 2 + 3 + tpd_fw_cdev.reg_char_num)) {
			pr_err("data invalided!\n");
			return -EINVAL;
		}
		pr_info("write %02X, %d bytes", tp_reg_rw.reg, length);

		/* first byte is the register addr */
		tp_reg_rw.type = REG_OP_WRITE;
		tp_reg_rw.len = length;
	}

	if (tp_reg_rw.len > 0) {
		tp_reg_rw.opbuf = kzalloc(tp_reg_rw.len, GFP_KERNEL);
		if (tp_reg_rw.opbuf == NULL) {
			pr_err("allocate memory failed!\n");
			return -ENOMEM;
		}

		if (tp_reg_rw.type == REG_OP_WRITE) {
			pr_info("write buffer:\n");
			for (i = 0; i < tp_reg_rw.len; i++) {
				tp_reg_rw.opbuf[i] = str_to_u8(buf + 3 + tpd_fw_cdev.reg_char_num + i * 2, 2);
				pr_info("buf[%d]: %02X\n", i, tp_reg_rw.opbuf[i] & 0xFF);
			}
		}
	}

	return tp_reg_rw.len;
}

static ssize_t tprwreg_show(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	int i = 0;
	int length = 0;
	uint8_t buf[500] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	mutex_lock(&cdev->cmd_mutex);

	if (tp_reg_rw.len < 0) {
		count = snprintf(buf, sizeof(buf), "Invalid cmd line\n");
	} else if (tp_reg_rw.len == 1) {
		if (tp_reg_rw.type == REG_OP_READ) {
			if (tp_reg_rw.res == 0) {
				count = snprintf(buf, sizeof(buf), "Read reg 0x%02X: 0x%02X\n",
					tp_reg_rw.reg, tp_reg_rw.val);
			} else {
				count =
				    snprintf(buf, sizeof(buf), "Read reg 0x%02X failed, ret: %d\n", tp_reg_rw.reg,
					     tp_reg_rw.res);
			}
		} else {
			if (tp_reg_rw.res == 0) {
				count =
				    snprintf(buf, sizeof(buf), "Write reg 0x%02X, 0x%02X success\n", tp_reg_rw.reg,
					     tp_reg_rw.val);
			} else {
				count =
				    snprintf(buf, sizeof(buf), "Write reg 0x%02X failed, ret: %d\n", tp_reg_rw.reg,
					     tp_reg_rw.res);
			}
		}
	} else {
		if (tp_reg_rw.type == REG_OP_READ) {
			length = tp_reg_rw.len - 1;
			length = (length < 0) ? 0 : length;
			count =
			    snprintf(buf, sizeof(buf), "Read Reg: [0x%02X]-[0x%02X]\n", tp_reg_rw.reg,
				     tp_reg_rw.reg + length);
			count += snprintf(buf + count, sizeof(buf) - count, "Result: ");
			if (tp_reg_rw.res) {
				count += snprintf(buf + count, sizeof(buf) - count, "failed, ret: %d\n", tp_reg_rw.res);
			} else {
				if (tp_reg_rw.opbuf) {
					for (i = 0; i < tp_reg_rw.len; i++) {
						count += snprintf(buf + count, sizeof(buf) - count, "0x%02X ",
							tp_reg_rw.opbuf[i]);
					}
					count += snprintf(buf + count, sizeof(buf) - count, "\n");
				}
			}
		} else {
			length = tp_reg_rw.len - 1;
			length = (length < 0) ? 0 : length;
			count =
			    snprintf(buf, sizeof(buf), "Write Reg: [0x%02X]-[0x%02X]\n", tp_reg_rw.reg,
				     tp_reg_rw.reg + length);
			count += snprintf(buf + count, sizeof(buf) - count, "Write Data: ");
			if (tp_reg_rw.opbuf) {
				for (i = 0; i < tp_reg_rw.len; i++) {
					count += snprintf(buf + count, sizeof(buf) - count, "0x%02X ",
						tp_reg_rw.opbuf[i]);
				}
				count += snprintf(buf + count, sizeof(buf) - count, "\n");
			}
			if (tp_reg_rw.res) {
				count += snprintf(buf + count, sizeof(buf) - count, "Result: failed, ret: %d\n",
						tp_reg_rw.res);
			} else {
				count += snprintf(buf + count, sizeof(buf) - count, "Result: success\n");
			}
		}
	}
	if (tp_reg_rw.opbuf != NULL) {
		kfree(tp_reg_rw.opbuf);
		tp_reg_rw.opbuf = NULL;
	}
	mutex_unlock(&cdev->cmd_mutex);

	return simple_read_from_buffer(buffer, count, offset, buf, count);
}

/********************************************************************************************
 * Format buf:
 * [0]: '0' write, '1' read
 * [1-2]: addr, hex
 * [3-4]: length, hex
 * [5-6]...[n-(n+1)]: data, hex
 * read reg 00:echo 00 > rw_reg
 * write reg 00 value 01:echo 0001 > rw_reg
 * read reg [00 -09] :echo 1000A > rw_reg
 * write reg [00 -09] value 00,01,02,03,04,05,06,07,08,09 :echo 0000A00010203040506070809 > rw_reg
 ******************************************************************************************/
static ssize_t tprwreg_store(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	size_t cmd_length = 0;
	int ret = 0;
	char buf[500] = {0};
	u8 reg = 0, val = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	mutex_lock(&cdev->cmd_mutex);
	cmd_length = len - 1;

	if (tp_reg_rw.opbuf != NULL) {
		kfree(tp_reg_rw.opbuf);
		tp_reg_rw.opbuf = NULL;
	}
	if (cmd_length > sizeof(buf))
		goto INVALID_CMD;
	ret = copy_from_user(buf, buffer, cmd_length);

	pr_info("cmd len: %d, buf: %s.\n", (int)cmd_length, buf);
	/* compatible old ops */
	if (cmd_length == tpd_fw_cdev.reg_char_num) {
		tp_reg_rw.type = REG_OP_READ;
		tp_reg_rw.len = 1;
		tp_reg_rw.reg = str_to_int(buf, tpd_fw_cdev.reg_char_num);
	} else if (cmd_length == 2 + tpd_fw_cdev.reg_char_num) {
		tp_reg_rw.type = REG_OP_WRITE;
		tp_reg_rw.len = 1;
		tp_reg_rw.reg = str_to_int(buf, tpd_fw_cdev.reg_char_num);
		tp_reg_rw.val = str_to_int(buf + tpd_fw_cdev.reg_char_num, 2);

	} else if (cmd_length < 3 + tpd_fw_cdev.reg_char_num) {
		pr_err("Invalid cmd buffer.\n");
		goto INVALID_CMD;
	} else {
		tp_reg_rw.len = tp_parse_buf(buf, cmd_length);
	}
	if (tp_reg_rw.len < 0) {
		pr_err("cmd buffer error!\n");

	} else {
		if (tp_reg_rw.type == REG_OP_READ) {
			if (tp_reg_rw.len == 1) {
				reg = tp_reg_rw.reg & 0xFF;
				if (tpd_fw_cdev.reg_char_num == REG_CHAR_NUM_2) {
					tp_reg_rw.res = tp_reg_read(reg, &val, 1);
				} else {
					tp_reg_rw.res = tp_16bor32b_reg_read(tp_reg_rw.reg, &val, 1);
				}
				tp_reg_rw.val = val;
			} else {
				reg = tp_reg_rw.reg & 0xFF;
				if (tpd_fw_cdev.reg_char_num == REG_CHAR_NUM_2) {
					tp_reg_rw.res = tp_reg_read(reg, tp_reg_rw.opbuf, tp_reg_rw.len);
				} else {
					tp_reg_rw.res =
						tp_16bor32b_reg_read(tp_reg_rw.reg, tp_reg_rw.opbuf, tp_reg_rw.len);
				}
			}

			if (tp_reg_rw.res < 0) {
				pr_err("Could not read 0x%02x.\n", tp_reg_rw.reg);
			} else {
				pr_info("read 0x%02x, %d bytes successful.\n", tp_reg_rw.reg, tp_reg_rw.len);
				tp_reg_rw.res = 0;
			}

		} else {
			if (tp_reg_rw.len == 1) {
				reg = tp_reg_rw.reg & 0xFF;
				val = tp_reg_rw.val & 0xFF;
				if (tpd_fw_cdev.reg_char_num == REG_CHAR_NUM_2) {
					tp_reg_rw.res = tp_reg_write(reg, &val, 1);
				} else {
					tp_reg_rw.res = tp_16bor32b_reg_write(tp_reg_rw.reg, &val, 1);
				}
			} else {
				reg = tp_reg_rw.reg & 0xFF;
				if (tpd_fw_cdev.reg_char_num == REG_CHAR_NUM_2) {
					tp_reg_rw.res =  tp_reg_write(reg, tp_reg_rw.opbuf, tp_reg_rw.len);
				} else {
					tp_reg_rw.res =
						tp_16bor32b_reg_write(tp_reg_rw.reg, tp_reg_rw.opbuf, tp_reg_rw.len);
				}
			}
			if (tp_reg_rw.res < 0) {
				pr_err("Could not write 0x%02x.\n", tp_reg_rw.reg);

			} else {
				pr_info("Write 0x%02x, %d bytes successful.\n", tp_reg_rw.reg, tp_reg_rw.len);
				tp_reg_rw.res = 0;
			}
		}
	}
	mutex_unlock(&cdev->cmd_mutex);

	return len;
INVALID_CMD:
	mutex_unlock(&cdev->cmd_mutex);
	return -EINVAL;
}

static ssize_t tp_rawdata_read(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	char *buf = NULL;
	int len = 0;
	int ret = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	mutex_lock(&cdev->cmd_mutex);
	buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("tpd: %s kzalloc failed\n", __func__);
		mutex_unlock(&cdev->cmd_mutex);
		return -ENOMEM;
	}
	if (cdev->tpd_test_get_tp_rawdata) {
		len = cdev->tpd_test_get_tp_rawdata(buf, PAGE_SIZE);
	}
	pr_notice("tpd: tp_rawdata_read:%d\n", len);
	ret = simple_read_from_buffer(buffer, count, offset, buf, len);
	kfree(buf);
	mutex_unlock(&cdev->cmd_mutex);
	return ret;
}

static ssize_t tp_module_info_read(struct file *file,
	char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t buffer_tpd[200];
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->get_tpinfo) {
	cdev->get_tpinfo(cdev);
	}
	tpd_get_tp_module_name(cdev);
	len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "TP module: %s(0x%x)\n",
			cdev->ic_tpinfo.vendor_name, cdev->ic_tpinfo.module_id);
	len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "IC type : %s\n",
			cdev->ic_tpinfo.tp_name);
	len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "I2C address: 0x%x\n",
			cdev->ic_tpinfo.i2c_addr);
	len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "Firmware version : 0x%x\n",
			cdev->ic_tpinfo.firmware_ver);
	if (cdev->ic_tpinfo.config_ver)
		len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "Config version:0x%x\n",
			cdev->ic_tpinfo.config_ver);
	if (cdev->ic_tpinfo.display_ver)
		len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "Display version:0x%x\n",
			cdev->ic_tpinfo.display_ver);
	if (cdev->ic_tpinfo.chip_batch[0])
		len += snprintf(buffer_tpd + len, sizeof(buffer_tpd) - len, "Chip hard version:%s\n",
			cdev->ic_tpinfo.chip_batch);
	return simple_read_from_buffer(buffer, count, offset, buffer_tpd, len);
}

static ssize_t tp_wake_gesture_read(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->get_gesture) {
		cdev->get_gesture(cdev);
	}
	pr_notice("tpd: %s val:%d.\n", __func__, cdev->b_gesture_enable);

	len = snprintf(data_buf, sizeof(data_buf), "%u\n", cdev->b_gesture_enable);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t tp_wake_gesture_write(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	ret = kstrtouint_from_user(buffer, len, 10, &input);
	if (ret)
		return -EINVAL;
	input = input > 0 ? 1 : 0;
	pr_notice("tpd: %s val %d.\n", __func__, input);
	if (cdev->wake_gesture) {
		cdev->wake_gesture(cdev, input);
	}
	return len;
}
static ssize_t tp_smart_cover_read(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->get_smart_cover) {
		cdev->get_smart_cover(cdev);
	}
	pr_notice("tpd: %s val:%d.\n", __func__, cdev->b_smart_cover_enable);
	len = snprintf(data_buf, sizeof(data_buf), "%u\n", cdev->b_smart_cover_enable);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t tp_smart_cover_write(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	ret = kstrtouint_from_user(buffer, len, 10, &input);
	if (ret)
		return -EINVAL;
	input = input > 0 ? 1 : 0;
	pr_notice("tpd: %s val %d.\n", __func__, input);
	if (cdev->set_smart_cover) {
		cdev->set_smart_cover(cdev, input);
	}
	return len;
}
static ssize_t tp_glove_read(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->get_glove_mode) {
		cdev->get_glove_mode(cdev);
	}
	pr_notice("tpd: %s val:%d.\n", __func__, cdev->b_glove_enable);
	len = snprintf(data_buf, sizeof(data_buf), "%u\n", cdev->b_glove_enable);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t tp_glove_write(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	ret = kstrtouint_from_user(buffer, len, 10, &input);
	if (ret)
		return -EINVAL;
	input = input > 0 ? 1 : 0;
	pr_notice("tpd: %s val %d.\n", __func__, input);
	if (cdev->set_glove_mode) {
		cdev->set_glove_mode(cdev, input);
	}
	return len;
}
static ssize_t tpfwupgrade_store(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	char *fwname = NULL;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	fwname = kmalloc(len, GFP_KERNEL);
	if (fwname == NULL) {
		pr_err("tpd: %s kmalloc failed\n", __func__);
		return -ENOMEM;
	}
	memset(fwname, 0, len);
	ret = copy_from_user(fwname, buffer, len);
	if (ret) {
		kfree(fwname);
		return -EINVAL;
	}
	pr_notice("%s, fwname: %s.\n", __func__, fwname);
	if (cdev->tp_fw_upgrade) {
		cdev->tp_fw_upgrade(cdev, fwname, len);
	}
	kfree(fwname);
	return len;
}

static ssize_t suspend_show(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[30] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->tp_suspend_show) {
		cdev->tp_suspend_show(cdev);
	}
	pr_notice("tpd: %s val:%d.\n", __func__, cdev->tp_suspend);
	len = snprintf(data_buf, sizeof(data_buf), "tp suspend is: %u\n", cdev->tp_suspend);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t suspend_store(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	ret = kstrtouint_from_user(buffer, len, 10, &input);
	if (ret)
		return -EINVAL;
	input = input > 0 ? 1 : 0;
	pr_notice("tpd: %s val %d.\n", __func__, input);

	mutex_lock(&cdev->cmd_mutex);
	if (cdev->sys_set_tp_suspend_flag == input) {
		pr_notice("tpd: %s tp state don't need change.\n", __func__);
		mutex_unlock(&cdev->cmd_mutex);
		return len;
	}
	cdev->sys_set_tp_suspend_flag = input;
	if (cdev->set_tp_suspend) {
		cdev->set_tp_suspend(cdev, PROC_SUSPEND_NODE, input);
	}
	mutex_unlock(&cdev->cmd_mutex);
	return len;
}

static ssize_t tp_switch_store(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	ret = kstrtouint_from_user(buffer, len, 10, &input);
	if (ret)
		return -EINVAL;

	input = input > 0 ? 1 : 0;
	pr_notice("tpd: %s val = %d\n", __func__, input);

	if (cdev->set_tp_state) {
		cdev->set_tp_state(cdev, input);
	}

	return len;
}

static ssize_t tp_single_tap_read(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0)
		return 0;

	if (cdev->get_singletap)
		cdev->get_singletap(cdev);

	pr_notice("tpd: %s val: %d.\n", __func__, cdev->b_single_tap_enable);
	len = snprintf(data_buf, sizeof(data_buf), "%u\n", cdev->b_single_tap_enable);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t tp_single_tap_write(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	ret = kstrtouint_from_user(buffer, len, 10, &input);
	if (ret)
		return -EINVAL;

	input = input > 0 ? 5 : 0;
	pr_notice("tpd: %s val = %d\n", __func__, input);

	if (cdev->set_singletap)
		cdev->set_singletap(cdev, input);

	return len;
}

static ssize_t tp_edge_report_limit_read(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[200] = {0};
	int i = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0)
		return 0;
	len += snprintf(data_buf + len, sizeof(data_buf) - len,
		"pixel limit level:%5u", cdev->edge_limit_pixel_level);
	len += snprintf(data_buf + len, sizeof(data_buf) - len,
		"\n edge limit level:%5u", cdev->edge_limit_level);
	len += snprintf(data_buf + len, sizeof(data_buf) - len, "\n edge limit width:");
	for (i = 0; i < MAX_LIMIT_NOM; i++) {
		if (len >= (sizeof(data_buf) - 5))
			break;
		len += snprintf(data_buf + len, sizeof(data_buf) - len, "%5u", cdev->edge_report_limit[i]);
	}
	len += snprintf(data_buf + len, sizeof(data_buf) - len, "\n long pess suppression width:");
	for (i = 0; i < MAX_LIMIT_NOM; i++) {
		if (len >= (sizeof(data_buf) - 5))
			break;
		len += snprintf(data_buf + len, sizeof(data_buf) - len, "%5u", cdev->long_pess_suppression[i]);
	}
	len += snprintf(data_buf + len, sizeof(data_buf) - len, "\n");
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t tp_edge_report_limit_write(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	u8 *token = NULL;
	char *cur = NULL;
	char buff[100] = { 0 };
	u16 count = 0;
	unsigned int  s_to_u8 = 0;
	unsigned int input = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	len = (len < sizeof(buff)) ? len : sizeof(buff);
	if (buffer != NULL) {
		if (copy_from_user(buff, buffer, len)) {
			pr_err("Failed to copy data from user space\n");
			len = -EINVAL;
			goto out;
		}
	}
	if (buff[0] == 's' || buff[0] == 'S') {
		ret = kstrtouint(&buff[2], 10, &s_to_u8);
		if (ret == 0) {
			cdev->edge_limit_level = s_to_u8;
			pr_info("edge_limit_level = %d\n", cdev->edge_limit_level);
			if (cdev->set_edge_limit_level != NULL)
				cdev->set_edge_limit_level(cdev, cdev->edge_limit_level);
		}
	} else if (buff[0] == 'w' || buff[0] == 'W') {
		cur = &buff[2];
		while ((token = strsep(&cur, ",")) != NULL) {
			ret = kstrtouint(token, 10, &s_to_u8);
			if (ret == 0) {
				cdev->edge_report_limit[count] = s_to_u8;
				pr_info("edge_report_limit[%d] = %d\n", count, cdev->edge_report_limit[count]);
				count++;
			}
			if (count >= MAX_LIMIT_NOM)
				break;
		}
	} else if (buff[0] == 'l' || buff[0] == 'L') {
		cur = &buff[2];
		while ((token = strsep(&cur, ",")) != NULL) {
			ret = kstrtouint(token, 10, &s_to_u8);
			if (ret == 0) {
				cdev->long_pess_suppression[count] = s_to_u8;
				pr_info("long_pess_suppression[%d] = %d\n", count, cdev->long_pess_suppression[count]);
				count++;
			}
			if (count >= MAX_LIMIT_NOM)
				break;
		}
	}  else {
		ret = kstrtouint_from_user(buffer, len, 10, &input);
		if (ret || input > 10)
			return -EINVAL;
		cdev->edge_limit_pixel_level = input;
		/* user set level  0-5: x_max x 1% increase
		     user set level  6-10:  x_max x 0.5% increase
		*/
		if (cdev->edge_limit_pixel_level <= 5)
			cdev->user_edge_limit[0] = cdev->max_x * cdev->edge_limit_pixel_level  / 100;
		else
			cdev->user_edge_limit[0] = cdev->max_x * 5  / 100
				+ (cdev->max_x  / 200) * (cdev->edge_limit_pixel_level - 5);
		if (cdev->max_y >= EDGE_LIMIT_PIXEL_HEIGHT)			
			cdev->user_edge_limit[1] = cdev->max_y - EDGE_LIMIT_PIXEL_HEIGHT;
		else
			cdev->user_edge_limit[1] = 0;
		pr_info("tpd: edge_limit_pixel_level = %d, limit[0,1] = [%d,%d]\n",
			cdev->edge_limit_pixel_level, cdev->user_edge_limit[0], cdev->user_edge_limit[1]);
	}
out:
	return len;
}

static ssize_t get_one_key(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->get_one_key) {
		cdev->get_one_key(cdev);
	}
	pr_notice("tpd: %s val:%d.\n", __func__, cdev->one_key_enable);
	len = snprintf(data_buf, sizeof(data_buf), "%u\n", cdev->one_key_enable);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t set_one_key(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	ret = kstrtouint_from_user(buffer, len, 10, &input);
	if (ret)
		return -EINVAL;

	input = input > 0 ? 1 : 0;

	pr_notice("tpd: %s val = %d\n", __func__, input);

	if (cdev->set_one_key) {
		cdev->set_one_key(cdev, input);
	}

	return len;
}

static ssize_t get_play_game(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->get_play_game) {
		cdev->get_play_game(cdev);
	}
	pr_notice("tpd: %s val:%d.\n", __func__, cdev->play_game_enable);
	len = snprintf(data_buf, sizeof(data_buf), "%u\n", cdev->play_game_enable);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t set_play_game(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	ret = kstrtouint_from_user(buffer, len, 10, &input);
	if (ret)
		return -EINVAL;

	input = input > 0 ? 1 : 0;

	pr_notice("tpd: %s val = %d\n", __func__, input);

	if (cdev->set_play_game) {
		cdev->set_play_game(cdev, input);
	}

	return len;
}

static void free_rt_data(void)
{
	struct tp_runtime_data *tmp, *tp_rt;

	 list_for_each_entry_safe(tp_rt, tmp, &rt_data_head, list) {
		 list_del(&tp_rt->list);
		 kfree(tp_rt->rt_data);
		 kfree(tp_rt);
	 }
}

static int list_init(void)
{
	int i;
	int retval = -1;
	struct tp_runtime_data *tmp;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	for (i = 0; i < RT_DATA_NUM; i++) {
		tmp = kmalloc(sizeof(struct tp_runtime_data), GFP_KERNEL);
		if (!tmp) {
			pr_err("tpd: %s struct kmalloc is failed!\n", __func__);
			goto STRUCT_FAILED;
		}

		tmp->rt_data = kzalloc(RT_DATA_LEN, GFP_KERNEL);
		if (!tmp->rt_data) {
			pr_err("tpd: %s data kmalloc is failed!\n", __func__);
			goto DATA_FAILED;
		}

		tmp->is_empty = true;

		list_add_tail(&(tmp->list), &rt_data_head);
	}

	if (cdev->get_noise)
		retval = cdev->get_noise(cdev, &rt_data_head);
	if (retval != 0) {
		pr_err("tpd: %s get_noise failed!\n", __func__);
		goto STRUCT_FAILED;
	}

	return 0;

DATA_FAILED:
	kfree(tmp);
	tmp = NULL;
STRUCT_FAILED:
	free_rt_data();

	return -EINVAL;
}

static void *get_noise_seq_start(struct seq_file *s, loff_t *pos)
{
	int retval;

	if (*pos == 0) {
		retval = list_init();
		if (retval < 0) {
			pr_err("%s: list_init failed!\n", __func__);
			return NULL;
		}
	}
	return seq_list_start(&rt_data_head, *pos);
}

static void *get_noise_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return seq_list_next(v, &rt_data_head, pos);
}

static void get_noise_seq_stop(struct seq_file *seq, void *v)
{
	if (v == NULL)
		free_rt_data();
}

static int get_noise_seq_show(struct seq_file *seq, void *v)
{
	struct tp_runtime_data *tmp;

	tmp = list_entry(v, struct tp_runtime_data, list);
	if (tmp->is_empty)
		return 1;

	seq_printf(seq, "%s\n", tmp->rt_data);

	return 0;
}

static const struct seq_operations get_noise_proc_ops = {
	.start	= get_noise_seq_start,
	.next	= get_noise_seq_next,
	.stop	= get_noise_seq_stop,
	.show	= get_noise_seq_show,
};

static int get_noise_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &get_noise_proc_ops);
}


static ssize_t headset_state_show(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[30] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}
	if (cdev->headset_state_show) {
		cdev->headset_state_show(cdev);
	}
	pr_notice("tpd: %s val:%d.\n", __func__, cdev->headset_state);
	len = snprintf(data_buf, sizeof(data_buf), "headset state: %u\n", cdev->headset_state);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t headset_state_store(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	char data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	len = len >= sizeof(data_buf) ? sizeof(data_buf) - 1 : len;
	ret = copy_from_user(data_buf, buffer, len);
	if (ret)
		return -EINVAL;
	ret = kstrtouint(data_buf, 0, &input);
	if (ret)
		return -EINVAL;
	input = input > 0 ? 1 : 0;
	pr_notice("headset_state: %s val %d.\n", __func__, input);
	if (cdev->set_headset_state) {
		cdev->set_headset_state(cdev, input);
	}
	return len;
}

static ssize_t display_rotation_show(struct file *file,
					 char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[30] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0) {
		return 0;
	}

	pr_notice("tpd: %s val:%d.\n", __func__, cdev->display_rotation);
	len = snprintf(data_buf, sizeof(data_buf), "display rotation: %d\n", cdev->display_rotation);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t set_display_rotation(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	char data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	len = len >= sizeof(data_buf) ? sizeof(data_buf) - 1 : len;
	ret = copy_from_user(data_buf, buffer, len);
	if (ret)
		return -EINVAL;
	ret = kstrtouint(data_buf, 0, &input);
	if (ret)
		return -EINVAL;
	cdev->display_rotation = input;
	pr_notice("display rotation: %s val %d.\n", __func__, cdev->display_rotation);
	if (cdev->set_display_rotation) {
		cdev->set_display_rotation(cdev, input);
	}
	return len;
}

#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
static ssize_t tp_get_screen_state(struct file *file,
		char __user *buffer, size_t count, loff_t *offset)
{
	ssize_t len = 0;
	uint8_t data_buf[10] = {0};
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	if (*offset != 0)
		return 0;

	if (cdev->get_screen_state)
		cdev->get_screen_state(cdev);

	pr_notice("tpd: %s val: %d.\n", __func__, cdev->screen_is_on);
	len = snprintf(data_buf, sizeof(data_buf), "%u\n", cdev->screen_is_on);
	return simple_read_from_buffer(buffer, count, offset, data_buf, len);
}

static ssize_t tp_set_screen_state(struct file *file,
				const char __user *buffer, size_t len, loff_t *off)
{
	int ret = 0;
	unsigned int input = 0;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

	ret = kstrtouint_from_user(buffer, len, 10, &input);
	if (ret)
		return -EINVAL;

	input = (input == 2) ? 1 : 0;

	if (cdev->set_screen_state)
		cdev->set_screen_state(cdev, input);

	return len;
}
#endif

static const struct file_operations proc_ops_tp_module_Info = {
	.owner = THIS_MODULE,
	.read = tp_module_info_read,
};
static const struct file_operations proc_ops_wake_gesture = {
	.owner = THIS_MODULE,
	.read = tp_wake_gesture_read,
	.write = tp_wake_gesture_write,
};
static const struct file_operations proc_ops_smart_cover = {
	.owner = THIS_MODULE,
	.read = tp_smart_cover_read,
	.write = tp_smart_cover_write,
};

static const struct file_operations proc_ops_glove = {
	.owner = THIS_MODULE,
	.read = tp_glove_read,
	.write = tp_glove_write,
};

static const struct file_operations proc_ops_rwreg = {
	.owner = THIS_MODULE,
	.read = tprwreg_show,
	.write = tprwreg_store,
};

static const struct file_operations proc_ops_tpfwupgrade = {
	.owner = THIS_MODULE,
	.write = tpfwupgrade_store,
};

static const struct file_operations proc_ops_tprawdata_read = {
	.owner = THIS_MODULE,
	.read = tp_rawdata_read,
};

static const struct file_operations proc_ops_suspend = {
	.owner = THIS_MODULE,
	.read = suspend_show,
	.write = suspend_store,
};

static const struct file_operations proc_ops_headset_state = {
	.owner = THIS_MODULE,
	.read = headset_state_show,
	.write = headset_state_store,
};

static const struct file_operations proc_ops_mrotation = {
	.owner = THIS_MODULE,
	.read = display_rotation_show,
	.write = set_display_rotation,
};

static const struct file_operations proc_ops_switch = {
	.owner = THIS_MODULE,
	.write = tp_switch_store,
};

static const struct file_operations proc_ops_single_tap = {
	.owner = THIS_MODULE,
	.read = tp_single_tap_read,
	.write = tp_single_tap_write,
};

static const struct file_operations proc_ops_get_noise = {
	.owner = THIS_MODULE,
	.open = get_noise_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static const struct file_operations proc_ops_edge_report_limit = {
	.owner = THIS_MODULE,
	.read = tp_edge_report_limit_read,
	.write = tp_edge_report_limit_write,
};

#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
static const struct file_operations proc_ops_screen_state = {
	.owner = THIS_MODULE,
	.read = tp_get_screen_state,
	.write = tp_set_screen_state,
};
#endif

static const struct file_operations proc_ops_onekey = {
	.owner = THIS_MODULE,
	.read = get_one_key,
	.write = set_one_key,
};

static const struct file_operations proc_ops_playgame = {
	.owner = THIS_MODULE,
	.read = get_play_game,
	.write = set_play_game,
};

static void create_tpd_proc_entry(void)
{
	struct proc_dir_entry *tpd_proc_entry = NULL;

	tpd_proc_dir = proc_mkdir(PROC_TOUCH_DIR, NULL);
	if (tpd_proc_dir == NULL) {
		pr_err("%s: mkdir touchscreen failed!\n",  __func__);
		return;
	}
	tpd_proc_entry = proc_create(PROC_TOUCH_INFO, 0664, tpd_proc_dir, &proc_ops_tp_module_Info);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create ts_information failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_WAKE_GESTURE, 0664,  tpd_proc_dir, &proc_ops_wake_gesture);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create wake_gesture failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_SMART_COVER, 0664, tpd_proc_dir, &proc_ops_smart_cover);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create smart_cover failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_GLOVE, 0664, tpd_proc_dir, &proc_ops_glove);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create glove mode failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_RW_REG, 0664,  tpd_proc_dir, &proc_ops_rwreg);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create rw_reg failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_FW_UPGRADE, 0664,  tpd_proc_dir, &proc_ops_tpfwupgrade);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create FW_upgrade failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_GET_TPRAWDATA, 0664,  tpd_proc_dir, &proc_ops_tprawdata_read);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create tprawdata failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_SUSPEND, 0664,  tpd_proc_dir, &proc_ops_suspend);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create suspend failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_HEADSET_STATE, 0664,  tpd_proc_dir, &proc_ops_headset_state);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create headset_state failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_MROTATION, 0664,  tpd_proc_dir, &proc_ops_mrotation);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create mRotation failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_TP_SWITCH, 0664, tpd_proc_dir, &proc_ops_switch);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create tp_switch failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_TP_SINGLETAP, 0664, tpd_proc_dir, &proc_ops_single_tap);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create single_tap failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_GET_NOISE, 0664, tpd_proc_dir, &proc_ops_get_noise);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create get_noise failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_EDGE_REPORT_LIMIT, 0664, tpd_proc_dir, &proc_ops_edge_report_limit);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create get_noise failed!\n");
#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
		tpd_proc_entry = proc_create(PROC_TOUCH_SCREEN_STATE, 0664, tpd_proc_dir, &proc_ops_screen_state);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create screen_state_interface failed!\n");
#endif
	tpd_proc_entry = proc_create(PROC_TOUCH_ONEKEY, 0664,  tpd_proc_dir, &proc_ops_onekey);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create one_key failed!\n");
	tpd_proc_entry = proc_create(PROC_TOUCH_PLAY_GAME, 0664,  tpd_proc_dir, &proc_ops_playgame);
	if (tpd_proc_entry == NULL)
		pr_err("proc_create play_game failed!\n");
}

void tpd_proc_deinit(void)
{
	if (tpd_proc_dir == NULL) {
		pr_err("%s: proc/touchscreen is NULL!\n",  __func__);
		return;
	}
	remove_proc_entry(PROC_TOUCH_INFO, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_WAKE_GESTURE, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_SMART_COVER, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_GLOVE, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_RW_REG, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_FW_UPGRADE, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_GET_TPRAWDATA, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_SUSPEND, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_HEADSET_STATE, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_MROTATION, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_TP_SWITCH, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_TP_SINGLETAP, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_GET_NOISE, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_EDGE_REPORT_LIMIT, tpd_proc_dir);
#ifdef CONFIG_SCREEN_ON_TEMP_SUPPORT
	remove_proc_entry(PROC_TOUCH_SCREEN_STATE, tpd_proc_dir);
#endif
	remove_proc_entry(PROC_TOUCH_ONEKEY, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_PLAY_GAME, tpd_proc_dir);
	remove_proc_entry(PROC_TOUCH_DIR, NULL);
	tpd_workquue_deinit();
}

static ssize_t ts_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct tpd_classdev_t *cdev = &tpd_fw_cdev;

#ifdef CONFIG_TPD_UFP_MAC
	return count;
#endif
	if (kstrtouint(buf, 10, &input))
		return -EINVAL;
	input = input > 0 ? 1 : 0;
	pr_notice("tpd: %s val %d.\n", __func__, input);

	mutex_lock(&cdev->cmd_mutex);
	if (cdev->sys_set_tp_suspend_flag == input) {
		pr_notice("tpd: %s tp state don't need change.\n", __func__);
		mutex_unlock(&cdev->cmd_mutex);
		return count;
	}
	cdev->sys_set_tp_suspend_flag = input;
	if (cdev->set_tp_suspend) {
		cdev->set_tp_suspend(cdev, SYS_SUSPEND_NODE, input);
	}
	mutex_unlock(&cdev->cmd_mutex);
	return count;
}

static DEVICE_ATTR(ts_suspend, 0664, NULL, ts_suspend_store);

static struct attribute *tp_sysfs_attrs[] = {
	&dev_attr_ts_suspend.attr,
	NULL,
};

static struct attribute_group tp_attr_group = {
		.attrs = tp_sysfs_attrs,
};

static int tp_sysfs_init(void)
{
	struct kobject *tp_ctrl_kobj = NULL;

	tp_ctrl_kobj = kobject_create_and_add("touchscreen", NULL);
	if (!tp_ctrl_kobj){
		pr_err("Create tp_sysfs_init failed!\n");
		return -ENOMEM;
	}
	return sysfs_create_group(tp_ctrl_kobj, &tp_attr_group);
}
/**
 * tpd_classdev_register - register a new object of tpd_classdev_t class.
 * @parent: The device to register.
 * @tsp_fw_cdev: the tpd_classdev_t structure for this device.
 */
int tpd_classdev_register(struct device *parent, struct tpd_classdev_t *tsp_fw_cdev)
{
	tsp_fw_cdev->dev = device_create(tsp_fw_class, NULL, 0, tsp_fw_cdev,
					  "%s", tsp_fw_cdev->name);
	if (IS_ERR(tsp_fw_cdev->dev))
		return PTR_ERR(tsp_fw_cdev->dev);

	mutex_init(&tsp_fw_cdev->cmd_mutex);

	/**tpd_create_wake_gesture_sysfs();*/
	create_tpd_proc_entry();
	tp_sysfs_init();
	pr_info("tpd: Registered tsp_fw device: %s\n",
			tsp_fw_cdev->name);

	return 0;
}
EXPORT_SYMBOL_GPL(tpd_classdev_register);

/**
 * tpd_classdev_unregister - unregisters a object of tsp_fw_properties class.
 * @tsp_fw_cdev: the tsp_fw device to unregister
 *
 * Unregisters a previously registered via tpd_classdev_register object.
 */
void tpd_classdev_unregister(struct tpd_classdev_t *tsp_fw_cdev)
{
	device_unregister(tsp_fw_cdev->dev);
	tpd_proc_deinit();
}
EXPORT_SYMBOL_GPL(tpd_classdev_unregister);

static int __init tpd_class_init(void)
{
	tsp_fw_class = class_create(THIS_MODULE, "tsp_fw");
	if (IS_ERR(tsp_fw_class))
		return PTR_ERR(tsp_fw_class);
	tsp_fw_class->dev_groups = tsp_dev_attribute_groups;

	/* for tp probe after se init.*/
	tpd_fw_cdev.name = "touchscreen";
	tpd_fw_cdev.private = NULL;
	tpd_fw_cdev.read_block = NULL;
	tpd_fw_cdev.write_block = NULL;
	tpd_fw_cdev.get_tpinfo = NULL;
	tpd_fw_cdev.get_gesture = NULL;
	tpd_fw_cdev.wake_gesture = NULL;
	tpd_fw_cdev.get_smart_cover = NULL;
	tpd_fw_cdev.set_smart_cover = NULL;
	tpd_fw_cdev.get_glove_mode = NULL;
	tpd_fw_cdev.set_glove_mode = NULL;
	tpd_fw_cdev.tp_i2c_reg_read = NULL;
	tpd_fw_cdev.tp_i2c_reg_write = NULL;
	tpd_fw_cdev.tp_i2c_16bor32b_reg_read = NULL;
	tpd_fw_cdev.tp_i2c_16bor32b_reg_write = NULL;
	tpd_fw_cdev.tp_fw_upgrade = NULL;
	tpd_fw_cdev.tp_suspend_show = NULL;
	tpd_fw_cdev.set_tp_suspend = NULL;
	tpd_fw_cdev.set_headset_state = NULL;
	tpd_fw_cdev.headset_state_show = NULL;
	tpd_fw_cdev.set_display_rotation = NULL;
	tpd_fw_cdev.set_edge_limit_level = NULL;
	tpd_fw_cdev.tpd_esd_check = NULL;
	tpd_fw_cdev.tp_hw_reset = NULL;
	tpd_fw_cdev.tp_reset_gpio_output = NULL;
	tpd_fw_cdev.set_tp_state = NULL;
	tpd_fw_cdev.get_singletap = NULL;
	tpd_fw_cdev.set_singletap = NULL;
	tpd_fw_cdev.get_noise = NULL;
	tpd_fw_cdev.get_one_key = NULL;
	tpd_fw_cdev.set_one_key = NULL;
	tpd_fw_cdev.get_play_game = NULL;
	tpd_fw_cdev.set_play_game = NULL;
	tpd_fw_cdev.b_gesture_enable = 0;
	tpd_fw_cdev.b_smart_cover_enable = 0;
	tpd_fw_cdev.b_glove_enable = 0;
	tpd_fw_cdev.display_rotation = 0;
	tpd_fw_cdev.TP_have_registered = false;
	tpd_fw_cdev.tp_suspend = false;
	tpd_fw_cdev.sys_set_tp_suspend_flag = false;
	tpd_fw_cdev.headset_state = false;
	tpd_fw_cdev.lcd_reset_processing = false;
	tpd_fw_cdev.tp_suspend_write_gesture = false;
	tpd_fw_cdev.reg_char_num = REG_CHAR_NUM_2;
	tpd_fw_cdev.b_single_tap_enable = 0;
	tpd_fw_cdev.screen_is_on = false;
	tpd_fw_cdev.edge_limit_level = 0;
	tpd_fw_cdev.edge_limit_pixel_level = 0;
	tpd_fw_cdev.long_press_max_count = 0;
	tpd_fw_cdev.edge_long_press_check = false;
	tpd_fw_cdev.one_key_enable =0;

	/**for tpd test*/
	tpd_fw_cdev.tpd_test_set_save_filepath = NULL;
	tpd_fw_cdev.tpd_test_get_save_filepath = NULL;
	tpd_fw_cdev.tpd_test_set_save_filename = NULL;
	tpd_fw_cdev.tpd_test_get_save_filename = NULL;
	tpd_fw_cdev.tpd_test_set_ini_filepath = NULL;
	tpd_fw_cdev.tpd_test_get_ini_filepath = NULL;
	tpd_fw_cdev.tpd_test_set_filename = NULL;
	tpd_fw_cdev.tpd_test_get_filename = NULL;
	tpd_fw_cdev.tpd_test_set_cmd = NULL;
	tpd_fw_cdev.tpd_test_get_cmd = NULL;
	tpd_fw_cdev.tpd_test_set_node_data_type = NULL;
	tpd_fw_cdev.tpd_test_get_node_data = NULL;
	tpd_fw_cdev.tpd_test_get_channel_info = NULL;
	tpd_fw_cdev.tpd_test_get_result = NULL;
	tpd_fw_cdev.tpd_test_get_tp_rawdata = NULL;
#ifdef CONFIG_TP_BSC_CALIBRATION
	tpd_fw_cdev.tpd_test_set_bsc_calibration = NULL;
	tpd_fw_cdev.tpd_test_get_bsc_calibration = NULL;
#endif
	tpd_fw_cdev.tpd_gpio_shutdown = NULL;
	tpd_fw_cdev.tpd_suspend_need_awake = NULL;

	tpd_uevent_init();

	memset(tpd_fw_cdev.ic_tpinfo.chip_batch, 0, MAX_VENDOR_NAME_LEN);
	memset(tpd_fw_cdev.edge_report_limit, 0, sizeof(tpd_fw_cdev.edge_report_limit));
	memset(tpd_fw_cdev.long_pess_suppression, 0, sizeof(tpd_fw_cdev.long_pess_suppression));
	tpd_clean_all_event();
	tpd_classdev_register(NULL, &tpd_fw_cdev);
	tpd_workquue_init();
	return 0;
}

static void __exit tpd_class_exit(void)
{
	tpd_uevent_exit();
	class_destroy(tsp_fw_class);
}

subsys_initcall(tpd_class_init);
module_exit(tpd_class_exit);

MODULE_AUTHOR("John Lenz, Richard Purdie");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TSP FW Class Interface");


