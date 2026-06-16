#define LOG_TAG         "Test"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_test.h"
#include "cts_firmware.h"
#include "cts_selftest.h"

extern char save_file_path[];
extern char save_test_result_log_name[];

bool test_debug = 0;
struct cts_log cts_log;

static char *cts_test_config_data =
				    "\nfw_version_test=1"
				    "\nshort_test=1"
				    "\nopen_test=1"
				    "\nrawdata_test=1"
				    "\nfirmware_version=0x0900"
				    "\nshort_threshold = 800"
				    "\nrawdata_max=3086"
				    "\nrawdata_min=1841"
				    ;

int cts_test_save_log(struct cts_device *cts_dev, const char *filepath, char *buf)
{

	struct file *file;
	/* u32 size; */
	u32 len;
	int ret;
	loff_t	pos = 0;

#ifndef SUPPORT_SAVE_TEST_LOG
	cts_info("Unsupport save log file");
	return -EPERM;
#endif

	if (buf == NULL) {
		cts_err("buf is NULL");
		return -EPERM;
	}

	file = filp_open(filepath, O_RDWR | O_CREAT | O_APPEND, 0666); /* O_TRUNC  O_APPEND */
	if (IS_ERR(file)) {
		cts_err("Open file '%s' failed %ld", filepath, PTR_ERR(file));
		return -EPERM;
	}

	len = strlen(buf);
	cts_info("write to file %s size: %d", filepath, len);

#if  KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	ret = kernel_write(file, buf, len, &pos);
#else
	ret = kernel_write(file, buf, len, pos);
#endif
	if (ret != len) {
		cts_err("kernel write %s fail %d", filepath, ret);
	}
	ret = filp_close(file, NULL);
	if (ret) {
		cts_warn("Close file '%s' failed %d", filepath, ret);
	}
	return ret;

}

int cts_fw_version_test_fun(struct cts_device *cts_dev, u16 para1, u16 para2)
{
	return cts_fw_version_test(cts_dev, para1);
}
int cts_short_test_fun(struct cts_device *cts_dev, u16 para1, u16 para2)
{
	return cts_short_test(cts_dev, para1);
}

int cts_open_test_fun(struct cts_device *cts_dev, u16 para1, u16 para2)
{
	return cts_open_test(cts_dev, para1);
}

int cts_rawdata_test_fun(struct cts_device *cts_dev, u16 para1, u16 para2)
{
	return cts_rawdata_test(cts_dev, para1, para2);
}

struct cts_test_cfg cts_test_items[] = {
	{
		.item_name = "fw_version_test",
		.result = -1,
		.para = {
			.para1 = "firmware_version",
		},
		.cts_test_fun = cts_fw_version_test_fun,
	},

	{
		.item_name = "short_test",
		.result = -1,
		.para = {
			.para1 = "short_threshold",
		},
		.cts_test_fun = cts_short_test_fun,
	},

	{
		.item_name = "open_test",
		.result = -1,
		.para = {
			.para1 = "rawdata_min",
		},
		.cts_test_fun = cts_open_test_fun,
	},

	{
		.item_name = "rawdata_test",
		.result = -1,
		.para = {
			.para1 = "rawdata_min",
			.para2 = "rawdata_max",
		},
		.cts_test_fun = cts_rawdata_test_fun,
	},
};

int cts_get_adc_data(struct cts_device *cts_dev, u16 *adc_data, int len)
{
	u8 ucTemp[4];
	u8 data[128];
	u16 count = 0;
	int i;

	/* adc stop */
	ucTemp[0] = 0x01;
	if (icn85xx_prog_i2c_txdata(cts_dev, 0x0408ac, ucTemp, 1) != 0)
		return 0;

	ucTemp[0] = 0x0f;
	if (icn85xx_prog_i2c_txdata(cts_dev, 0x040100, ucTemp, 1) != 0)
		return 0;

	ucTemp[0] = 0x07;
	if (icn85xx_prog_i2c_txdata(cts_dev, 0x040100, ucTemp, 1) != 0)
		return 0;

	/* adc start */
	ucTemp[0] = 0x01;
	if (icn85xx_prog_i2c_txdata(cts_dev, 0x040870, ucTemp, 1) != 0)
		return 0;

	/* Thread.Sleep(10); */
	usleep_range(10000, 10010);

	/* poll status,Wait adc data ready */
	/* uint start = timeGetTime(); */
	while (1) {
		count++;
		if (icn85xx_prog_i2c_rxdata(cts_dev, 0x040870, ucTemp, 1) != 0)
			return 0;

		if ((ucTemp[0] & 0x02) != 0)
			break;

		usleep_range(1000, 1010);
		if (count > 1000)
			return 0;
	}

	/* adc stop */
	ucTemp[0] = 0x01;
	if (icn85xx_prog_i2c_txdata(cts_dev, 0x0408ac, ucTemp, 1) != 0)
		return 0;

	/* Read ADC data from SRAM 0x24000. */
	if (icn85xx_prog_i2c_rxdata(cts_dev, 0x24000, data, (u32)len * 2) != 0)
		return 0;


	for (i = 0; i < len; i++) {
		adc_data[i] = (u16)(data[i * 2] + data[i * 2 + 1] * 256);
	}
	return 1;

}

#if (ICNT86xx_TYPE == CTS_CHIP_TYPE)
int icnt86_get_adc_data(struct cts_device *cts_dev, u16 *adc_data, int index)
{
	/* u8 ucTemp[600]; */
	char *ucTemp = NULL;
	u16 tmpval;
	u16 count = 0;
	int i, j, ret;
	int valid_data_flag = 0;
	int valid_line_flag = 0;
	int rowStart = 0;
	int rowEnd = 0;
	int average[15];
	u16 adcData[20][15];
	u16 MaxData, MinData;
	u8 ucValid = 0;
	u8 cnt = 20;

	ucTemp = kzalloc(600, GFP_KERNEL);
	if (ucTemp == NULL) {
		cts_err("allocate memery for short test log fail");
		return -ENOMEM;
	}

	while (cnt > 0) {
		/* adc stop */
		ucTemp[0] = 0x01;
		ret = icn85xx_prog_i2c_txdata(cts_dev, 0x0408ac, ucTemp, 1);
		if (ret) {
			cts_err("%s icn85xx_prog_i2c_txdata 0x0408ac\n", __func__);
		}


		/* adc start */
		ucTemp[0] = 0x01;
		ret = icn85xx_prog_i2c_txdata(cts_dev, 0x040870, ucTemp, 1);
		if (ret) {
			cts_err("%s icn85xx_prog_i2c_txdata 0x040870\n", __func__);
		}

		usleep_range(10000, 10010);

		while (1) {
			count++;
			ret = icn85xx_prog_i2c_rxdata(cts_dev, 0x04085e, ucTemp, 1);
			if (ret) {
				cts_err("%s icn85xx_prog_i2c_txdata 0x04085e\n", __func__);
			}

			if ((ucTemp[0] & 0x01) != 0)
				break;

			usleep_range(1000, 1010);
			if (count > 1000)
				/* return 0; */
				goto err_get_adc_data;
		}

		/* adc stop */
		ucTemp[0] = 0x01;
		ret = icn85xx_prog_i2c_txdata(cts_dev, 0x0408ac, ucTemp, 1);
		if (ret) {
			cts_err("%s icn85xx_prog_i2c_txdata 0x0408ac\n", __func__);
		}

		/* Read ADC data from SRAM 0x24000. */
		ret = icn85xx_prog_i2c_rxdata(cts_dev, 0x24000, ucTemp, 600);
		if (ret) {
			cts_err("%s icn85xx_prog_i2c_txdata 0x24000\n", __func__);
		}

		for (i = 0; i < 20; i++) {
			for (j = 0; j < 15; j++) {
				tmpval = (u16)(ucTemp[i * 30 + 2 * j + 1] << 8);
				tmpval |= ucTemp[i * 30 + 2 * j] & 0xff;
				adcData[i][j] = tmpval;
			}
		}

		for (i = 1; i < 20; i++) {
			valid_line_flag = 1;
			for (j = 0; j < 15; j++) {
				if (abs(adcData[i - 1][j] - adcData[i][j]) > 300) {
					valid_data_flag = false;
					break;
				}
			}

			if (i - rowStart >= 4) {
				rowEnd = i;
				valid_data_flag = true;
				break;
			} else if (valid_line_flag == false) {
				rowStart = i;
			}
		}

		if (valid_data_flag == false) {
			cnt--;
			continue;
		}

		for (i = 0; i < 15; i++)
			average[i] = 0;

		for (i = rowStart; i <= rowEnd; i++) {
			for (j = 0; j < 15; j++) {
				average[j] += adcData[i][j];
			}
		}

		for (i = 0; i < 15; i++)
			average[i] /= 5;

		MaxData = 0x0000;
		MinData = 0x7FFF;

		for (i = 0; i < 15; i++) {
			if (MaxData < average[i]) {
				MaxData = (u16)average[i];
			}
			if (MinData > average[i]) {
				MinData = (u16)average[i];
			}
		}

		if ((MaxData >= (1024 + 500)) && (MinData <= (0 + 200))) {
			cnt--;
			continue;
		} else {
			adc_data[index] = MaxData;
			ucValid = 1;
		}

		cnt--;
		if (ucValid == 1) {
			break;
		}
	}

	kfree(ucTemp);

	if (cnt > 0) {
		return true;
	} else {
		return false;
	}

err_get_adc_data:
	kfree(ucTemp);
	return false;
}

