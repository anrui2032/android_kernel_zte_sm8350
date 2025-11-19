/*
 *drivers/input/touchscreen/Hynitron_ex_fun.c
 *
 *FocalTech IC driver expand function for debug.
 *
 *Copyright (c) 2010  Focal tech Ltd.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *Note:the error code of EIO is the general error in this file.
 */

#include <linux/netdevice.h>
#include <linux/mount.h>
/* #include <linux/netdevice.h> */
#include <linux/proc_fs.h>
#include "hynitron.h"
#include "hynitron_ex_fun.h"
#include "hynitron_ctl.h"
#include  "hynitron_config.h"

#include <linux/gpio.h>
/* #include <soc/sprd/board.h> */
/* #include "../driver_config.h" */

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/firmware.h>

struct i2c_client *client_up;

extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern  void cst8xx_ts_reset(void);
extern u8 tp_fm_ver;
extern struct cst8xx_ts_data *g_cst8xx_ts;

#if ANDROID_TOOL_SURPORT
static unsigned short g_unnormal_mode = 0;
#endif

#define REG_LEN_1B	1
#define REG_LEN_2B	2

#if SYSFS_DEBUG
static struct mutex g_device_mutex;
static DEFINE_MUTEX(g_device_mutex);
static struct kobject *k_obj = NULL;
static int hyn_updatefw_by_sysfs(u8 *dir, int name_len);
#endif
/*
*hyn_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/
int hyn_i2c_Read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = 1,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			hyn_err("%s: i2c read error.\n", __func__);
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 1,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			hyn_err("%s:i2c read error.\n", __func__);
	}
	return ret;
}
/*write data by i2c*/

int hyn_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		hyn_err("%s i2c write error.\n", __func__);
	return ret;
}

int hyn_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
	unsigned char buf[2] = {0};

	buf[0] = regaddr;
	buf[1] = regvalue;

	return hyn_i2c_Write(client, buf, sizeof(buf));
}

int hyn_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
	return hyn_i2c_Read(client, &regaddr, 1, regvalue, 1);
}

#if HYN_EN_AUTO_UPDATE
static unsigned char dev_addr;
static unsigned char update_fw_flag;
static unsigned char chip_sumok_flag;
extern unsigned char *p_cst836u_upgrade_firmware;
extern unsigned char  apk_upgrade_flag;
#endif

/*****************************************************************/
/*
 *
 */
int hctp_write_bytes(unsigned short reg, unsigned char *buf, unsigned short len, unsigned char reg_len)
{
	int ret;
	unsigned char mbuf[600];

	if (reg_len == 1) {
		mbuf[0] = reg;
		memcpy(mbuf+1, buf, len);
	} else {
		mbuf[0] = reg>>8;
		mbuf[1] = reg;
		memcpy(mbuf+2, buf, len);
	}
	ret = hyn_i2c_Write(client_up, mbuf, len+reg_len);
	if (ret < 0) {
		hyn_err("%s i2c write error.\n", __func__);
	}
	return ret;
}

int hctp_read_bytes(unsigned short reg, unsigned char *buf, unsigned short len, unsigned char reg_len)
{
	int ret;
	unsigned char reg_buf[2];

	if (reg_len == 1) {
		reg_buf[0] = reg;
	} else {
		reg_buf[0] = reg>>8;
		reg_buf[1] = reg;
	}

	ret = hyn_i2c_Read(client_up, reg_buf, reg_len, buf, len);
	if (ret < 0) {
		hyn_err("%s: i2c read error.\n", __func__);
	}
	return ret;
}

/*****************************************************************/

int cst78xx_enter_bootmode(void)
{
	char retryCnt = 10;

	cst8xx_ts_reset();
	mdelay(5);
	while (retryCnt--) {
		u8 cmd[3];

		cmd[0] = 0xAA;
		if (-1 == hctp_write_bytes(0xA001, cmd, 1, REG_LEN_2B)) {  /* enter program mode */
			mdelay(2); /* 4ms */
			continue;
		}
		if (-1 == hctp_read_bytes(0xA003, cmd, 1, REG_LEN_2B)) { /* read flag */
			mdelay(2); /* 4ms */
			continue;
		} else {
			if (cmd[0] != 0x55) {
				msleep(20); /* 4ms */
				continue;
			} else {
				return 0;
			}
		}
	}
	return -EPERM;
}

#if HYN_EN_AUTO_UPDATE

