/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83113 chipset
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_ic_HX83113.h"
#include "himax_modular.h"

static void hx83113_chip_init(void)
{
	(*kp_private_ts)->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n", __func__,
		(*kp_private_ts)->chip_cell_type);
	*kp_IC_CHECKSUM = HX_TP_BIN_CHECKSUM_CRC;
	/*Himax: Set FW and CFG Flash Address*/
	*kp_FW_VER_MAJ_FLASH_ADDR = 49157;  /*0x00C005*/
	*kp_FW_VER_MIN_FLASH_ADDR = 49158;  /*0x00C006*/
	*kp_CFG_VER_MAJ_FLASH_ADDR = 49408;  /*0x00C100*/
	*kp_CFG_VER_MIN_FLASH_ADDR = 49409;  /*0x00C101*/
	*kp_CID_VER_MAJ_FLASH_ADDR = 49154;  /*0x00C002*/
	*kp_CID_VER_MIN_FLASH_ADDR = 49155;  /*0x00C003*/
	*kp_CFG_TABLE_FLASH_ADDR = 0x10000;
}

static bool hx83113_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	do {
		if (cnt == 0 || (tmp_data[0] != 0xA5
		&& tmp_data[0] != 0x00 && tmp_data[0] != 0x87))
			kp_g_core_fp->fp_register_write(
				(*kp_pfw_op)->addr_ctrl_fw_isr,
				DATA_LEN_4,
				(*kp_pfw_op)->data_fw_stop,
				0);

		usleep_range(10000, 11000);
		/* check fw status */
		kp_g_core_fp->fp_register_read(
			(*kp_pic_op)->addr_cs_central_state,
			ADDR_LEN_4,
			tmp_data,
			0);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
				__func__, tmp_data[0]);
			break;
		}

		kp_g_core_fp->fp_register_read((*kp_pfw_op)->addr_ctrl_fw_isr,
			4, tmp_data, false);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
			cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 10) && check_en == true);

	cnt = 0;

	do {
		/*===========================================
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 *===========================================
		 */
		tmp_data[0] = (*kp_pic_op)->data_i2c_psw_lb[0];

		ret = kp_himax_bus_write((*kp_pic_op)->adr_i2c_psw_lb[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 *===========================================
		 */
		tmp_data[0] = (*kp_pic_op)->data_i2c_psw_ub[0];

		ret = kp_himax_bus_write((*kp_pic_op)->adr_i2c_psw_ub[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x00
		 *===========================================
		 */
		tmp_data[0] = 0x00;

		ret = kp_himax_bus_write((*kp_pic_op)->adr_i2c_psw_lb[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 *===========================================
		 */
		tmp_data[0] = (*kp_pic_op)->data_i2c_psw_lb[0];

		ret = kp_himax_bus_write((*kp_pic_op)->adr_i2c_psw_lb[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 *===========================================
		 */
		tmp_data[0] = (*kp_pic_op)->data_i2c_psw_ub[0];

		ret = kp_himax_bus_write((*kp_pic_op)->adr_i2c_psw_ub[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/* ======================
		 *Check enter_save_mode
		 *======================
		 */
		kp_g_core_fp->fp_register_read(
			(*kp_pic_op)->addr_cs_central_state,
			ADDR_LEN_4,
			tmp_data,
			0);
		I("%s: Check enter_save_mode data[0]=%X\n",
			__func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/*=====================================
			 *Reset TCON
			 *=====================================
			 */
			kp_g_core_fp->fp_register_write(
				(*kp_pic_op)->addr_tcon_on_rst,
				DATA_LEN_4,
				(*kp_pic_op)->data_rst,
				0);
			usleep_range(1000, 1100);
			/*=====================================
			 *Reset ADC
			 *=====================================
			 */
			kp_g_core_fp->fp_register_write(
				(*kp_pic_op)->addr_adc_on_rst,
				DATA_LEN_4,
				(*kp_pic_op)->data_rst,
				0);

			usleep_range(1000, 1100);
			tmp_data[3] = (*kp_pic_op)->data_rst[3];
			tmp_data[2] = (*kp_pic_op)->data_rst[2];
			tmp_data[1] = (*kp_pic_op)->data_rst[1];
			tmp_data[0] = (*kp_pic_op)->data_rst[0] | 0x01;
			kp_g_core_fp->fp_register_write(
				(*kp_pic_op)->addr_adc_on_rst,
				DATA_LEN_4,
				tmp_data,
				0);

			goto SUCCED;
		} else {
			/*msleep(10);*/
#if defined(HX_RST_PIN_FUNC)
			kp_g_core_fp->fp_ic_reset(false, false);
#else
			kp_g_core_fp->fp_system_reset();
#endif
		}
	} while (cnt++ < 5);

	return false;
SUCCED:
	return true;
}

#if defined(HX_ZERO_FLASH)
static void himax_hx83113_reload_to_active(void)
{
	uint8_t addr[DATA_LEN_4] = {0};
	uint8_t data[DATA_LEN_4] = {0};
	uint8_t retry_cnt = 0;

	kp_himax_parse_assign_cmd(zf_addr_activ_relod, addr, sizeof(addr));
	do {
		kp_himax_parse_assign_cmd(zf_data_activ_in, data,
			sizeof(data));
		kp_g_core_fp->fp_register_write(addr, DATA_LEN_4, data, 0);
		usleep_range(1000, 1001);
		kp_g_core_fp->fp_register_read(addr, DATA_LEN_4, data, 0);
		I("%s: data[1]=%d, data[0]=%d, retry_cnt=%d\n", __func__,
			data[1], data[0], retry_cnt);
		retry_cnt++;
	} while ((data[1] != 0x01
		|| data[0] != 0xEC)
		&& retry_cnt < HIMAX_I2C_RETRY_TIMES);
}

static void himax_hx83113_resume_ic_action(void)
{
#if !defined(HX_RESUME_HW_RESET)
	kp_g_core_fp->fp_0f_reload_to_active();
#endif
}

static void himax_hx83113_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};
	int retry = 0;
#if defined(HX_RST_PIN_FUNC)
	int ret = 0;
#endif

	I("Enter %s\n", __func__);
	(*kp_private_ts)->notouch_frame = (*kp_private_ts)->ic_notouch_frame;
	kp_g_core_fp->fp_interface_on();

	if (!FlashMode) {
#if defined(HX_RST_PIN_FUNC)
		/*kp_g_core_fp->fp_ic_reset(false, false);*/
		/**
		 * password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = (*kp_pic_op)->data_i2c_psw_lb[0];

		ret = kp_himax_bus_write((*kp_pic_op)->adr_i2c_psw_lb[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: xfer fail!\n", __func__);

		/**
		 * password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = (*kp_pic_op)->data_i2c_psw_ub[0];

		ret = kp_himax_bus_write((*kp_pic_op)->adr_i2c_psw_ub[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: xfer fail!\n", __func__);

		/**
		 * password[7:0] set Enter safe mode : 0x31 ==> 0x00
		 */
		tmp_data[0] = 0x00;

		if (kp_himax_bus_write(
		(*kp_pic_op)->adr_i2c_psw_lb[0],
		tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0)
			E("%s: xfer fail!\n", __func__);
#else
		kp_g_core_fp->fp_system_reset();
#endif
	} else {
		do {
			kp_g_core_fp->fp_register_read(
				(*kp_pfw_op)->addr_flag_reset_event,
				DATA_LEN_4,
				tmp_data,
				0);
			I("%s:Read status from IC = %X,%X\n", __func__,
				tmp_data[0], tmp_data[1]);
		} while ((tmp_data[1] != 0x01
			|| tmp_data[0] != 0x00)
			&& retry++ < 5);

		if (retry >= 5) {
			E("%s: Fail:\n", __func__);
#if defined(HX_RST_PIN_FUNC)
			kp_g_core_fp->fp_ic_reset(false, false);
#else
			kp_g_core_fp->fp_system_reset();
#endif
		} else {
			I("%s:OK and Read status from IC = %X,%X\n",
					__func__, tmp_data[0], tmp_data[1]);
			/* reset code*/
			tmp_data[0] = 0x00;

			if (kp_himax_bus_write((*kp_pic_op)->adr_i2c_psw_lb[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0)
				E("%s: xfer fail!\n", __func__);

			if (kp_himax_bus_write((*kp_pic_op)->adr_i2c_psw_ub[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0)
				E("%s: xfer fail!\n", __func__);
		}
	}
	kp_g_core_fp->fp_0f_reload_to_active();
}
#endif

static void hx83113_func_re_init(void)
{
	kp_g_core_fp->fp_sense_off = hx83113_sense_off;
	kp_g_core_fp->fp_chip_init = hx83113_chip_init;
#if defined(HX_ZERO_FLASH)
	kp_g_core_fp->fp_resume_ic_action = himax_hx83113_resume_ic_action;
	kp_g_core_fp->fp_sense_on = himax_hx83113_sense_on;
	kp_g_core_fp->fp_0f_reload_to_active = himax_hx83113_reload_to_active;
#endif
}

static void hx83113_reg_re_init(void)
{
	(*kp_private_ts)->ic_notouch_frame = hx83113_notouch_frame;
}

static bool hx83113_chip_detect(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	bool ret_data = false;
	int ret = 0;
	int i = 0;

	if (himax_ic_setup_external_symbols())
		return false;

	ret = kp_himax_mcu_in_cmd_struct_init();
	if (ret < 0) {
		ret_data = false;
		E("%s:cmd_struct_init Fail:\n", __func__);
		return ret_data;
	}

	kp_himax_mcu_in_cmd_init();

	hx83113_reg_re_init();
	hx83113_func_re_init();
	if (kp_himax_bus_read((*kp_pic_op)->addr_conti[0],
	tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return false;
	}

	if (kp_g_core_fp->fp_sense_off(false) == false) {
		ret_data = false;
		E("%s:fp_sense_off Fail:\n", __func__);
		return ret_data;
	}
	for (i = 0; i < 5; i++) {
		if (kp_g_core_fp->fp_register_read(
		(*kp_pfw_op)->addr_icid_addr,
		DATA_LEN_4, tmp_data, false) != 0) {
			ret_data = false;
			E("%s:fp_register_read Fail:\n", __func__);
			return ret_data;
		}
		I("%s:Read driver IC ID = %X, %X, %X\n", __func__,
			tmp_data[3], tmp_data[2], tmp_data[1]);

		if ((tmp_data[3] == 0x83)
		&& (tmp_data[2] == 0x11)
		&& (tmp_data[1] == 0x3a)) {
			strlcpy((*kp_private_ts)->chip_name,
				HX_83113A_SERIES_PWON,  30);

			I("%s:IC name = %s\n", __func__,
				(*kp_private_ts)->chip_name);

			I("Himax IC package %x%x%x in\n", tmp_data[3],
				tmp_data[2], tmp_data[1]);
			(*kp_ic_data)->ic_adc_num = hx83113_data_adc_num;
			ret_data = true;
			goto FINAL;
		} else {
			ret_data = false;
			E("%s:Read driver ID register Fail:\n", __func__);
			E("Could NOT find Himax Chipset\n");
			E("Please check 1.VCCD,VCCA,VSP,VSN\n");
			E("2. LCM_RST,TP_RST\n");
			E("3. Power On Sequence\n");
		}
	}

FINAL:
	return ret_data;
}

DECLARE(HX_MOD_KSYM_HX83113);

static int himax_hx83113_probe(void)
{
	I("%s:Enter\n", __func__);

	himax_add_chip_dt(hx83113_chip_detect);

	return 0;
}

static int himax_hx83113_remove(void)
{
	free_chip_dt_table();
	return 0;
}

static int __init himax_hx83113_init(void)
{
	int ret = 0;

	I("%s\n", __func__);
	ret = himax_hx83113_probe();
	return 0;
}

static void __exit himax_hx83113_exit(void)
{
	himax_hx83113_remove();
}

module_init(himax_hx83113_init);
module_exit(himax_hx83113_exit);

MODULE_DESCRIPTION("HIMAX HX83113 touch driver");
MODULE_LICENSE("GPL");