int icnt86_set_config(struct cts_device *cts_dev, int index)
{
	u32 regAddr[128];
	u8 regData[128];
	int i, ret = 0;
	int regCount = 0;

	cts_info("%s begin", __func__);

	regAddr[regCount] = 0x040460;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x040009;
	regData[regCount++] = 0xff;
	regAddr[regCount] = 0x040105;
	regData[regCount++] = 0x16;
	regAddr[regCount] = 0x040106;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x04010C;
	regData[regCount++] = 0xbc;
	regAddr[regCount] = 0x04010D;
	regData[regCount++] = 0x40;
	regAddr[regCount] = 0x040120;
	regData[regCount++] = 0x01;
	regAddr[regCount] = 0x040135;
	regData[regCount++] = 0x60;
	regAddr[regCount] = 0x04013C;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x040158;
	regData[regCount++] = 0x09;
	regAddr[regCount] = 0x040188;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x040189;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x04018A;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x04018B;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x04018C;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x04018D;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x04018E;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x04018F;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x040194;
	regData[regCount++] = 0xA0;
	regAddr[regCount] = 0x040196;
	regData[regCount++] = 0x0B;
	regAddr[regCount] = 0x040197;
	regData[regCount++] = 0x0E;
	regAddr[regCount] = 0x040198;
	regData[regCount++] = 0x1A;

	switch (index) {
	case 0: /* RX */
		regAddr[regCount] = 0x040100;
		regData[regCount++] = 0x01;
		regAddr[regCount] = 0x04010B;
		regData[regCount++] = 0x30;
		regAddr[regCount] = 0x04010E;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040118;
		regData[regCount++] = 0x14;
		regAddr[regCount] = 0x040862;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040864;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040868;
		regData[regCount++] = 0x01;
		regAddr[regCount] = 0x040858;
		regData[regCount++] = 0x0F;
		regAddr[regCount] = 0x040859;
		regData[regCount++] = 0x0F;
		regAddr[regCount] = 0x04085A;
		regData[regCount++] = 0x01;
		regAddr[regCount] = 0x04085B;
		regData[regCount++] = 0x01;

		regAddr[regCount] = 0x040110;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040111;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040112;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040113;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040114;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040115;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040116;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040117;
		regData[regCount++] = 0xBB;

		regAddr[regCount] = 0x040874;
		regData[regCount++] = 0x01;
		regAddr[regCount] = 0x0408B0;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0408B1;
		regData[regCount++] = 0x00;

		regAddr[regCount] = 0x0408B2;
		regData[regCount++] = 0x2C;
		regAddr[regCount] = 0x0408B3;
		regData[regCount++] = 0x01;

		regAddr[regCount] = 0x04085C;
		regData[regCount++] = 0x02;
		regAddr[regCount] = 0x04085D;
		regData[regCount++] = 0x00;

		regAddr[regCount] = 0x040800;
		regData[regCount++] = 0x20;
		regAddr[regCount] = 0x040801;
		regData[regCount++] = 0x22;
		regAddr[regCount] = 0x040802;
		regData[regCount++] = 0x24;
		regAddr[regCount] = 0x040803;
		regData[regCount++] = 0x26;
		regAddr[regCount] = 0x040804;
		regData[regCount++] = 0x28;
		regAddr[regCount] = 0x040805;
		regData[regCount++] = 0x2A;
		regAddr[regCount] = 0x040806;
		regData[regCount++] = 0x2C;
		regAddr[regCount] = 0x040807;
		regData[regCount++] = 0x2E;
		regAddr[regCount] = 0x040808;
		regData[regCount++] = 0x30;
		regAddr[regCount] = 0x040809;
		regData[regCount++] = 0x32;
		regAddr[regCount] = 0x04080A;
		regData[regCount++] = 0x34;
		regAddr[regCount] = 0x04080B;
		regData[regCount++] = 0x36;
		regAddr[regCount] = 0x04080C;
		regData[regCount++] = 0x38;
		regAddr[regCount] = 0x04080D;
		regData[regCount++] = 0x3A;
		regAddr[regCount] = 0x04080E;
		regData[regCount++] = 0x3C;

		regAddr[regCount] = 0x040810;
		regData[regCount++] = 0x21;
		regAddr[regCount] = 0x040811;
		regData[regCount++] = 0x23;
		regAddr[regCount] = 0x040812;
		regData[regCount++] = 0x25;
		regAddr[regCount] = 0x040813;
		regData[regCount++] = 0x27;
		regAddr[regCount] = 0x040814;
		regData[regCount++] = 0x29;
		regAddr[regCount] = 0x040815;
		regData[regCount++] = 0x2B;
		regAddr[regCount] = 0x040816;
		regData[regCount++] = 0x2D;
		regAddr[regCount] = 0x040817;
		regData[regCount++] = 0x2F;
		regAddr[regCount] = 0x040818;
		regData[regCount++] = 0x31;
		regAddr[regCount] = 0x040819;
		regData[regCount++] = 0x33;
		regAddr[regCount] = 0x04081A;
		regData[regCount++] = 0x35;
		regAddr[regCount] = 0x04081B;
		regData[regCount++] = 0x37;
		regAddr[regCount] = 0x04081C;
		regData[regCount++] = 0x39;
		regAddr[regCount] = 0x04081D;
		regData[regCount++] = 0x3B;
		regAddr[regCount] = 0x04081E;
		regData[regCount++] = 0x3D;

		regAddr[regCount] = 0x040118;
		regData[regCount++] = 0x16;
		regAddr[regCount] = 0x04015C;
		regData[regCount++] = 0x00;

		regAddr[regCount] = 0x04011C;
		regData[regCount++] = 0x55;
		regAddr[regCount] = 0x04011D;
		regData[regCount++] = 0x55;

		regAddr[regCount] = 0x040140;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040138;
		regData[regCount++] = 0x01;

		regAddr[regCount] = 0x040195;
		regData[regCount++] = 0xF1;
		regAddr[regCount] = 0x040B38;
		regData[regCount++] = 0x03;
		break;

	case 1: /* TX */
		regAddr[regCount] = 0x040100;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x04010B;
		regData[regCount++] = 0x30;
		regAddr[regCount] = 0x04010E;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040860;
		regData[regCount++] = 0x01;
		regAddr[regCount] = 0x040862;
		regData[regCount++] = 0x0E;
		regAddr[regCount] = 0x040948;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040949;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x04094A;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x04094B;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040864;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040868;
		regData[regCount++] = 0x01;
		regAddr[regCount] = 0x040858;
		regData[regCount++] = 0x0F;
		regAddr[regCount] = 0x040859;
		regData[regCount++] = 0x0F;
		regAddr[regCount] = 0x04085A;
		regData[regCount++] = 0x01;
		regAddr[regCount] = 0x04085B;
		regData[regCount++] = 0x01;
		regAddr[regCount] = 0x040110;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040111;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040112;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040113;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040114;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040115;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040116;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040117;
		regData[regCount++] = 0xBB;

		regAddr[regCount] = 0x040874;
		regData[regCount++] = 0x01;
		regAddr[regCount] = 0x0408B0;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0408B1;
		regData[regCount++] = 0x00;

		regAddr[regCount] = 0x0408B2;
		regData[regCount++] = 0x2C;
		regAddr[regCount] = 0x0408B3;
		regData[regCount++] = 0x01;

		regAddr[regCount] = 0x04085C;
		regData[regCount++] = 0x02;
		regAddr[regCount] = 0x04085D;
		regData[regCount++] = 0x00;

		regAddr[regCount] = 0x040800;
		regData[regCount++] = 0x20;
		regAddr[regCount] = 0x040801;
		regData[regCount++] = 0x22;
		regAddr[regCount] = 0x040802;
		regData[regCount++] = 0x24;
		regAddr[regCount] = 0x040803;
		regData[regCount++] = 0x26;
		regAddr[regCount] = 0x040804;
		regData[regCount++] = 0x28;
		regAddr[regCount] = 0x040805;
		regData[regCount++] = 0x2A;
		regAddr[regCount] = 0x040806;
		regData[regCount++] = 0x2C;
		regAddr[regCount] = 0x040807;
		regData[regCount++] = 0x2E;
		regAddr[regCount] = 0x040808;
		regData[regCount++] = 0x30;
		regAddr[regCount] = 0x040809;
		regData[regCount++] = 0x32;
		regAddr[regCount] = 0x04080A;
		regData[regCount++] = 0x34;
		regAddr[regCount] = 0x04080B;
		regData[regCount++] = 0x36;
		regAddr[regCount] = 0x04080C;
		regData[regCount++] = 0x38;
		regAddr[regCount] = 0x04080D;
		regData[regCount++] = 0x3A;
		regAddr[regCount] = 0x04080E;
		regData[regCount++] = 0x3C;

		regAddr[regCount] = 0x040810;
		regData[regCount++] = 0x21;
		regAddr[regCount] = 0x040811;
		regData[regCount++] = 0x23;
		regAddr[regCount] = 0x040812;
		regData[regCount++] = 0x25;
		regAddr[regCount] = 0x040813;
		regData[regCount++] = 0x27;
		regAddr[regCount] = 0x040814;
		regData[regCount++] = 0x29;
		regAddr[regCount] = 0x040815;
		regData[regCount++] = 0x2B;
		regAddr[regCount] = 0x040816;
		regData[regCount++] = 0x2D;
		regAddr[regCount] = 0x040817;
		regData[regCount++] = 0x2F;
		regAddr[regCount] = 0x040818;
		regData[regCount++] = 0x31;
		regAddr[regCount] = 0x040819;
		regData[regCount++] = 0x33;
		regAddr[regCount] = 0x04081A;
		regData[regCount++] = 0x35;
		regAddr[regCount] = 0x04081B;
		regData[regCount++] = 0x37;
		regAddr[regCount] = 0x04081C;
		regData[regCount++] = 0x39;
		regAddr[regCount] = 0x04081D;
		regData[regCount++] = 0x3B;
		regAddr[regCount] = 0x04081E;
		regData[regCount++] = 0x3D;

		regAddr[regCount] = 0x040118;
		regData[regCount++] = 0x04;
		regAddr[regCount] = 0x040119;
		regData[regCount++] = 0x67;

		regAddr[regCount] = 0x04012C;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x04012D;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x04012E;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x04012F;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040130;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040131;
		regData[regCount++] = 0x00;

		regAddr[regCount] = 0x040140;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040138;
		regData[regCount++] = 0x01;

		regAddr[regCount] = 0x040195;
		regData[regCount++] = 0xF1;
		regAddr[regCount] = 0x040136;
		regData[regCount++] = 0x01;

		regAddr[regCount] = 0x040198;
		regData[regCount++] = 0x1C;
		regAddr[regCount] = 0x040118;
		regData[regCount++] = 0x06;

		regAddr[regCount] = 0x04011C;
		regData[regCount++] = 0xFF;
		regAddr[regCount] = 0x04011D;
		regData[regCount++] = 0xFF;

		regAddr[regCount] = 0x04015C;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0408C0;
		regData[regCount++] = 0x01;

		regAddr[regCount] = 0x040964;
		regData[regCount++] = 0xBB;
		regAddr[regCount] = 0x040B38;
		regData[regCount++] = 0x00;
		break;

	default:
		break;
	}
	for (i = 0; i < regCount; i++) {
		ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[i], &regData[i], 1);
		if (ret)
			return 0;
		udelay(10);
	}

	cts_info("%s end", __func__);

	return 1;
}