#if HYN_EN_AUTO_UPDATE_CST78xx
static int cst78xx_update(u16 startAddr, u16 len, u8 *src)
{
	u16 sum_len;
	u8 cmd[10];
	int ret;

	ret = 0;
	if (cst78xx_enter_bootmode() == -1) {
		return -EPERM;
	}
	sum_len = 0;

 #define PER_LEN	512
	do {
		if (sum_len >= len) {
			return -EPERM;
		}

		/* send address */
		cmd[1] = startAddr>>8;
		cmd[0] = startAddr&0xFF;
		hctp_write_bytes(0xA014, cmd, 2, REG_LEN_2B);
	#if HYN_IIC_TRANSFER_LIMIT
			{
				u8 temp_buf[8];
				u16 j, iic_addr;

				iic_addr = 0;
				for (j = 0; j < 128; j++) {
					temp_buf[0] = *((u8 *)src+iic_addr+0);
					temp_buf[1] = *((u8 *)src+iic_addr+1);
					temp_buf[2] = *((u8 *)src+iic_addr+2);
					temp_buf[3] = *((u8 *)src+iic_addr+3);

					hctp_write_bytes((0xA018+iic_addr), (u8 *)temp_buf, 4, REG_LEN_2B);
					iic_addr += 4;
				if (iic_addr == 512)
					break;
				}

			}
	#else
				hctp_write_bytes(0xA018, src, PER_LEN, REG_LEN_2B);
	#endif


		cmd[0] = 0xEE;
		hctp_write_bytes(0xA004, cmd, 1, REG_LEN_2B);

		if (apk_upgrade_flag == 0)
			msleep(300);
		else
			msleep(100);

		{
			u8 retrycnt = 50;

			while (retrycnt--) {
				cmd[0] = 0;
				hctp_read_bytes(0xA005, cmd, 1, REG_LEN_2B);
				if (cmd[0] == 0x55) {
					/* success */
					break;
				}
				msleep(20);
			}

			if (cmd[0] != 0x55) {
				ret = -1;
			}
		}
		startAddr += PER_LEN;
		src	   += PER_LEN;
		sum_len   += PER_LEN;
	} while (len);

	/* exit program mode */
	cmd[0] = 0x00;
	hctp_write_bytes(0xA003, cmd, 1, REG_LEN_2B);

	return ret;
}

static u32 cst78xx_read_checksum(u16 startAddr, u16 len)
{
	union{
		u32 sum;
		u8 buf[4];
	} checksum;
	char cmd[3];
	char readback[4] = {0};

	if (cst78xx_enter_bootmode() == -1) {
		return -EPERM;
	}

	cmd[0] = 0;
	if (-1 == hctp_write_bytes(0xA003, cmd, 1, REG_LEN_2B)) {
		return -EPERM;
	}
	msleep(500);

	if (-1 == hctp_read_bytes(0xA000, readback, 1, REG_LEN_2B)) {
		return -EPERM;
	}
	if (readback[0] != 1) {
		return -EPERM;
	}
	if (-1 == hctp_read_bytes(0xA008, checksum.buf, 4, REG_LEN_2B)) {
		return -EPERM;
	}
	chip_sumok_flag  = 1;

	return checksum.sum;
}
#endif

static void read_fw_version(struct i2c_client *mclient)
{
	int ret;
	unsigned char reg_buf[2];
	unsigned char buf[4];
	unsigned short fwversion, chipversion;

	msleep(100);

	reg_buf[0] = 0xA6;
	ret = hyn_i2c_Read(client_up, reg_buf, 1, buf, 2);

	if (ret < 0) {
		hyn_err("%s: i2c read error.\n", __func__);
		return;
	}

	fwversion   = *(p_cst836u_upgrade_firmware+0x3BFD+6);
	fwversion <<= 8;
	fwversion  += *(p_cst836u_upgrade_firmware+0x3BFC+6);

	hyn_info("hyn fwversion: %x\n", fwversion);

	chipversion   = buf[1];
	chipversion <<= 8;
	chipversion  += buf[0];

	hyn_info("hyn chipversion: %x\n", chipversion);

	if (chipversion > fwversion) {
		update_fw_flag = 0;
		hyn_info("hyn update_fw_flag: %x\n", update_fw_flag);
	}
}

int ctp_hynitron_update(struct i2c_client *mclient)
{
	unsigned short startAddr;
	unsigned short length;
	unsigned short checksum;
	unsigned short chipchecksum;

	update_fw_flag  = 1;
	chip_sumok_flag = 0;

	client_up = mclient;

	if (apk_upgrade_flag == 0)
		read_fw_version(mclient);

	dev_addr  = client_up->addr;

#if HYN_EN_AUTO_UPDATE_CST78xx

	client_up->addr = 0x6A;

	if (cst78xx_enter_bootmode() == 0) {
		startAddr = *(p_cst836u_upgrade_firmware+1);

		length = *(p_cst836u_upgrade_firmware+3);

		checksum = *(p_cst836u_upgrade_firmware+5);

		startAddr <<= 8;
		startAddr |= *(p_cst836u_upgrade_firmware+0);

		length <<= 8;
		length |= *(p_cst836u_upgrade_firmware+2);

		checksum <<= 8;
		checksum |= *(p_cst836u_upgrade_firmware+4);

		chipchecksum = cst78xx_read_checksum(startAddr, length);

		if (update_fw_flag || chip_sumok_flag == 0) {
			hyn_info("%s low version,  updating!!!\n", __func__);
			hyn_info("cst78xx File, start-0x%04x len-0x%04x fileCheck-0x%04x\n", startAddr,
				   length, checksum);
			if (chipchecksum != checksum) {
				cst78xx_update(startAddr, length, (p_cst836u_upgrade_firmware+6));
				length = cst78xx_read_checksum(startAddr, length);
				hyn_info("cst78xx update %s, checksum-0x%04x",
					   ((length == checksum) ? "success" : "fail"), length);

			} else {
				hyn_info("cst78xx check pass...");
			}
		} else {
			hyn_info("%s high version  not update!!!r\n", __func__);
		}
	} else {
		client_up->addr = dev_addr;
		return -EPERM;
	}
#endif

	client_up->addr = dev_addr;
	cst8xx_ts_reset();
	msleep(50);
	hyn_read_reg(client_up, HYN_REG_FW_VER, &tp_fm_ver);

	return 0;
}

int check_binfile_valid(unsigned char *pbinbuffer, int bin_len, int chip_type)
{
	unsigned short	addr	  = 0;
	unsigned short	check_sum = 0x55;
	unsigned short	chip_sum, temp;

	if (chip_type == 0) {	/* cst836u */
		if (bin_len != (15 * 1024 + 6) || pbinbuffer == NULL) {
			hyn_info("error,binfile is crashed\n");
			return -EPERM;
		}

		addr	  = pbinbuffer[1];
		addr	<<= 8;
		addr	 += pbinbuffer[0];

		temp	  = pbinbuffer[3];
		temp	<<= 8;
		temp	 += pbinbuffer[2];

		check_sum  = pbinbuffer[5];
		check_sum <<= 8;
		check_sum += pbinbuffer[4];

		chip_sum   = pbinbuffer[1024*15+5];
		chip_sum <<= 8;
		chip_sum  += pbinbuffer[1024*15+4];

		hyn_info("start=%d,len=%d,checksum=%d,endchecksum=%d\n", addr, temp, check_sum,
			   chip_sum);

		if (addr != 0x0000 || temp != (15 * 1024) || check_sum != chip_sum) {
			hyn_info("error,binfile is crashed\n");
			return -ENOENT;
		}

		addr = 0;
		check_sum   = 0x55;
		pbinbuffer += 6;
		do {
			 check_sum += *pbinbuffer;
			 temp = check_sum>>15;
			 check_sum <<= 1;
			 check_sum |= temp;
			 addr++;
			 pbinbuffer++;
		} while (addr < (15*1024-2));

		chip_sum   = pbinbuffer[1];
		chip_sum <<= 8;
		chip_sum  += pbinbuffer[0];

		hyn_info("pbinbuffer[1]=%x,pbinbuffer[0]=%x\n", pbinbuffer[1], pbinbuffer[0]);
		if (check_sum != chip_sum) {
			hyn_info("error,binfile is crashed,check_sum=%x,chip_sum=%x\n", check_sum,
				   chip_sum);
			return -ESRCH;
		}
	} else {
		hyn_info("error,chip type error,chip_type=%d\n", chip_type);
		return -EINTR;
	}

	return 0;
}

#define UPDATE_FW_BY_WORK  1
#if (UPDATE_FW_BY_WORK == 1)
typedef struct _TP_MODULE_INFO_ {
	unsigned char module_id;
	unsigned char *pmodule_name;
	unsigned char lcd_id;
} TP_MODULE_INFO;

TP_MODULE_INFO cst8xx_tp_module_info[] = {
	{HYN_MODULE_ID, HYN_UPGRADE_FW_FILE, 0x00},
};

TP_MODULE_INFO  chip_module_info;
extern void cst8xx_irq_disable(struct cst8xx_ts_data *ts);
extern void cst8xx_irq_enable(struct cst8xx_ts_data *ts);
#define HYN_FILE_NAME_LENGTH					128
#define HYN_FW_NAME_PREX_WITH_REQUEST			   "hyn_cst836u_ts_fw_"
#define HYN_CST836U_BINFILE_SIZE				(15*1024+6)
static int read_fw_file(struct cst8xx_ts_data *ts)
{
	int ret = 0;
	const struct firmware *fw = NULL;
	u8 *tmpbuf = NULL;
	char fwname[HYN_FILE_NAME_LENGTH] = { 0 };

	snprintf(fwname, HYN_FILE_NAME_LENGTH, "%s%s.bin", HYN_FW_NAME_PREX_WITH_REQUEST,
		 chip_module_info.pmodule_name);
	hyn_info("cst8xx:%s\n", fwname);
	ret = request_firmware(&fw, fwname, ts->dev);
	if (ret == 0) {
		hyn_info("cst8xx:firmware(%s) request successfully", fwname);
		tmpbuf = vmalloc(fw->size);
		hyn_info("cst8xx:fw->size=%d request successfully", fw->size);

		if (tmpbuf == NULL) {
			hyn_info("cst8xx:fw buffer vmalloc fail");
			ret = -ENOMEM;
		} else {
			memcpy(tmpbuf, fw->data, fw->size);
			p_cst836u_upgrade_firmware = tmpbuf;
			hyn_info("fw->size=%d", fw->size);

			if (fw->size != HYN_CST836U_BINFILE_SIZE) {
				hyn_info("fw buffer vmalloc fail");
				ret = -EINVAL;
				vfree(p_cst836u_upgrade_firmware);
				p_cst836u_upgrade_firmware = NULL;
			}
		}
	} else {
		hyn_info("firmware(%s) request fail,ret=%d", fwname, ret);
	}

	if (fw != NULL) {
		release_firmware(fw);
		fw = NULL;
	}

	if (ret == 0) {
		ret = p_cst836u_upgrade_firmware[HYN_CST836U_BINFILE_SIZE-5]&0x0F;
		hyn_info("fw module ver ret=%d", ret);
		if (check_binfile_valid(p_cst836u_upgrade_firmware, HYN_CST836U_BINFILE_SIZE, 0) < 0) {
			ret = -EINVAL;
		}
	}

	return ret;
}