int icnt86_get_short_resistor_data(struct cts_device *cts_dev, u16 *data, int type)
{
	u32 regAddr[10];
	u16 temp_data[60];
	u8 regData[10];
	u8 ucTemp[4] = {0};
	u32 regCount = 0;
	u32 txMask = 1;
	int ret, i, j;
	int txNum = 0;

	switch (type) {
	case 1:  /* for RX */
		regAddr[regCount] = 0x040195;
		regData[regCount++] = 0xf1;
		regAddr[regCount] = 0x04010a;
		regData[regCount++] = 0x47;
		for (i = 0; i < regCount; i++) {
			ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[i], &regData[i], 1);
			if (ret) {
				return 0;
			}
			udelay(10);
		}

		for (i = 0; i < 15; i++) {
			memset(ucTemp, 0x00, sizeof(ucTemp));
			ucTemp[i / 8] = (u8)(1 << (i % 8));
			if (icn85xx_prog_i2c_txdata(cts_dev, 0x04011c, ucTemp, 3) != 0) {
				return 0;
			}

			ucTemp[0] = 0x14;
			ret = icn85xx_prog_i2c_txdata(cts_dev, 0x40118, ucTemp, 1);
			if (ret) {
				return 0;
			}

			ucTemp[0] = 0x16;
			ret = icn85xx_prog_i2c_txdata(cts_dev, 0x40118, ucTemp, 1);
			if (ret) {
				return 0;
			}

			if (icnt86_get_adc_data(cts_dev, temp_data, i * 2) == 0) {
				return 0;
			}

			data[i * 2] = temp_data[i * 2];

			ucTemp[0] = 0x24;
			ret = icn85xx_prog_i2c_txdata(cts_dev, 0x40118, ucTemp, 1);
			if (ret) {
				return 0;
			}

			ucTemp[0] = 0x26;
			ret = icn85xx_prog_i2c_txdata(cts_dev, 0x40118, ucTemp, 1);
			if (ret) {
				return 0;
			}

			if (icnt86_get_adc_data(cts_dev, temp_data, i * 2 + 1) == 0) {
				return 0;
			}

			data[i * 2 + 1] = temp_data[i * 2 + 1];
		}
		break;

	case 2: /* for TX */
		for (i = 0; i < 15; i++) {
			regCount = 0;

			memset(ucTemp, 0x00, sizeof(ucTemp));
			ucTemp[i / 8] = (u8)(1 << (i % 8));
			if (icn85xx_prog_i2c_txdata(cts_dev, 0x04011c, ucTemp, 3) != 0)
				return 0;

			/* ---first array--- */
			regAddr[regCount] = 0x040119;
			regData[regCount++] = 0x61;
			regAddr[regCount] = 0x040124;
			regData[regCount++] = (u8)(~txMask);
			regAddr[regCount] = 0x040125;
			regData[regCount++] = (u8)(~txMask >> 8);
			regAddr[regCount] = 0x040126;
			regData[regCount++] = (u8)(~txMask >> 16);
			regAddr[regCount] = 0x040127;
			regData[regCount++] = (u8)(~txMask >> 24);
			regAddr[regCount] = 0x040128;
			regData[regCount++] = 0x00;
			regAddr[regCount] = 0X040129;
			regData[regCount++] = 0x00;
			for (j = 0; j < regCount; j++) {
				ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[j], &regData[j], 1);
				if (ret)
					return 0;
				udelay(10);
			}

			if (icnt86_get_adc_data(cts_dev, temp_data, i * 3) == 0)
				return false;

			data[i * 3] = temp_data[i * 3];

			txNum++;
			if (txNum > 41)
				break;
			txMask <<= 1;

			/* ----second array---- */
			regCount = 0;
			regAddr[regCount] = 0x040119;
			regData[regCount++] = 0x62;
			regAddr[regCount] = 0x040124;
			regData[regCount++] = (u8)(~txMask);
			regAddr[regCount] = 0x040125;
			regData[regCount++] = (u8)(~txMask >> 8);
			regAddr[regCount] = 0x040126;
			regData[regCount++] = (u8)(~txMask >> 16);
			regAddr[regCount] = 0x040127;
			regData[regCount++] = (u8)(~txMask >> 24);
			regAddr[regCount] = 0x040128;
			regData[regCount++] = 0x00;
			regAddr[regCount] = 0X040129;
			regData[regCount++] = 0x00;
			for (j = 0; j < regCount; j++) {
				ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[j], &regData[j], 1);
				if (ret)
					return 0;
				udelay(10);
			}

			if (icnt86_get_adc_data(cts_dev, temp_data, i * 3 + 1) == 0)
				return false;

			data[i * 3 + 1] = temp_data[i * 3 + 1];

			txNum++;
			if (txNum > 41)
				break;
			txMask <<= 1;

			/* ----third array----- */
			regCount = 0;
			regAddr[regCount] = 0x040119;
			regData[regCount++] = 0x64;
			regAddr[regCount] = 0x040124;
			regData[regCount++] = (u8)(~txMask);
			regAddr[regCount] = 0x040125;
			regData[regCount++] = (u8)(~txMask >> 8);
			regAddr[regCount] = 0x040126;
			regData[regCount++] = (u8)(~txMask >> 16);
			regAddr[regCount] = 0x040127;
			regData[regCount++] = (u8)(~txMask >> 24);
			regAddr[regCount] = 0x040128;
			regData[regCount++] = 0x00;
			regAddr[regCount] = 0X040129;
			regData[regCount++] = 0x00;
			for (j = 0; j < regCount; j++) {
				ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[j], &regData[j], 1);
				if (ret)
					return 0;
				udelay(10);
			}

			if (icnt86_get_adc_data(cts_dev, temp_data, i * 3 + 2) == 0)
				return false;

			data[i * 3 + 2] = temp_data[i * 3 + 2];

			txNum++;
			if (txNum > 41)
				break;
			txMask <<= 1;
		}
		break;
	}
	return 1;
}

int icnt86xx_get_tiny_short_data(struct cts_device *cts_dev,
				 struct tiny_short_config *tiny_short_config)
{
	u16 temp_data[48];
	u8 ucTemp[0x100] = {0};
	u16 rx_data[RX_CHANNEL_NUM];
	u16 tx_data[TX_CHANNEL_NUM];
	int i, ret;

	for (i = 0; i < 8; i++) {
		ret = cts_prog_writesb_retry(cts_dev, 0xc800 + i * 0x100, ucTemp, 0x100);
		if (ret) {
			cts_err("Write C800-D000 failed %d", ret);
			return ret;
		}
	}

	ret = cts_prog_writesb_retry(cts_dev, 0x020000, ucTemp, 16);
	if (ret) {
		cts_err("Write 0x20000 failed %d", ret);
		return ret;
	}

	/* RX */
	if (icnt86_set_config(cts_dev, 0) == false)
		return false;


	if (icnt86_get_short_resistor_data(cts_dev, temp_data, 1) == false)
		return false;

	for (i = 0; i < RX_CHANNEL_NUM; i++) {
		rx_data[i] = temp_data[i];
		tiny_short_config->rx_data[i] = (u32)rx_data[i];
	}

	if (test_debug) {
		cts_info("rx_data");
		for (i = 0; i < RX_CHANNEL_NUM; i++)
			cts_info("%d", rx_data[i]);
	}

	/* TX */
	if (icnt86_set_config(cts_dev, 1) == false)
		return false;

	if (icnt86_get_short_resistor_data(cts_dev, temp_data, 2) == false)
		return false;

	for (i = 0; i < TX_CHANNEL_NUM; i++) {
		tx_data[i] = temp_data[i];
		tiny_short_config->tx_data[i] = (u32)tx_data[i];
	}

	if (test_debug) {
		cts_info("tx_data");
		for (i = 0; i < TX_CHANNEL_NUM; i++)
			cts_info("%d", tx_data[i]);
	}

	return 1;
}

int icnt86_start_tiny_short_test(struct cts_device *cts_dev, char *buf,
				 struct tiny_short_config *tiny_short_config)
{
	int index;
	int i;
	u32 th = (u32)(tiny_short_config->tiny_short_threshold);

	tiny_short_config->tiny_short_result = true;

	for (i = 0; i < RX_CHANNEL_NUM; i++) {
		tiny_short_config->rx_status[i] = 0;
	}

	for (i = 0; i < TX_CHANNEL_NUM; i++) {
		tiny_short_config->tx_status[i] = 0;
	}

	if (icnt86xx_get_tiny_short_data(cts_dev, tiny_short_config) == 1) {
		tiny_short_config->tiny_short_resistor = 0;
		tiny_short_config->short_tx_num = 0;
		tiny_short_config->short_rx_num = 0;

		for (i = 0; i < RX_CHANNEL_NUM; i++) {
			if (tiny_short_config->rx_data[i] < th) {
				tiny_short_config->tiny_short_result  = false;
				tiny_short_config->rx_status[i] = 1;
				if (tiny_short_config->rx_data[i] > tiny_short_config->tiny_short_resistor) {
					tiny_short_config->tiny_short_resistor = (int)tiny_short_config->rx_data[i];
				}
				cts_err("RX physical index: %d tiny short test fail resistor: %d!!!", i,
					(int)tiny_short_config->rx_data[i]);
			}
		}

		for (i = 0; i < TX_CHANNEL_NUM; i++) {
			if (tiny_short_config->tx_data[i] < th) {
				tiny_short_config->tiny_short_result  = false;
				tiny_short_config->tx_status[i] = 1;
				if (tiny_short_config->tx_data[i] > tiny_short_config->tiny_short_resistor) {
					tiny_short_config->tiny_short_resistor = (int)tiny_short_config->tx_data[i];
				}
				cts_err("TX  physical index: %d tiny short test fail resistor: %d!!!", i,
					(int)tiny_short_config->tx_data[i]);

			}
		}

		for (i = 0; i < RX_CHANNEL_NUM; i++) {
			tiny_short_config->rx_logic_data[i] = 0;
			tiny_short_config->rx_logic_status[i] = 0;
		}

		for (i = 0; i < TX_CHANNEL_NUM; i++) {
			tiny_short_config->tx_logic_data[i] = 0;
			tiny_short_config->tx_logic_status[i] = 0;
		}

		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
			"short threshold:%d\n", th);
		/* rx data */
		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
			"rx_logic_data:");

		for (i = 0; i < tiny_short_config->u8col_num; i++) {
			tiny_short_config->rx_logic_data[i] =
				tiny_short_config->rx_data[tiny_short_config->rx_order[i]];
			tiny_short_config->rx_logic_status[i] =
				tiny_short_config->rx_status[tiny_short_config->rx_order[i]];
			if (tiny_short_config->rx_logic_status[i]) {
				tiny_short_config->short_rx_num++;
				cts_err("Logic order: RX%d tiny short test fail resistor: %d !!!", i + 1,
					tiny_short_config->rx_logic_data[i]);
			}

			if ((i % 10) == 0) {
				cts_log.log_cnt += snprintf(buf + cts_log.log_cnt,
						PAGE_SIZE * 2 - cts_log.log_cnt, "\n");
			}
			cts_log.log_cnt += snprintf(buf + cts_log.log_cnt,
				PAGE_SIZE * 2 - cts_log.log_cnt, "%-8d", tiny_short_config->rx_logic_data[i]);
		}

		for (i = 0; i < tiny_short_config->u8col_num; i++) {
			if (tiny_short_config->rx_logic_status[i]) {
				cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
							   "\nLogic order: RX%d tiny short test fail resistor: %d !!!\n",
							   i + 1, tiny_short_config->rx_logic_data[i]);
			}
		}
		/* tx data */
		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt, "\ntx_logic_data:");

		for (i = 0; i < tiny_short_config->u8row_num; i++) {

			tiny_short_config->tx_logic_data[i] =
				tiny_short_config->tx_data[tiny_short_config->tx_order[i]];
			tiny_short_config->tx_logic_status[i] =
				tiny_short_config->tx_status[tiny_short_config->tx_order[i]];

			if (tiny_short_config->tx_logic_status[i]) {
				tiny_short_config->short_tx_num++;
				cts_err("Logic order: TX%d tiny short test fail resistor: %d !!!", i + 1,
					tiny_short_config->tx_logic_data[i]);
			}

			if ((i % 10) == 0) {
				cts_log.log_cnt += snprintf(buf + cts_log.log_cnt,
						PAGE_SIZE * 2 - cts_log.log_cnt, "\n");
			}
			cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt, "%-8d",
					tiny_short_config->tx_logic_data[i]);

		}

		for (i = 0; i < tiny_short_config->u8row_num; i++) {
			if (tiny_short_config->tx_logic_status[i]) {
				cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
							   "\nLogic order: TX%d tiny short test fail resistor: %d !!!",
							   i + 1, tiny_short_config->tx_logic_data[i]);
			}
		}
		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt,
			PAGE_SIZE * 2 - cts_log.log_cnt, "\n");

	} else {
		tiny_short_config->tiny_short_result = false;
		return -EPERM;
	}

	return 0;
}
#else
int cts_tiny_short_test_init(struct cts_device *cts_dev)
{
	u32 regAddr[128];
	u8 regData[128];
	u32 regCount = 0;
	int ret, i;

	/* Step1:  initial setting */
	regAddr[regCount] = 0x040005;
	regData[regCount++] = 0x01;
	regAddr[regCount] = 0x040138;
	regData[regCount++] = 0x01;
	regAddr[regCount] = 0x040140;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x04013c;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x04010B;
	regData[regCount++] = 0x30;
	regAddr[regCount] = 0x040105;
	regData[regCount++] = 0x1a;
	regAddr[regCount] = 0x040136;
	regData[regCount++] = 0x30;
	regAddr[regCount] = 0x040100;
	regData[regCount++] = 0x07;
	regAddr[regCount] = 0x04011c;
	regData[regCount++] = 0xff;
	regAddr[regCount] = 0x04011d;
	regData[regCount++] = 0xff;
	regAddr[regCount] = 0x04011e;
	regData[regCount++] = 0x03;
	regAddr[regCount] = 0x04015c;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x040195;
	regData[regCount++] = 0xF1;
	regAddr[regCount] = 0x0401B2;
	regData[regCount++] = 0x03;
	regAddr[regCount] = 0x0401BC;
	regData[regCount++] = 0x01;
	regAddr[regCount] = 0x040197;
	regData[regCount++] = 0x02;
	regAddr[regCount] = 0x040178;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x04019C;
	regData[regCount++] = 0xFF;
	regAddr[regCount] = 0x0401B3;
	regData[regCount++] = 0x1F;
	regAddr[regCount] = 0x04088D;
	regData[regCount++] = 0x03;
	regAddr[regCount] = 0x04088C;
	regData[regCount++] = 0x84;
	regAddr[regCount] = 0x04088F;
	regData[regCount++] = 0x02;
	regAddr[regCount] = 0x04088E;
	regData[regCount++] = 0x58;
	regAddr[regCount] = 0x0408C4;
	regData[regCount++] = 0x20;
	regAddr[regCount] = 0x0408C5;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x0408C6;
	regData[regCount++] = 0x01;
	regAddr[regCount] = 0x040841;
	regData[regCount++] = 0x0b;
	regAddr[regCount] = 0x040858;
	regData[regCount++] = 0x12;
	regAddr[regCount] = 0x040859;
	regData[regCount++] = 0x12;
	regAddr[regCount] = 0x040145;
	regData[regCount++] = 0x00;
	regAddr[regCount] = 0x040198;
	regData[regCount++] = 0x1A;
	regAddr[regCount] = 0x04010A;
	regData[regCount++] = 0x44;
	regAddr[regCount] = 0x04082C;
	regData[regCount++] = 0x08;
	for (i = 0; i < (RX_CHANNEL_NUM + 1) / 2; i++) {
		regAddr[regCount] = (u32)(0x040800 + i);
		regData[regCount++] = (u8)(0x40 + i * 2);
	}

	for (i = 0; i < RX_CHANNEL_NUM / 2; i++) {
		regAddr[regCount] = (u32)(0x040814 + i);
		regData[regCount++] = (u8)(0x40 + i * 2 + 1);
	}

	regAddr[regCount] = 0x040874;
	regData[regCount++] = 0x01;
	regAddr[regCount] = 0x040868;
	regData[regCount++] = 0x01;
	regAddr[regCount] = 0x0408F0;
	regData[regCount++] = 0x01;

	/* Debug.WriteLine("-----------InitialSetting-----------------"); */
	/* for (int i = 0; i < regCount;i++ ) */
	/* { */
	/* Debug.WriteLine(string.Format("{0:X6}={1:X2}",regAddr[i],regData[i])); */
	/* } */
	for (i = 0; i < regCount; i++) {
		ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[i], &regData[i], 1);
		if (ret)
			return 0;
		udelay(10);
	}

	/* if (testConfig.chip.I2cWriteProgModeEx(regAddr, regData, regCount) != 0) */
	/* { */
	/* return false; */
	/* } */
	return 1;
}