static int get_module_version(void)
{
	int i, ret, retry, module_size;
	unsigned char reg_buf[2];
	unsigned char buf[4];

	chip_module_info.pmodule_name = cst8xx_tp_module_info[0].pmodule_name;
	for (retry = 0; retry < 3; retry++) {
		reg_buf[0] = 0xA8;
		ret = hyn_i2c_Read(client_up, reg_buf, 1, buf, 2);

		if (ret < 0) {
			hyn_err("%s: i2c read error.\n", __func__);
		} else {
			break;
		}
	}

	if (ret < 0) {
		hyn_err("%s,i2c read error\n", __func__);
		return -EPERM;
	}

	chip_module_info.module_id =  buf[0]&0x0F;
	chip_module_info.lcd_id	= (buf[0]&0xF0)>>4;

	module_size = sizeof(cst8xx_tp_module_info)/sizeof(TP_MODULE_INFO);
	for (i = 0; i < module_size; i++) {
		if (cst8xx_tp_module_info[i].module_id == chip_module_info.module_id) {
			chip_module_info.pmodule_name = cst8xx_tp_module_info[i].pmodule_name;
			break;
		}
	}

	if (i >= module_size) {
		hyn_err("%s module_ver=%d\n", __func__, chip_module_info.module_id);
		chip_module_info.module_id	= cst8xx_tp_module_info[0].module_id;
		chip_module_info.pmodule_name = cst8xx_tp_module_info[0].pmodule_name;
		return -EPERM;
	}

	hyn_info("%s module_ver=%d\n", __func__, chip_module_info.module_id);

	return chip_module_info.module_id;
}

int hyn_get_module_id(void)
{
	return get_module_version();
}

extern COMMON_INTERFACE_INFO g_itfs_ver_info;
int cst8xx_get_all_version(void)
{
	int i, ret, retry, module_size;
	unsigned char reg_buf[2];
	unsigned char buf[8];

	g_itfs_ver_info.fw_ver	  = 0x10;
	g_itfs_ver_info.ic_checksum = 0x55;
	g_itfs_ver_info.lcd_id	  = 0x00;
	g_itfs_ver_info.module_id   = 0x00;
	for (retry = 0; retry < 5; retry++) {
		reg_buf[0] = 0xA6;
		ret = hyn_i2c_Read(client_up, reg_buf, 1, buf, 6);
		if (ret < 0) {
			hyn_err("%s: i2c read error.\n", __func__);
		} else {
			break;
		}
		msleep(20);
	}
	if (ret < 0) {
		hyn_err("%s i2c read error\n", __func__);
		return -EPERM;
	}

	g_itfs_ver_info.lcd_id	  = buf[2]>>4;
	g_itfs_ver_info.module_id   = buf[2]&0x0F;

	g_itfs_ver_info.fw_ver	  = buf[1];
	g_itfs_ver_info.fw_ver	<<= 8;
	g_itfs_ver_info.fw_ver	 += buf[0];

	g_itfs_ver_info.ic_checksum   = buf[5];
	g_itfs_ver_info.ic_checksum <<= 8;
	g_itfs_ver_info.ic_checksum  += buf[4];


	module_size = sizeof(cst8xx_tp_module_info)/sizeof(TP_MODULE_INFO);
	for (i = 0; i < module_size; i++) {
		if (cst8xx_tp_module_info[i].module_id == g_itfs_ver_info.module_id) {
			g_itfs_ver_info.pmodule_name = cst8xx_tp_module_info[i].pmodule_name;
			break;
		}
	}

	if (i >= module_size) {
		g_itfs_ver_info.pmodule_name = "unknown module";
		return -ENOENT;
	}

	return 0;
}

int hyn_i2c_wr_pkt(char addr, u8 *data, int len)
{
	int i = 0;

	if (len == 1) {
		goto WRITE_ONE_REG;
	} else {
		char *tmpbuf = NULL;

		tmpbuf = kzalloc(len + 1, GFP_KERNEL);
		if (!tmpbuf) {
			hyn_err("allocate memory failed!\n");
			return -ENOMEM;
		}
		tmpbuf[0] = addr & 0xFF;
		for (i = 1; i <= len; i++) {
			tmpbuf[i] = data[i - 1];
		}
		if (hyn_i2c_Write(client_up, tmpbuf, len + 1) < 0) {
			hyn_err("%s i2c access fail1!\n", __func__);
			kfree(tmpbuf);
			return -EIO;
		}
		kfree(tmpbuf);
		return 0;
	}
WRITE_ONE_REG:
	if (hyn_write_reg(client_up, addr, *data) < 0) {
			hyn_err("%s i2c access fail2!\n", __func__);
			return -EIO;
	}
	return 0;
}


int hyn_i2c_rd_pkt(char addr, u8 *data, int len)
{
	return hyn_i2c_Read(client_up, &addr, 1, data, len);
}

int cst8xx_adb_update(u8 *dir, int name_len)
{
	return hyn_updatefw_by_sysfs(dir, name_len);
}


static void cst8xx_update_work(struct work_struct *work)
{
	int module_ver, file_module_ver;
	int i, ret, module_size;

	module_size = sizeof(cst8xx_tp_module_info) / sizeof(TP_MODULE_INFO);

	if (module_size > 10)
		module_size = 10;

	g_cst8xx_ts->fw_update_flag = 1;

	p_cst836u_upgrade_firmware = NULL;

	i = 0;
	do {
		module_ver = get_module_version();

		file_module_ver = read_fw_file(g_cst8xx_ts);

		hyn_info("%s module_ver=%d,file_module_ver=%d\n", __func__, module_ver,
			   file_module_ver);

		if ((module_ver == file_module_ver || module_ver < 0) && (p_cst836u_upgrade_firmware != NULL)) {
			cst8xx_irq_disable(g_cst8xx_ts);

			ret = ctp_hynitron_update(g_cst8xx_ts->client);

			module_ver = get_module_version();

			cst8xx_irq_enable(g_cst8xx_ts);

			hyn_info("%s module_ver=%d,ret=%d\n", __func__, module_ver, ret);

			if (ret == 0 && module_ver == file_module_ver) {
				hyn_info(" %s module_ver=%d,file_module_ver=%d\n",
					   __func__, module_ver, file_module_ver);
				break;
			}
			hyn_info("fail %s module_ver=%d,file_module_ver=%d\n",
				__func__, module_ver, file_module_ver);
		} else {
			if (module_ver != file_module_ver) {
			} else {
				hyn_info("module ver read fail,module_ver=%d\n", module_ver);
			}
		}

		if (p_cst836u_upgrade_firmware != NULL) {
			hyn_info("vfree p_cst836u_upgrade_firmware\n");
			vfree(p_cst836u_upgrade_firmware);
			p_cst836u_upgrade_firmware = NULL;
		}
	} while (++i < module_size);

	g_cst8xx_ts->fw_update_flag = 0;
}