int cts_get_short_resistor_data(struct cts_device *cts_dev, u16 *data, enum adc_data type)
{
	u32 regAddr[128];
	u16 temp_data[128];
	u8 regData[128];
	u8 ucTemp[4] = {0};
	u32 regCount = 0;
	int ret, i;

	switch (type) {
	case CHANNEL_OFFSET:
		regCount = 0;
		regAddr[regCount] = 0x040118;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040119;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040136;
		regData[regCount++] = 0x30;
		regAddr[regCount] = 0x0401A0;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401A1;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401A2;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401A3;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401A4;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401C0;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401C1;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401C2;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401C3;
		regData[regCount++] = 0x00;

		/* Debug.WriteLine(string.Format("-----------{0}-----------------", type.ToString())); */
		/* for (int i = 0; i < regCount; i++) */
		/* { */
		/* Debug.WriteLine(string.Format("{0:X6}={1:X2}", regAddr[i], regData[i])); */
		/* } */

		/* if (icn85xx_prog_i2c_txdata(cts_dev, regAddr, regData, regCount) != 0) */
		/* return 0; */
		for (i = 0; i < regCount; i++) {
			ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[i], &regData[i], 1);
			if (ret)
				return 0;
			udelay(10);
		}

		if (cts_get_adc_data(cts_dev, data, RX_CHANNEL_NUM) == 0)
			return 0;

		break;
	case INTERNAL_REFERENCE_RESISTOR:
		regCount = 0;
		regAddr[regCount] = 0x040118;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040119;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040136;
		regData[regCount++] = 0x31;
		regAddr[regCount] = 0x0401A0;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401A1;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401A2;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401A3;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401A4;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401C0;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401C1;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401C2;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x0401C3;
		regData[regCount++] = 0x00;

		/* Debug.WriteLine(string.Format("-----------{0}-----------------", type.ToString())); */
		/* for (int i = 0; i < regCount; i++) */
		/* { */
		/* Debug.WriteLine(string.Format("{0:X6}={1:X2}", regAddr[i], regData[i])); */
		/* } */

		/* if (testConfig.chip.I2cWriteProgModeEx(regAddr, regData, regCount) != 0) */
		/* return false; */
		for (i = 0; i < regCount; i++) {
			ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[i], &regData[i], 1);
			if (ret)
				return 0;
			udelay(10);
		}

		if (cts_get_adc_data(cts_dev, data, RX_CHANNEL_NUM) == 0)
			return 0;

		break;
	case ODD_RX_TO_GROUND_SHORT_RESISTOR:
		regCount = 0;
		regAddr[regCount] = 0x040118;
		regData[regCount++] = 0x10;
		regAddr[regCount] = 0x040119;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040136;
		regData[regCount++] = 0x30;
		regAddr[regCount] = 0x0401A0;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A1;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A2;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A3;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A4;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C0;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C1;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C2;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C3;
		regData[regCount++] = 0xff;

		/* Debug.WriteLine(string.Format("-----------{0}-----------------", type.ToString())); */
		/* for (int i = 0; i < regCount; i++) */
		/* { */
		/* Debug.WriteLine(string.Format("{0:X6}={1:X2}", regAddr[i], regData[i])); */
		/* } */

		/* if (testConfig.chip.I2cWriteProgModeEx(regAddr, regData, regCount) != 0) */
		/* return false; */
		for (i = 0; i < regCount; i++) {
			ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[i], &regData[i], 1);
			if (ret)
				return 0;
			udelay(10);
		}

		for (i = 0; i < (RX_CHANNEL_NUM + 1) / 2; i++) {
			/* byte[] ucTemp = new byte[4]; */
			/* Array.Clear(ucTemp, 0, ucTemp.Count()); */
			memset(ucTemp, 0x00, sizeof(ucTemp));
			ucTemp[i / 8] = (u8)(1 << (i % 8));
			if (icn85xx_prog_i2c_txdata(cts_dev, 0x04011c, ucTemp, 3) != 0)
				return 0;

			/* for (int j = 0; j < 3; j++) */
			/* { */
			/* Debug.WriteLine(string.Format("{0:X6}={1:X2}", 0x04011c + j, ucTemp[j])); */
			/* } */

			/* UInt16[] temp_data = new UInt16[RX_CHANNEL_NUM]; */
			if (cts_get_adc_data(cts_dev, temp_data, RX_CHANNEL_NUM) == 0)
				return 0;

			data[i * 2] = temp_data[i * 2];
		}

		break;
	case EVEN_RX_TO_GROUND_SHORT_RESISTOR:
		regAddr[regCount] = 0x040118;
		regData[regCount++] = 0x20;
		regAddr[regCount] = 0x040119;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040136;
		regData[regCount++] = 0x30;
		regAddr[regCount] = 0x0401A0;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A1;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A2;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A3;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A4;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C0;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C1;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C2;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C3;
		regData[regCount++] = 0xff;

		/* Debug.WriteLine(string.Format("-----------{0}-----------------", type.ToString())); */
		/* for (int i = 0; i < regCount; i++) */
		/* { */
		/* Debug.WriteLine(string.Format("{0:X6}={1:X2}", regAddr[i], regData[i])); */
		/* } */

		/* if (testConfig.chip.I2cWriteProgModeEx(regAddr, regData, regCount) != 0) */
		/* return false; */
		for (i = 0; i < regCount; i++) {
			ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[i], &regData[i], 1);
			if (ret)
				return 0;
			udelay(10);
		}

		for (i = 0; i < RX_CHANNEL_NUM / 2; i++) {
			/* byte[] ucTemp = new byte[4]; */
			/* Array.Clear(ucTemp, 0, ucTemp.Count()); */
			memset(ucTemp, 0x00, sizeof(ucTemp));

			ucTemp[i / 8] = (u8)(1 << (i % 8));
			if (icn85xx_prog_i2c_txdata(cts_dev, 0x04011c, ucTemp, 3) != 0)
				return 0;

			/* for (int j = 0; j < 3; j++) */
			/* { */
			/* Debug.WriteLine(string.Format("{0:X6}={1:X2}", 0x04011c + j, ucTemp[j])); */
			/* } */

			/* UInt16[] temp_data = new UInt16[RX_CHANNEL_NUM]; */
			if (cts_get_adc_data(cts_dev, temp_data, RX_CHANNEL_NUM) == 0)
				return 0;

			data[i * 2] = temp_data[i * 2];
		}

		break;
	case ODD_TX_TO_GROUND_SHORT_RESISTOR:
		regAddr[regCount] = 0x040118;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040119;
		regData[regCount++] = 0x01;
		regAddr[regCount] = 0x040136;
		regData[regCount++] = 0x30;
		regAddr[regCount] = 0x0401A0;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A1;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A2;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A3;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A4;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C0;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C1;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C2;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C3;
		regData[regCount++] = 0xff;

		/* if (testConfig.chip.I2cWriteProgModeEx(regAddr, regData, regCount) != 0) */
		/* return false; */
		for (i = 0; i < regCount; i++) {
			ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[i], &regData[i], 1);
			if (ret)
				return 0;
			udelay(10);
		}

		for (i = 0; i < (TX_CHANNEL_NUM + 1) / 2; i++) {
			/* byte[] ucTemp = new byte[4]; */
			/* Array.Clear(ucTemp, 0, ucTemp.Count()); */
			memset(ucTemp, 0x00, sizeof(ucTemp));
			ucTemp[i / 8] = (u8)(1 << (i % 8));
			if (icn85xx_prog_i2c_txdata(cts_dev, 0x04011c, ucTemp, 3) != 0)
				return 0;

			/* UInt16[] temp_data = new UInt16[TX_CHANNEL_NUM]; */
			if (cts_get_adc_data(cts_dev,  temp_data, TX_CHANNEL_NUM) == 0)
				return 0;

			data[i * 2] = temp_data[i * 2];
		}

		break;
	case EVEN_TX_TO_GROUND_SHORT_RESISTOR:
		regAddr[regCount] = 0x040118;
		regData[regCount++] = 0x00;
		regAddr[regCount] = 0x040119;
		regData[regCount++] = 0x02;
		regAddr[regCount] = 0x040136;
		regData[regCount++] = 0x30;
		regAddr[regCount] = 0x0401A0;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A1;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A2;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A3;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401A4;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C0;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C1;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C2;
		regData[regCount++] = 0xff;
		regAddr[regCount] = 0x0401C3;
		regData[regCount++] = 0xff;

		/* if (testConfig.chip.I2cWriteProgModeEx(regAddr, regData, regCount) != 0) */
		/* return false; */
		for (i = 0; i < regCount; i++) {
			ret = icn85xx_prog_i2c_txdata(cts_dev, regAddr[i], &regData[i], 1);
			if (ret)
				return 0;
			udelay(10);
		}

		for (i = 0; i < TX_CHANNEL_NUM / 2; i++) {
			/* byte[] ucTemp = new byte[4]; */
			/* Array.Clear(ucTemp, 0, ucTemp.Count()); */
			memset(ucTemp, 0x00, sizeof(ucTemp));
			ucTemp[i / 8] = (u8)(1 << (i % 8));
			if (icn85xx_prog_i2c_txdata(cts_dev, 0x04011c, ucTemp, 3) != 0)
				return 0;

			/* UInt16[] temp_data = new UInt16[TX_CHANNEL_NUM]; */
			if (cts_get_adc_data(cts_dev,  temp_data, TX_CHANNEL_NUM) == 0)
				return 0;

			data[i * 2] = temp_data[i * 2];
		}

		break;
	}
	return 1;
}

int cts_get_tiny_short_data(struct cts_device *cts_dev,
			    struct tiny_short_config *tiny_short_config)
{
	int temp_num;
	u16 temp_data[64];
	u16 offset_data[64];
	u16 ref_data[64];

	u16 rx_data[RX_CHANNEL_NUM];
	u16 tx_data[TX_CHANNEL_NUM];
	/* float rx_registor[RX_CHANNEL_NUM]; */
	/* float tx_registor[TX_CHANNEL_NUM]; */
	/* float a; */
	/* float b; */
	int a, b;
	int rx_registor[RX_CHANNEL_NUM];
	int tx_registor[TX_CHANNEL_NUM];
	int i;



	if (cts_tiny_short_test_init(cts_dev) == false)
		return false;

	if (cts_get_short_resistor_data(cts_dev, temp_data, CHANNEL_OFFSET) == false)
		return false;

	for (i = 0; i < RX_CHANNEL_NUM; i += 2) {
		offset_data[i] = temp_data[i];
		if ((i + 1) < RX_CHANNEL_NUM) {
			offset_data[i + 1] = temp_data[i];
		}
	}

	if (cts_show_debug_log) {
		cts_info("offset_data");
		for (i = 0; i < RX_CHANNEL_NUM; i++)
			cts_info("%d", offset_data[i]);
	}

	/* REF DATA */
	if (cts_get_short_resistor_data(cts_dev, temp_data, INTERNAL_REFERENCE_RESISTOR) == 0)
		return 0;

	for (i = 0; i < RX_CHANNEL_NUM; i += 2) {
		ref_data[i] = temp_data[i];
		if ((i + 1) < RX_CHANNEL_NUM) {
			ref_data[i + 1] = temp_data[i];
		}
	}

	if (test_debug) {
		cts_info("ref_data");
		for (i = 0; i < RX_CHANNEL_NUM; i++)
			cts_info("%d", ref_data[i]);
	}

	/* RX DATA */
	if (cts_get_short_resistor_data(cts_dev, temp_data, ODD_RX_TO_GROUND_SHORT_RESISTOR) == 0)
		return 0;
	for (i = 0; i < RX_CHANNEL_NUM; i += 2) {
		rx_data[i] = temp_data[i];
	}
	if (cts_get_short_resistor_data(cts_dev, temp_data, EVEN_RX_TO_GROUND_SHORT_RESISTOR) == 0)
		return 0;
	for (i = 0; i < RX_CHANNEL_NUM; i += 2) {
		if ((i + 1) < RX_CHANNEL_NUM) {
			rx_data[i + 1] = temp_data[i];
		}
	}

	if (test_debug) {
		cts_info("rx_data");
		for (i = 0; i < RX_CHANNEL_NUM; i++)
			cts_info("%d", rx_data[i]);
	}

	/* TX DATA */
	if (cts_get_short_resistor_data(cts_dev, temp_data, ODD_TX_TO_GROUND_SHORT_RESISTOR) == 0)
		return 0;
	for (i = 0; i < TX_CHANNEL_NUM; i += 2) {
		tx_data[i] = temp_data[i];
	}
	if (cts_get_short_resistor_data(cts_dev, temp_data, EVEN_TX_TO_GROUND_SHORT_RESISTOR) == 0)
		return 0;
	for (i = 0; i < TX_CHANNEL_NUM; i += 2) {
		if ((i + 1) < TX_CHANNEL_NUM) {
			tx_data[i + 1] = temp_data[i];
		}
	}

	if (test_debug) {
		cts_info("tx_data");
		for (i = 0; i < TX_CHANNEL_NUM; i++)
			cts_info("%d", tx_data[i]);
	}

	for (i = 0; i < RX_CHANNEL_NUM; i++) {
		/* a = (float)fabs((float)ref_data[i] - (float)offset_data[i]); */
		/* b = (float)fabs((float)rx_data[i] - (float)offset_data[i]); */
		a = (int)((ref_data[i] >= offset_data[i])
			? (ref_data[i] - offset_data[i]) : (offset_data[i] - ref_data[i]));
		b = (int)((rx_data[i] >= offset_data[i])
			? (rx_data[i] - offset_data[i]) : (offset_data[i] - rx_data[i]));
		if (b < 1)
			b = 1;
		/* rx_registor[i] = (a / b * 2 - 1) * 100; */
		rx_registor[i] = (a * 100 / b * 2 - 100);
		if (test_debug)
			cts_info("a:%d, b:%d, rx_registor[%d]:%d", a, b, i, rx_registor[i]);
		tiny_short_config->rx_data[i] = (u32)rx_registor[i];
	}

	if (test_debug) {
		for (i = 0; i < RX_CHANNEL_NUM; i++) {
			cts_info("RX Resistor: %d ,", tiny_short_config->rx_data[i]);
		}
	}

	for (i = 0; i < TX_CHANNEL_NUM; i++) {
		/* float a = (float)fabs((float)ref_data[i] - (float)offset_data[i]); */
		/* float b = (float)fabs((float)tx_data[i] - (float)offset_data[i]); */
		a = (int)((ref_data[i] >= offset_data[i])
			? (ref_data[i] - offset_data[i]) : (offset_data[i] - ref_data[i]));
		b = (int)((tx_data[i] >= offset_data[i])
			? (tx_data[i] - offset_data[i]) : (offset_data[i] - tx_data[i]));
		if (b < 1)
			b = 1;
		/* tx_registor[i] = (a / b * 2 - 1) * 100; */
		tx_registor[i] = (a * 100 / b * 2 - 100);
		if (test_debug)
			cts_info("a:%d,b:%d,tx_registor[%d]:%d", a, b, i, tx_registor[i]);
		tiny_short_config->tx_data[i] = (u32)tx_registor[i];
	}

	if (test_debug) {
		for (i = 0; i < TX_CHANNEL_NUM; i++) {
			cts_info("TX Resistor: %d ,", tiny_short_config->tx_data[i]);
		}
	}

	return 1;
}

int cts_start_tiny_short_test(struct cts_device *cts_dev, char *buf,
			      struct tiny_short_config *tiny_short_config)
{
	u8 table[4] = { 3, 2, 1, 0 };
	int index;
	int i;
	u32 th = (u32)(tiny_short_config->tiny_short_threshold);

	tiny_short_config->tiny_short_result = true;

	for (i = 0; i < RX_CHANNEL_NUM; i++) {
		tiny_short_config->rx_status[i] = 0;
	}
	for (i = 0; i < TX_CHANNEL_NUM; i++) {
		tiny_short_config->tx_status[i] = 0;
	}

	if (cts_get_tiny_short_data(cts_dev, tiny_short_config) == 1) {
		tiny_short_config->tiny_short_resistor = 0;
		tiny_short_config->short_tx_num = 0;
		tiny_short_config->short_rx_num = 0;

		for (i = 0; i < RX_CHANNEL_NUM; i++) {
			if (tiny_short_config->rx_data[i] < th) {
				tiny_short_config->tiny_short_result  = false;
				tiny_short_config->rx_status[i] = 1;
				if (tiny_short_config->rx_data[i] > tiny_short_config->tiny_short_resistor) {
					tiny_short_config->tiny_short_resistor = (int)tiny_short_config->rx_data[i];
				}
				cts_err("RX  physical index: %d tiny short test fail resistor: %d!!!",
					i, (int)tiny_short_config->rx_data[i]);
			}
		}

		for (i = 0; i < TX_CHANNEL_NUM; i++) {
			if (tiny_short_config->tx_data[i] < th) {
				tiny_short_config->tiny_short_result  = false;
				tiny_short_config->tx_status[i] = 1;
				if (tiny_short_config->tx_data[i] > tiny_short_config->tiny_short_resistor) {
					tiny_short_config->tiny_short_resistor = (int)tiny_short_config->tx_data[i];
				}
				cts_err("TX  physical index: %d tiny short test fail resistor: %d!!!",
					i, (int)tiny_short_config->tx_data[i]);

			}
		}

		for (i = 0; i < RX_CHANNEL_NUM; i++) {
			tiny_short_config->rx_logic_data[i] = 0;
			tiny_short_config->rx_logic_status[i] = 0;
		}

		for (i = 0; i < TX_CHANNEL_NUM; i++) {
			tiny_short_config->tx_logic_data[i] = 0;
			tiny_short_config->tx_logic_status[i] = 0;
		}

		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
				"short threshold:%d\n", th);
		/* rx data */
		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt,
					PAGE_SIZE * 2 - cts_log.log_cnt, "rx_logic_data:");

		for (i = 0; i < tiny_short_config->u8col_num; i++) {
			tiny_short_config->rx_logic_data[i] =
				tiny_short_config->rx_data[tiny_short_config->rx_order[i]];
			tiny_short_config->rx_logic_status[i] =
				tiny_short_config->rx_status[tiny_short_config->rx_order[i]];
			if (tiny_short_config->rx_logic_status[i]) {
				tiny_short_config->short_rx_num++;
				cts_err("Logic order: RX%d tiny short test fail resistor: %d !!!",
					i + 1, tiny_short_config->rx_logic_data[i]);
			}

			if ((i % 10) == 0) {
				cts_log.log_cnt += snprintf(buf + cts_log.log_cnt,
						PAGE_SIZE * 2 - cts_log.log_cnt, "\n");
			}
			cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
				"%-8d", tiny_short_config->rx_logic_data[i]);
		}

		for (i = 0; i < tiny_short_config->u8col_num; i++) {
			if (tiny_short_config->rx_logic_status[i]) {
				cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
					"\nLogic order: RX%d tiny short test fail resistor: %d !!!\n",
							   i + 1, tiny_short_config->rx_logic_data[i]);
			}
		}
		/* tx data */
		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt, "\ntx_logic_data:");

		for (i = 0; i < tiny_short_config->u8row_num; i++) {
			if (tiny_short_config->tx_order[i] > TX_CHANNEL_NUM - 2) {
				table[0] = 3;
				table[1] = 2;
				table[2] = 1;
				table[3] = 0;
				index = (RX_CHANNEL_NUM - 4) + table[(tiny_short_config->tx_order[i]
					- (TX_CHANNEL_NUM - 1))];
				tiny_short_config->tx_logic_data[i] = tiny_short_config->rx_data[index];
				tiny_short_config->tx_logic_status[i] = tiny_short_config->rx_status[index];
			} else {
				tiny_short_config->tx_logic_data[i] =
					tiny_short_config->tx_data[tiny_short_config->tx_order[i]];
				tiny_short_config->tx_logic_status[i] =
					tiny_short_config->tx_status[tiny_short_config->tx_order[i]];
			}
			if (tiny_short_config->tx_logic_status[i]) {
				tiny_short_config->short_tx_num++;
				cts_err("Logic order: TX%d tiny short test fail resistor: %d !!!",
					i + 1, tiny_short_config->tx_logic_data[i]);
			}

			if ((i % 10) == 0) {
				cts_log.log_cnt += snprintf(buf + cts_log.log_cnt,
						PAGE_SIZE * 2 - cts_log.log_cnt, "\n");
			}
			cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
				"%-8d", tiny_short_config->tx_logic_data[i]);

		}

		for (i = 0; i < tiny_short_config->u8row_num; i++) {
			if (tiny_short_config->tx_logic_status[i]) {
				cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
					"\nLogic order: TX%d tiny short test fail resistor: %d !!!",
							   i + 1, tiny_short_config->tx_logic_data[i]);
			}
		}
		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt,
				PAGE_SIZE * 2 - cts_log.log_cnt, "\n");

	} else {
		tiny_short_config->tiny_short_result = false;
		return -EPERM;
	}

	return 0;
}
#endif