int cst8xx_update_by_work(struct cst8xx_ts_data *ts)
{
	apk_upgrade_flag   = 0;
	ts->fw_update_flag = 0;
	INIT_WORK(&ts->fwupdate_work, cst8xx_update_work);
	ts->fwupdate_workqueue = create_singlethread_workqueue("hyn_update_work_queue");
	queue_work(ts->fwupdate_workqueue, &ts->fwupdate_work);
	return 0;
}
#endif

#endif  /* CTP_HYNITRON_EXT==1 */

#if ANDROID_TOOL_SURPORT   /* debug tool support */

#define CST8XX_PROC_DIR_NAME	"cst8xx_ts"
#define CST8XX_PROC_FILE_NAME	"cst8xx-update"

static struct proc_dir_entry *g_proc_dir;
static struct proc_dir_entry *g_update_file;
static int CMDIndex = 0;

static struct file *cst8xx_open_fw_file(char *path, mm_segment_t *old_fs_p)
{
	struct file *filp;
	int ret;

	*old_fs_p = get_fs();
	/* set_fs(KERNEL_DS); */
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		hyn_info("%s filp_open error.\n", __func__);
		return NULL;
	}
	filp->f_op->llseek(filp, 0, 0);

	return filp;
}

static void cst2xx_close_fw_file(struct file *filp, mm_segment_t old_fs)
{
	if (filp)
		filp_close(filp, NULL);
}

static int cst2xx_read_fw_file(unsigned char *filename, unsigned char *pdata, int *plen)
{
	struct file *fp;
	mm_segment_t old_fs;
	int ret = -1;
	loff_t pos;
	off_t fsize;
	struct inode *inode;
	unsigned long magic;

	hyn_info("%s enter.\n", __func__);

	if ((pdata == NULL) || (strlen(filename) == 0))
		return ret;

	fp = cst8xx_open_fw_file(filename, &old_fs);
	if (fp == NULL) {
		hyn_err("Open bin file failed.path:%s.\n", filename);
		goto clean;
	}

	if (IS_ERR(fp)) {
		hyn_err("error occurred while opening file %s.\n", filename);
		return -EIO;
	}
	inode = fp->f_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	ret = vfs_read(fp, pdata, fsize, &pos);
	if (ret == fsize) {
		hyn_info("vfs_read success.ret:%d.\n", ret);
	} else {
		hyn_err("vfs_read fail.ret:%d.\n", ret);
	}
	filp_close(fp, NULL);
	set_fs(old_fs);

	hyn_info("vfs_read done.\n");

clean:
	cst2xx_close_fw_file(fp, old_fs);
	return ret;
}

static int cst8xx_apk_fw_dowmload(struct i2c_client *client, unsigned char *pdata, int length)
{
	int ret;

	hyn_info("%s enter.\n", __func__);

	apk_upgrade_flag = 1;
	p_cst836u_upgrade_firmware = (unsigned char *)pdata;

	ret = ctp_hynitron_update(client);
	p_cst836u_upgrade_firmware = NULL;
	apk_upgrade_flag = 0;
	if (ret < 0) {
		hyn_info("online update fw failed.\n");
		return -EPERM;
	}

	return 0;
}

static ssize_t cst8xx_proc_read_foobar(struct file *page, char __user *user_buf, size_t count, loff_t *data)
{
	unsigned char buf[150];
	int len = 0;
	int ret;
	struct i2c_client *client = (struct i2c_client *)PDE_DATA(file_inode(page));

	hyn_info("%s CMDIndex:%d.\n", __func__, CMDIndex);

	if (CMDIndex == 0) {
		snprintf(buf, sizeof(buf), "Hynitron touchscreen driver 1.0.\n");
		/* strcpy(page,buf); */
		len = strlen(buf);
		ret = copy_to_user(user_buf, buf, len);
	} else if (CMDIndex == 1) {
		buf[0] = 0xA6;
		ret = hyn_i2c_Read(client, (u8 *) buf, 1, (u8 *) buf, 8);
		if (ret < 0) {
			hyn_info("%s hyn_i2c_Read fail.\n", __func__);
		} else {
			ret = copy_to_user(user_buf, buf, 8);
			len = 8;
		}
	}
	if (CMDIndex == 2 || CMDIndex == 3 || CMDIndex == 4) {

		int data_len = 80;
		int report_count = 0;

		if (CMDIndex == 2) {	/* read diff */
			buf[0] = 0x00;
			buf[1] = 0x07;
		} else if (CMDIndex == 3) {	/* rawdata */
			buf[0] = 0x00;
			buf[1] = 0x06;
		} else if (CMDIndex == 4) {	/* factory */
			buf[0] = 0x00;
			buf[1] = 0x04;
			mdelay(1000);
		}
		ret = hyn_i2c_Write(client, buf, 2);
		if (ret < 0) {
			hyn_info("Write command raw/diff mode failed.error:%d.\n", ret);
			goto END;
		}

		mdelay(10);

		for (report_count = 0; report_count < 16; report_count++) {
			unsigned char temp_buf[7];

			ret = i2c_master_recv(client, temp_buf, 6);
			if (ret < 0) {
				hyn_info("Read raw/diff data failed.error:%d.\n", ret);
				goto END;
			}
			memcpy((unsigned char *)buf + 2 + report_count * 5, (unsigned char *)temp_buf + 1, 5);
		}

		buf[0] = 0x00;
		buf[1] = 0x00;
		ret = hyn_i2c_Write(client, buf, 2);
		if (ret < 0) {
			hyn_info("Write command raw/diff mode failed.error:%d.\n", ret);
			goto END;
		}

		buf[0] = 4;
		buf[1] = 10;
		ret = copy_to_user(user_buf, buf, data_len + 2);
		len = data_len + 2;

		if (CMDIndex == 4) {
			cst8xx_ts_reset();
			msleep(50);
		}
	}

END:
	g_unnormal_mode = 0;
	CMDIndex = 0;
	return len;
}

static ssize_t cst8xx_proc_write_foobar(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	unsigned char cmd[258];
	unsigned char buf[4];
	unsigned char *pdata = NULL;
	int len;
	int ret;
	int length = 16*1024;
	struct i2c_client *client = (struct i2c_client *)PDE_DATA(file_inode(file));

	if (count > 256)
		len = 256;
	else
		len = count;

	if (copy_from_user(cmd, buffer, len)) {
		hyn_err("copy data from user space failed.\n");
		return -EFAULT;
	}

	hyn_info("%s cmd:%d %d len:%d .\n", __func__, cmd[0], cmd[1], len);

	if (client == NULL) {
		client = client_up;
		hyn_info("client is null.\n");
	}

	if (cmd[0] == 0) {
		pdata = kzalloc((sizeof(char) * length), GFP_KERNEL);
		if (pdata == NULL) {
			hyn_err("zalloc GFP_KERNEL memory fail.\n");
			return -ENOMEM;
		}

		ret = cst2xx_read_fw_file(&cmd[1], pdata, &length);
		if (ret < 0) {
			hyn_err("cst2xx_read_fw_file fail.\n");
			if (pdata != NULL) {
				kfree(pdata);
				pdata = NULL;
			}
			return -EPERM;
		}
		ret = cst8xx_apk_fw_dowmload(client, pdata, length);
		if (ret < 0) {
			hyn_err("update firmware failed.\n");
			if (pdata != NULL) {
				kfree(pdata);
				pdata = NULL;
			}
			return -EPERM;
		}

		if (pdata != NULL) {
			kfree(pdata);
			pdata = NULL;
		}
	} else if (cmd[0] == 2) {
		/* cst2xx_touch_release(); */
		CMDIndex = cmd[1];
	} else if (cmd[0] == 3) {
		CMDIndex = 0;

		buf[0] = 0x00;
		buf[1] = 0x00;
		ret = hyn_i2c_Write(client, buf, 2);
		if (ret < 0) {
			hyn_err("Write command raw/diff mode failed.error:%d.\n", ret);
		}
	} else {

		hyn_info("cmd[0] error:%d.\n", cmd[0]);
	}

	return count;
}


static const struct file_operations proc_tool_debug_fops = {

	.owner		= THIS_MODULE,
	.read		= cst8xx_proc_read_foobar,
	.write		= cst8xx_proc_write_foobar,

};
int cst8xx_proc_fs_init(void)
{
	int ret;

	g_proc_dir = proc_mkdir(CST8XX_PROC_DIR_NAME, NULL);
	if (g_proc_dir == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	g_update_file =
		proc_create_data(CST8XX_PROC_FILE_NAME, 0777 | S_IFREG, g_proc_dir, &proc_tool_debug_fops,
				 (void *)client_up);
	if (g_update_file == NULL) {
		ret = -ENOMEM;
		hyn_err("proc_create_data CST8XX_PROC_FILE_NAME failed.\n");
		goto no_foo;
	}

	return 0;

no_foo:
	remove_proc_entry(CST8XX_PROC_FILE_NAME, g_proc_dir);
out:
	return ret;
}

#if SYSFS_DEBUG
static ssize_t hyn_tpfwver_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t num_read_chars = 0;
	u8 buf1[10] = { 0 };
	unsigned int chip_version, module_version, project_version, chip_type, checksum;

	memset((u8 *)buf1, 0, 10);
	mutex_lock(&g_device_mutex);

	chip_version = 0;
	module_version = 0;
	project_version = 0;
	chip_type = 0;
	checksum = 0;

	buf1[0] = 0xA6;
	if (hyn_i2c_Read(client_up, (u8 *)buf1, 1, (u8 *)buf1, 8) < 0)
		num_read_chars = snprintf(buf, 128, "get tp fw version fail!\n");
	else{
		chip_version  = buf1[0];
		chip_version |= buf1[1]<<8;

		module_version = buf1[2];
		project_version = buf1[3];

		chip_type  = buf1[4];
		chip_type |= buf1[5]<<8;

		checksum = buf1[6];
		checksum |= buf1[7] << 8;

		num_read_chars =
			snprintf(buf, 128,
				 "chip_version: 0x%02X,module_version:0x%02X,project_version:0x%02X,chip_type:0x%02X,checksum:0x%02X .\n",
				 chip_version, module_version, project_version, chip_type, checksum);
	}

	mutex_unlock(&g_device_mutex);

	return num_read_chars;
}