int cts_rawdata_alu(struct cts_device *cts_dev, char *buf, s16 *rawdata, u16 threshold_min,
		    u16 threshold_max, u8 is_open_test)
{
#define RAWDATA_BUFFER_SIZE(cts_dev) \
	(cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)

	s16 *result = NULL;
	int i, j, ret = 0;
	u16 ng_num = 0;
	u16 th_min, th_max;
	u16 count = 0;

	cts_info("rawdata process");
	result = kmalloc(RAWDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
	if (result == NULL) {
		cts_err("Allocate memory for rawdata failed");
		return -EPERM;
	}

	if (is_open_test) {
		th_min = threshold_min;
		th_max = 0xffff;
	} else {
		th_min = threshold_min;
		th_max = threshold_max;
	}
	count += snprintf(buf + count, PAGE_SIZE * 2 - count, "th_min:%d[0x%x], th_max: %d[0x%x]\n",
		th_min, th_min, th_max, th_max);
	for (i = 0; i < cts_dev->fwdata.rows; i++) {
		for (j = 0; j < cts_dev->fwdata.cols; j++) {
			if ((rawdata[i * cts_dev->fwdata.cols + j] < th_min)
			    || (rawdata[i * cts_dev->fwdata.cols + j] > th_max)) {
				result[i * cts_dev->fwdata.cols + j] = 1;
				ng_num++;
				cts_err("Test node:[TX%d,RX%d]:%d FAIL", i + 1, j + 1,
					rawdata[i * cts_dev->fwdata.cols + j]);
			}
			count += snprintf(buf + count, PAGE_SIZE * 2 - count,
				"%-6d", rawdata[i * cts_dev->fwdata.cols + j]);
		}
		count += snprintf(buf + count, PAGE_SIZE * 2 - count, "\n");
	}
	cts_info("th_min:%d[0x%x], th_max: %d[0x%x] ng_num=%d", th_min, th_min, th_max, th_max, ng_num);

	/* open test: result save as format: tx[rows]+rx[cols] */
	if (is_open_test && ng_num) {
		/* tx open test... */
		cts_info("TX open test");
		count += snprintf(buf + count, PAGE_SIZE * 2 - count, "TX open test...\n");
		for (i = 0; i < cts_dev->fwdata.rows; i++) {
			result[i] = 0;
			for (j = 0; j < cts_dev->fwdata.cols; j++) {
				if ((rawdata[i * cts_dev->fwdata.cols + j] < th_min)
				    || (rawdata[i * cts_dev->fwdata.cols + j] > th_max)) {
					result[i]++;
				}
				if (j == cts_dev->fwdata.cols - 1) {
					if (result[i] == cts_dev->fwdata.cols) {
						result[i] = 1;
						ret++;
						cts_err("TX%d open test fail", i + 1);
						count += snprintf(buf + count, PAGE_SIZE * 2 - count,
							"TX%d open test fail\n", i + 1);
					} else {
						result[i] = 0;
					}
				}
			}
		}
		/* rx open test... */
		cts_info("RX open test");
		count += snprintf(buf + count, PAGE_SIZE * 2 - count, "RX open test...\n");
		for (j = 0; j < cts_dev->fwdata.cols; j++) {
			result[cts_dev->fwdata.rows + j] = 0;
			for (i = 0; i < cts_dev->fwdata.rows; i++) {
				if ((rawdata[i * cts_dev->fwdata.cols + j] < th_min)
				    || (rawdata[i * cts_dev->fwdata.cols + j] > th_max)) {
					result[cts_dev->fwdata.rows + j]++;
				}
				if (i == cts_dev->fwdata.rows - 1) {
					if (result[cts_dev->fwdata.rows + j] == cts_dev->fwdata.rows) {
						result[cts_dev->fwdata.rows + j] = 1;
						ret++;
						cts_err("RX%d open test fail", j + 1);
						count += snprintf(buf + count, PAGE_SIZE * 2 - count,
							"RX%d open test fail\n", j + 1);
					} else {
						result[cts_dev->fwdata.rows + j] = 0;
					}
				}
			}
		}
	} else {
		/* rawdata test */
		ret =  ng_num;
	}
	cts_log.log_cnt += count;

	kfree(result);

	return ret;
#undef RAWDATA_BUFFER_SIZE
}

/* Return 0 success
	negative value while error occurs
	positive value means how many nodes fail */
int cts_rawdata_process(struct cts_device *cts_dev,  char *buf,
			u16 threshold_min, u16 threshold_max, u8 is_open_test)
{
#define RAWDATA_BUFFER_SIZE(cts_dev) \
	(cts_dev->fwdata.rows * cts_dev->fwdata.cols * 2)

	u16 *rawdata = NULL;
	/* u8 row_index[128]={0}; */
	/* u8 col_index[128]={0}; */

	int  ret;

	cts_info("cts rawdata process rows=%d cols=%d, RAWDATA_BUFFER_SIZE=%d",
		cts_dev->fwdata.rows, cts_dev->fwdata.cols, RAWDATA_BUFFER_SIZE(cts_dev));
	rawdata = kmalloc(RAWDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
	if (rawdata == NULL) {
		cts_err("Allocate memory for rawdata failed");
		return -ENOMEM;
	}
	cts_stop_device(cts_dev);
	cts_dev->rtdata.testing = true;

	ret = cts_enable_get_rawdata(cts_dev);
	if (ret) {
		cts_err("Enable read raw data failed %d", ret);
		goto err_free_rawdata;
	}

	ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
	if (ret) {
		cts_err("Send cmd QUIT_GESTURE_MONITOR failed %d", ret);
		goto err_free_rawdata;
	}
	msleep(50);

	ret = cts_get_rawdata(cts_dev, rawdata);
	if (ret) {
		cts_err("Get raw data failed %d", ret);
		/* Fall through to disable get rawdata */
	}
	ret = cts_disable_get_rawdata(cts_dev);
	if (ret) {
		cts_err("Disable read raw data failed %d", ret);
		/* Fall through to show rawdata */
	}

	ret = cts_rawdata_alu(cts_dev, buf, rawdata, threshold_min, threshold_max, is_open_test);

err_free_rawdata:
	kfree(rawdata);
	cts_dev->rtdata.testing = false;
	cts_start_device(cts_dev);

	return ret;

#undef RAWDATA_BUFFER_SIZE

}


int cts_parse_test_para(char *test_config,  char *name, u16 *th)
{
	char *item_name;
	char *item_value;
	char *temp_ptr;
	char temp[128];
	u16 value;
	int count = 0;
	u16 len;
	int ret;

	item_name = strnstr(test_config, name, strlen(test_config));
	if (item_name) {
		len = strlen(name);
		cts_info("parse para name: %s len: %d", name, len);
		if (len > sizeof(temp)) {
			cts_info("parse para name too long!!! len: %d", len);
			return false;
		}

		item_value = strnchr(item_name, strlen(item_name), '=');/* item_name + len +1; */
		if (item_value == NULL) {
			cts_err("parse para value not found");
			return false;
		}

		item_value += 1;
		count = 0;
		while (item_value[count] != '\r'
		       && item_value[count] != '\n'
		       && item_value[count] != '\0') {
			count++;
		}
		if (count + 1 >= sizeof(temp)) {
			cts_info("parse para value too long!!! len: %d", count);
			return false;
		}

		strlcpy((char *)&temp[0], item_value, count + 1);
		temp_ptr = &temp[0];
		temp_ptr[count] = '\0';
		temp_ptr = strim(temp);

		cts_info("parse para value :%s count: %d", temp_ptr, count);

		if (isdigit(temp_ptr[0])) {
			ret = kstrtou16(temp_ptr, 0, &value);
			cts_info("parse para:%s = %d [0x%x]", name, value, value);
			if (ret) {
				cts_err("parse para value error!!!");
				return false;
			}
			*th = value;
			return true;
		}

		cts_err("parse para value invalid:%c", temp_ptr[0]);
	} else {
		cts_err("parse para : %s not found in config file", name);
	}

	return false;
}

int cts_fw_version_test(struct cts_device *cts_dev, u16 fw_ver)
{
	u16 version;
	int ret;
	char *buf = NULL;
	char result_log_path[MAX_FILE_FULL_PATH_LEN] = {0};

	cts_info("cts firmware version test");

	cts_info("request log memery size: %ld", PAGE_SIZE);
	buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (buf == NULL) {
		cts_err("allocate memery for firmware version test log fail");
		return -ENOMEM;
	}
	cts_log.log_cnt = 0;
	cts_log.log_cnt += snprintf(buf, PAGE_SIZE, "\nStart firmware version test...\n");

	snprintf(result_log_path, MAX_FILE_FULL_PATH_LEN - 1, "%s%s",
		save_file_path, save_test_result_log_name);
	ret = cts_get_firmware_version(cts_dev, &version);
	if (ret) {
		cts_err("cts firmware version test error");
		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE - cts_log.log_cnt,
			"cts firmware version test error\n");
		cts_test_save_log(cts_dev, result_log_path, buf);
		kfree(buf);
		return ret;
	}
	if (fw_ver ==  version) {
		cts_info("cts firmware version: 0x%x test PASS", version);
		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE - cts_log.log_cnt,
			"cts firmware version: 0x%x test PASS\n", version);
		cts_test_save_log(cts_dev, result_log_path, buf);
		kfree(buf);
		return 0;
	}

	cts_info("cts firmware version test FAIL");
	cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE - cts_log.log_cnt,
				   "cts firmware current version: 0x%x!= 0x%x test FAIL\n", version, fw_ver);
	cts_test_save_log(cts_dev, result_log_path, buf);
	kfree(buf);
	return -EPERM;
}

/* Return 0 success
	negative value while error occurs
	positive value means how many nodes fail */
int cts_short_test(struct cts_device *cts_dev, u16 threshold)
{
	int  ret;
	int  err_num = 0;
	char *buf = NULL;
	struct tiny_short_config *tiny_short_config = NULL;
	char result_log_path[MAX_FILE_FULL_PATH_LEN] = {0};

	cts_info("cts short test");
	cts_stop_device(cts_dev);

	cts_dev->rtdata.testing = true;
	tiny_short_config = kzalloc(sizeof(*tiny_short_config), GFP_KERNEL);
	if (tiny_short_config == NULL) {
		cts_err("Allocate tiny_short_config failed");
		return -ENOMEM;
	}

	cts_info("request short test log memery size: %ld", PAGE_SIZE);
	buf = kzalloc(PAGE_SIZE * 2, GFP_KERNEL);
	if (buf == NULL) {
		cts_err("allocate memery for short test log fail");
		return -ENOMEM;
	}

	tiny_short_config->u8row_num = cts_dev->fwdata.rows;
	tiny_short_config->u8col_num = cts_dev->fwdata.cols;
	tiny_short_config->tiny_short_threshold = threshold;
	tiny_short_config->tiny_short_result = false;

	ret = cts_get_para_rx_order(cts_dev, tiny_short_config->rx_order);
	if (ret) {
		cts_err("get para rx order fail");
	}
	ret = cts_get_para_tx_order(cts_dev, tiny_short_config->tx_order);
	if (ret) {
		cts_err("get para tx order fail");
	}

	cts_enter_program_mode(cts_dev);

	cts_log.log_cnt = 0;
	cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
		"\nStart cts short test...\n");

#if (ICNT86xx_TYPE == CTS_CHIP_TYPE)
	ret = icnt86_start_tiny_short_test(cts_dev, buf, tiny_short_config);
#else
	ret = cts_start_tiny_short_test(cts_dev, buf, tiny_short_config);
#endif
	err_num = tiny_short_config->short_tx_num + tiny_short_config->short_rx_num;

	if (ret) {
		cts_info("!!! Cts short test error !!!");
		cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
			"!!! Cts short test error !!!\n");
		ret = -1;
	} else {
		if (tiny_short_config->tiny_short_result) {
			cts_info("Cts short test PASS !");
			cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
				"!!! Cts short test PASS !!!\n");
			ret = 0;
		} else {
			cts_info("!!! Cts short test Fail  !!!");
			cts_log.log_cnt += snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt,
				"!!! Cts short test Fail !!!\n");
			ret = err_num;
		}
	}
	snprintf(result_log_path, MAX_FILE_FULL_PATH_LEN - 1, "%s%s",
		save_file_path, save_test_result_log_name);
	cts_test_save_log(cts_dev, result_log_path, buf);
	kfree(buf);

	cts_dev->rtdata.testing = false;

	cts_plat_reset_device(cts_dev->pdata);

	cts_enter_normal_mode(cts_dev);

	cts_start_device(cts_dev);

	kfree(tiny_short_config);

	return ret;

}

/* Return 0 success
	negative value while error occurs
	positive value means how many nodes fail */

int cts_rawdata_test(struct cts_device *cts_dev, u16 th_min, u16 th_max)
{
	int ret;
	char *buf = NULL;
	char result_log_path[MAX_FILE_FULL_PATH_LEN] = {0};
	/* u16 count = 0; */

	cts_info("cts rawdata test");

	cts_info("request rawdata test log memery size: %ld", PAGE_SIZE);
	buf = kzalloc(PAGE_SIZE * 2, GFP_KERNEL);
	if (buf == NULL) {
		cts_err("allocate memery for rawdata test log fail");
		return -ENOMEM;
	}

	cts_log.log_cnt = 0;
	cts_log.log_cnt += snprintf(buf, PAGE_SIZE * 2, "\nStart rawdata test...\n");

	ret = cts_rawdata_process(cts_dev, buf + cts_log.log_cnt, th_min, th_max, 0);
	if (ret < 0) {
		cts_info("!!! Cts rawdata test error !!!");
		snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt, "!!! Cts rawdata test error !!!\n");
	} else if (ret > 0) {
		cts_info("!!! Cts rawdata test FAIL  !!!");
		snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt, "!!! Cts rawdata test FAIL !!!\n");
	} else {
		cts_info("!!! Cts rawdata test PASS  !!!");
		snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt, "!!! Cts rawdata test PASS !!!\n");
	}

	snprintf(result_log_path, MAX_FILE_FULL_PATH_LEN - 1, "%s%s",
		save_file_path, save_test_result_log_name);
	cts_test_save_log(cts_dev, result_log_path, buf);
	kfree(buf);
	return ret;
}

/* Return 0 success
	negative value while error occurs
	positive value means how many nodes fail */

int cts_open_test(struct cts_device *cts_dev, u16 th)
{
	int ret;
	char *buf = NULL;
	char result_log_path[MAX_FILE_FULL_PATH_LEN] = {0};
	/* u16 count = 0; */

	cts_info("cts open test");

	cts_info("request log memery size: %ld", PAGE_SIZE);
	buf = kzalloc(PAGE_SIZE * 2, GFP_KERNEL);
	if (buf == NULL) {
		cts_err("allocate memery for open test log fail");
		return -ENOMEM;
	}
	cts_log.log_cnt = 0;
	cts_log.log_cnt += snprintf(buf, PAGE_SIZE * 2, "\nStart open test...\n");

	ret = cts_rawdata_process(cts_dev, buf + cts_log.log_cnt, th, 0xffff, 1);
	if (ret < 0) {
		cts_info("!!! Cts open test error !!!");
		snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt, "!!! Cts open test error !!!\n");
	} else if (ret > 0) {
		cts_info("!!! Cts open test FAIL  !!!");
		snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt, "!!! Cts open test FAIL !!!\n");
	} else {
		cts_info("!!! Cts open test PASS  !!!");
		snprintf(buf + cts_log.log_cnt, PAGE_SIZE * 2 - cts_log.log_cnt, "!!! Cts open test PASS !!!\n");
	}

	snprintf(result_log_path, MAX_FILE_FULL_PATH_LEN - 1, "%s%s",
		save_file_path, save_test_result_log_name);
	cts_test_save_log(cts_dev, result_log_path, buf);
	kfree(buf);
	return ret;
}

int cts_fwversion_test(struct cts_device *cts_dev, u16 version)
{
	if (cts_dev->fwdata.version == version) {
		return 0;
	}
	return -EPERM;
}


#ifdef CFG_CTS_HAS_RESET_PIN
int cts_reset_test(struct cts_device *cts_dev)
{
	int ret = 0;
	int val = 0;

	cts_lock_device(cts_dev);
	ret = cts_stop_device(cts_dev);
	if (ret) {
		cts_err("Stop device failed %d", ret);
	}
	/* cts_plat_set_reset(cts_dev->pdata, 0); */
	mdelay(50);

	/* Check whether device is in normal mode */
	if (!cts_plat_is_i2c_online(cts_dev->pdata, CTS_NORMAL_MODE_I2CADDR)) {
		val++;
	}
	/* cts_plat_set_reset(cts_dev->pdata, 1); */
	mdelay(50);

	/* Check whether device is in normal mode */
	if (!cts_plat_is_i2c_online(cts_dev->pdata, CTS_NORMAL_MODE_I2CADDR)) {
		val++;
	}

	ret = cts_start_device(cts_dev);
	if (ret) {
		cts_err("Stop device failed %d", ret);
	}
#ifdef CONFIG_CTS_CHARGER_DETECT
	if (cts_is_charger_exist(cts_dev)) {
		cts_charger_plugin(cts_dev);
	}
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_GLOVE
	if (cts_is_glove_enabled(cts_dev)) {
		cts_enter_glove_mode(cts_dev);
	}
#endif

	cts_unlock_device(cts_dev);
	if (val == 2) {
		ret = cts_enter_normal_mode(cts_dev);
		if (ret) {
			cts_err("Enter normal mode failed %d", ret);
			return ret;
		}
		return 0;
	}
	return ret;
}
#endif


int cts_test(struct cts_device *cts_dev, const char *filepath)
{
#define CTS_TEST_PASS   \
"****************!!!PASSED!!!****************"

#define CTS_TEST_FAIL   \
"****************!!!FAILED!!!****************"

	struct file *file = NULL;
	char *test_config;
	u16 th;
	u16 size;
	int i, ret;
	char *pass = CTS_TEST_PASS;
	char *fail = CTS_TEST_FAIL;
	char *buf = NULL;
	loff_t pos = 0;
	char result_log_path[MAX_FILE_FULL_PATH_LEN] = {0};

	cts_info("Cts test");

	cts_stop_device(cts_dev);
	cts_dev->rtdata.testing = true;

	if (filepath == NULL) {
		test_config = cts_test_config_data;
		cts_info("Use cts test configure bultin driver");
	} else {
		cts_info("Open cts test cfg file '%s'", filepath);
		file = filp_open(filepath, O_RDONLY, 0);
		if (IS_ERR(file)) {
			cts_err("Open file '%s' failed %ld", filepath, PTR_ERR(file));
			return -EPERM;
		}

		test_config = kzalloc(1024, GFP_KERNEL);
		if (test_config == NULL) {
			cts_err("Allocate test_config memery failed");
			goto cts_cfg_file_err;
			return -ENOMEM;
		}

		size = file_inode(file)->i_size;
		if (size <= 0 && size > 1024) {
			cts_err("Cts test config file invalid");
			goto cts_cfg_file_err;
		}

#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
		ret = kernel_read(file, test_config, size, &pos);
#else
		ret = kernel_read(file, pos, test_config, size);
#endif
		if (ret < 0) {
			cts_err("Read test config file error");
			goto cts_cfg_file_err;
		}
	}

	cts_info("cts_test_config:\n%s", test_config);

	for (i = 0; i < ARRAY_SIZE(cts_test_items); i++) {
		cts_info("cts test item[----step %d----] : %s", i, cts_test_items[i].item_name);
		ret = cts_parse_test_para(test_config, cts_test_items[i].item_name, &th);
		if (ret) {
			cts_test_items[i].need_test = true;
		} else {
			cts_test_items[i].need_test = false;
		}

		if (cts_test_items[i].need_test) {
			if (cts_test_items[i].para.para1) {
				ret = cts_parse_test_para(test_config, cts_test_items[i].para.para1, &th);
				if (ret) {
					cts_test_items[i].para.threshold1 = th;
					cts_info("Get test item para1: %s threshold1 = %d [0x%x]",
						cts_test_items[i].item_name, th, th);
				} else {
					cts_err("Get test item para1: %s error!!! ", cts_test_items[i].item_name);
					continue;
				}
			}
			if (cts_test_items[i].para.para2) {
				ret = cts_parse_test_para(test_config, cts_test_items[i].para.para2, &th);
				if (ret) {
					cts_test_items[i].para.threshold2 = th;
					cts_info("Get test item para2: %s threshold2 = %d [0x%x]",
							cts_test_items[i].item_name, th, th);
				} else {
					cts_err("Get test item para2: %s error!!! ", cts_test_items[i].item_name);
					continue;
				}
			}

			cts_test_items[i].result = cts_test_items[i].cts_test_fun(cts_dev,
				cts_test_items[i].para.threshold1, cts_test_items[i].para.threshold2);
		}
		cts_info(" ");
	}

	for (i = 0; i < ARRAY_SIZE(cts_test_items); i++) {
		if (cts_test_items[i].need_test && cts_test_items[i].result) {
			break;
		}
	}
	if (i == ARRAY_SIZE(cts_test_items)) {
		cts_info(CTS_TEST_PASS);
		buf = pass;
	} else {
		cts_info(CTS_TEST_FAIL);
		buf = fail;
	}

	snprintf(result_log_path, MAX_FILE_FULL_PATH_LEN - 1, "%s%s",
		save_file_path, save_test_result_log_name);
	cts_test_save_log(cts_dev, result_log_path, buf);

	if (filepath != NULL) {
		kfree(test_config);
		ret = filp_close(file, NULL);
		if (ret) {
			cts_warn("Close file '%s' failed %d", filepath, ret);
		}
	}

	cts_dev->rtdata.testing = false;

	cts_plat_reset_device(cts_dev->pdata);

	cts_enter_normal_mode(cts_dev);

	cts_start_device(cts_dev);

	return 0;

cts_cfg_file_err:

	kfree(test_config);

	ret = filp_close(file, NULL);
	if (ret) {
		cts_warn("Close file '%s' failed %d", filepath, ret);
	}
	return -ENOMEM;

#undef CTS_TEST_PASS
#undef CTS_TEST_FAIL
}

int cts_test_getrawdata(struct cts_device *cts_dev, u16 *rawdata)
{
	int ret = 0;
	int i = 0;

	ret = cts_stop_device(cts_dev);
	if (ret < 0) {
		return -EPERM;
	}
	ret = cts_enable_get_rawdata(cts_dev);
	if (ret) {
		cts_err("can not enable get rawdata");
		ret = -EPERM;
		goto err_free_rawdata;
	}

	ret = cts_send_command(cts_dev, CTS_CMD_QUIT_GESTURE_MONITOR);
	if (ret) {
		cts_err("send quit gesture monitor err");
	}

	msleep(50);
	for (i = 0; i < 3; i++) {
		ret = cts_get_rawdata(cts_dev, rawdata);
		if (ret) {
			cts_err("get rawdata err");
		} else {
			break;
		}
		mdelay(30);
	}
	ret = cts_disable_get_rawdata(cts_dev);
	if (ret) {
		cts_err("disable get rawdata err");
		ret = -EPERM;
	}

err_free_rawdata:
	cts_start_device(cts_dev);
	return ret;

}