static ssize_t hyn_tpfwver_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/*place holder for future use*/
	return -EPERM;
}


static ssize_t hyn_tprwreg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*place holder for future use*/
	return -EPERM;
}

static ssize_t hyn_tprwreg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t num_read_chars = 0;
	int retval;
	unsigned long int wmreg = 0;
	u8 regaddr = 0xff, regvalue = 0xff;
	u8 valbuf[5] = {0};

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(&g_device_mutex);
	num_read_chars = count - 1;

	if (num_read_chars != 2) {
		if (num_read_chars != 4) {
			hyn_err("please input 2 or 4 character\n");
			goto error_return;
		}
	}

	memcpy(valbuf, buf, num_read_chars);
	retval = kstrtoul(valbuf, 16, &wmreg);

	if (retval != 0) {
		hyn_err("%s() - ERROR: The given input was: \"%s\"\n", __func__, buf);
		goto error_return;
	}

	if (num_read_chars == 2) {
		/*read register*/
		regaddr = wmreg;
		if (hyn_read_reg(client_up, regaddr, &regvalue) < 0)
			hyn_err("Could not read the register(0x%02x).\n", regaddr);
		else
			hyn_info("the register(0x%02x) is 0x%02x\n", regaddr, regvalue);
	} else {
		regaddr = wmreg >> 8;
		regvalue = wmreg;
		if (hyn_write_reg(client_up, regaddr, regvalue) < 0)
			hyn_err("Could not write the register(0x%02x)\n", regaddr);
		else
			hyn_info("Write 0x%02x into register(0x%02x) successful\n", regvalue, regaddr);
	}

error_return:
	mutex_unlock(&g_device_mutex);

	return count;
}

static ssize_t hyn_fwupdate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* place holder for future use */
	return -EPERM;
}

/*upgrade from *.i*/
static ssize_t hyn_fwupdate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	hyn_info("%s enter.\n", __func__);

	mutex_lock(&g_device_mutex);
	disable_irq(client_up->irq);
	p_cst836u_upgrade_firmware = NULL;
	if (p_cst836u_upgrade_firmware != NULL) {
		apk_upgrade_flag = 1;
		ret = ctp_hynitron_update(client_up);
		if (ret < 0) {
			hyn_err("%s ctp_hynitron_update fail.\n", __func__);
		}
	}
	enable_irq(client_up->irq);
	mutex_unlock(&g_device_mutex);

	return count;
}

static ssize_t hyn_fwupgradeapp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*place holder for future use*/
	return -EPERM;
}

/*upgrade from app.bin*/
static ssize_t hyn_fwupgradeapp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char fwname[256];
	int ret;
	unsigned char *pdata = NULL;
	int length = 16*1024;

	hyn_info("%s enter.\n", __func__);

	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, sizeof(fwname), "/sdcard/%s", buf);
	fwname[count - 1 + 8] = '\0';

	hyn_info("fwname:%s.\n", fwname);
	pdata = kzalloc((sizeof(char) * length), GFP_KERNEL);
	if (pdata == NULL) {
		hyn_err("%s GFP_KERNEL memory fail.\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&g_device_mutex);
	disable_irq(client_up->irq);

	ret = cst2xx_read_fw_file(fwname, pdata, &length);
	if (ret < 0) {
		hyn_err("cst8xx_read_fw_file fail.\n");
	} else {
		ret = cst8xx_apk_fw_dowmload(client_up, pdata, length);
		if (ret < 0) {
			hyn_err("cst8xx_apk_fw_dowmload failed.\n");
		}
	}

	if (pdata != NULL) {
		kfree(pdata);
		pdata = NULL;
	}

	enable_irq(client_up->irq);
	mutex_unlock(&g_device_mutex);

	hyn_info("%s exit.\n", __func__);

	return count;
}
static int cst8xx_read_fix_fw_file(unsigned char *filename, int name_len, unsigned char *pdata, int *plen)
{
	struct file *fp;
	mm_segment_t old_fs;
	int ret = -1;
	loff_t pos;
	off_t fsize;
	struct inode *inode;
	unsigned long magic;
	char PathName[200] = { 0 };

	hyn_info("%s enter.\n", __func__);

	if ((pdata == NULL) || (strlen(filename) == 0) || name_len > 180) {
		hyn_err("param,pdata=%d,filename=%s,name_len=%d\n", (int)pdata, filename, name_len);
		return ret;
	}

	memset(PathName, 0, sizeof(PathName));
	snprintf(PathName, sizeof(PathName), "/sdcard/%s", filename);
	PathName[name_len - 1 + 8] = '\0';
	hyn_info("sdcard PathName(%s)\n", PathName);

	fp = cst8xx_open_fw_file(PathName, &old_fs);
	if (fp == NULL) {
		hyn_info("Open sdcard bin file failed.path:(%s)\n", PathName);

		memset(PathName, 0, sizeof(PathName));
		snprintf(PathName, sizeof(PathName), "/vendor/firmware/%s", filename);
		PathName[name_len - 1 + 17] = '\0';

		fp = cst8xx_open_fw_file(PathName, &old_fs);

		if (fp == NULL) {
			hyn_err("Open vendor firmware bin file failed.path:(%s)\n", PathName);
			goto clean;
		} else {
			hyn_info("Open sdcard bin file success.path:(%s)\n", PathName);
		}
	} else {
		hyn_info("Open sdcard bin file success.path:(%s)\n", PathName);
	}

	if (IS_ERR(fp)) {
		hyn_info("cst8xx:error occurred while opening file %s.\n", PathName);
		return -EIO;
	}
	inode = fp->f_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	ret = vfs_read(fp, pdata, fsize, &pos);
	if (ret == fsize) {
		hyn_info("vfs_read success.ret:%d.\n", ret);
	} else {
		hyn_info("vfs_read fail.ret:%d.\n", ret);
	}
	filp_close(fp, NULL);
	set_fs(old_fs);

	hyn_info("vfs_read done.\n");

clean:
	cst2xx_close_fw_file(fp, old_fs);
	return ret;
}

static int hyn_updatefw_by_sysfs(u8 *dir, int name_len)
{
	int ret = -1;
	unsigned char *pdata = NULL;
	int length = 16*1024;

	hyn_info("hyn_updatefw_run dir:(%s) enter.\n", dir);

	g_cst8xx_ts->fw_update_flag = 1;

	pdata = kzalloc((sizeof(char) * length), GFP_KERNEL);
	if (pdata == NULL) {
		hyn_err("%s GFP_KERNEL memory fail.\n",  __func__);
		g_cst8xx_ts->fw_update_flag = 0;

		return -ENOMEM;
	}

	mutex_lock(&g_device_mutex);
	disable_irq(client_up->irq);

	ret = cst8xx_read_fix_fw_file(dir, name_len, pdata, &length);
	if (ret < 0) {
		hyn_err("cst8xx_read_fw_file fail.\n");
		if (pdata != NULL) {
			kfree(pdata);
			pdata = NULL;
		}
		g_cst8xx_ts->fw_update_flag = 0;
		enable_irq(client_up->irq);
		mutex_unlock(&g_device_mutex);
		return -EPERM;
	}

	if (check_binfile_valid(pdata, HYN_CST836U_BINFILE_SIZE, 0) < 0) {
		hyn_err("check_binfile_valid unvalid binfile\n");
		if (pdata != NULL) {
			kfree(pdata);
			pdata = NULL;
		}
		g_cst8xx_ts->fw_update_flag = 0;
		enable_irq(client_up->irq);
		mutex_unlock(&g_device_mutex);
		return -ENOENT;
	}
	ret = cst8xx_apk_fw_dowmload(client_up, pdata, length);
	if (ret < 0) {
		hyn_err("cst8xx_apk_fw_dowmload failed.ret=%d.\n", ret);
		if (pdata != NULL) {
			kfree(pdata);
			pdata = NULL;
		}
		g_cst8xx_ts->fw_update_flag = 0;
		enable_irq(client_up->irq);
		mutex_unlock(&g_device_mutex);
		return -ESRCH;
	}

	if (pdata != NULL) {
		kfree(pdata);
		pdata = NULL;
	}
	enable_irq(client_up->irq);
	mutex_unlock(&g_device_mutex);

	hyn_info("hyn_updatefw_run exit.\n");
	g_cst8xx_ts->fw_update_flag = 0;
	return 0;
}

/*sysfs */
/*get the fw version
*example:cat hyntpfwver
*/
static DEVICE_ATTR(hyntpfwver, S_IRUGO | S_IWUSR, hyn_tpfwver_show, hyn_tpfwver_store);

/*upgrade from *.i
*example: echo 1 > hynfwupdate
*/
static DEVICE_ATTR(hynfwupdate, S_IRUGO | S_IWUSR, hyn_fwupdate_show, hyn_fwupdate_store);

/*read and write register
*read example: echo 88 > hyntprwreg ---read register 0x88
*write example:echo 8807 > hyntprwreg ---write 0x07 into register 0x88
*
*note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(hyntprwreg, S_IRUGO | S_IWUSR, hyn_tprwreg_show, hyn_tprwreg_store);

/*upgrade from app.bin
*example:echo "*_app.bin" > hynfwupgradeapp
*/
static DEVICE_ATTR(hynfwupgradeapp, S_IRUGO | S_IWUSR, hyn_fwupgradeapp_show, hyn_fwupgradeapp_store);

/*add your attr in here*/
static struct attribute *hyn_attributes[] = {
	&dev_attr_hyntpfwver.attr,
	&dev_attr_hynfwupdate.attr,
	&dev_attr_hyntprwreg.attr,
	&dev_attr_hynfwupgradeapp.attr,
	NULL
};

static struct attribute_group hyn_attribute_group = {
	.attrs = hyn_attributes
};
/*create sysfs for debug*/

int hyn_create_sysfs(struct i2c_client *client)
{
	int err = -1;

	client_up = client;
	k_obj = kobject_create_and_add("hynitron_debug", NULL);
	if (k_obj == NULL) {
		hyn_err("hynitron_debug sys node create error.\n");
	}
	err = sysfs_create_group(k_obj, &hyn_attribute_group);
	if (err) {
		hyn_err("%s() - ERROR: sysfs_create_group() failed.\n", __func__);
		sysfs_remove_group(k_obj, &hyn_attribute_group);
		return -EIO;
	}

	mutex_init(&g_device_mutex);
	hyn_info("%s() - sysfs_create_group() succeeded.\n", __func__);

	return err;
}

void hyn_release_sysfs(struct i2c_client *client)
{
	sysfs_remove_group(k_obj, &hyn_attribute_group);
	mutex_destroy(&g_device_mutex);
}
#endif


#endif

