#include "zte_lcd_common.h"
#include "sde_trace.h"
struct dsi_panel *g_zte_ctrl_pdata;
struct device *dsi_uevent_device = NULL;
#ifdef CONFIG_ZTE_LCD_REG_DEBUG
extern void zte_lcd_reg_debug_func(void);
#endif

/*add by zte for read lcd cmds start*/
int zte_dsi_panel_cmd_read(struct dsi_panel *panel, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}
	dsi = &g_zte_ctrl_pdata->mipi_device;
	if (!dsi) {
		pr_info("MSM_LCD No dsi device\n");
		return -EINVAL;
	}

	mipi_dsi_dcs_read(dsi, cmd, data, len);

	return 0;
}
/*add by zte for read lcd cmds end*/

int zte_node_write_panel(u32 index, u32 mode)
{
	int err = 0;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
	if (index == ZTE_LCD_HBM_CTRL && mode > 1) {
		pr_info("MSM_LCD HBM old_premode=%d, new_premode=%d\n",
				g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_pre_hbm_mode, mode);
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_pre_hbm_mode = mode;
		return 0;
	}
#endif

	mutex_lock(&g_zte_ctrl_pdata->panel_lock);
	if (!g_zte_ctrl_pdata->panel_initialized) {
		err = -EPERM;
		pr_err("MSM_LCD panel is off or not initialized index=%d mode=%d\n", index, mode);
		goto error;
	}
	pr_info("MSM_LCD Send cmds index=%d mode=%d to panel start\n", index, mode);
	switch (index) {
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
	case ZTE_LCD_HBM_CTRL:
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_bl_reg53_control) {
			zte_local_hbm_ctrl_display(mode);
		} else {
			zte_hbm_ctrl_display(mode);
		}
		break;
	case ZTE_LCD_HDR_CTRL:
		zte_hdr_ctrl_display(mode);
		break;
	case ZTE_LCD_FGP_CTRL:
		zte_fgp_ctrl_display(mode);
		break;
#endif
#ifdef CONFIG_ZTE_LCD_COLOR_GAMUT_CTRL
	case ZTE_LCD_COLOR_GAMUT_CTRL:
		zte_color_gamut_ctrl_display(mode);
		break;
#endif
#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
	case ZTE_LCD_AOD_BRIGHTNESS_CTRL:
		zte_aod_brightness_ctrl_display(mode);
		break;
#endif
#ifdef CONFIG_ZTE_LCD_ACL_CTRL
	case ZTE_LCD_ACL_CTRL:
		zte_acl_ctrl_display(mode);
		break;
#endif
#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
	case ZTE_LCD_2D_3D_CTRL:
		zte_bl_2d_3d_ctrl_display(mode);
		break;
	case ZTE_LCD_DISP_GAMMA:
		zte_bl_2d_3d_gamma_ctrl_display(mode);
		break;
	case ZTE_LCD_DIS_ID:
		g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_dsi_id = mode;
		pr_info("MSM_LCD lcd dsi id = %d\n", mode);
		break;
	case ZTE_LCD_OFF:
	    g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_off = mode;
		pr_info("MSM_LCD lcd off = %d\n", mode);
		break;
	case ZTE_LCD_GESTURE:
	    g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_gesture = mode;
		pr_info("MSM_LCD lcd gesture = %d\n", mode);
		break;
#endif
	default:
		pr_info("MSM_LCD Unsupported index=%d\n", index);
		break;
	}

	mutex_unlock(&g_zte_ctrl_pdata->panel_lock);
	return err;
error:
	mutex_unlock(&g_zte_ctrl_pdata->panel_lock);
	return err;
}

void zte_panel_status_send_uevent(int status)
{
	char *envp[3];

	if (status == LCD_STATUS_POWER_OFF)
		envp[0] = "LCD_STATUS=POWER_OFF";
	else if (status == LCD_STATUS_POWER_ON)
		envp[0] = "LCD_STATUS=POWER_ON";
	else if (status == LCD_STATUS_ENTER_AOD)
		envp[0] = "LCD_STATUS=ENTER_AOD";
	else if (status == LCD_STATUS_EXIT_AOD)
		envp[0] = "LCD_STATUS=EXIT_AOD";

	envp[1] = NULL;
	envp[2] = NULL;

	if (dsi_uevent_device)
		kobject_uevent_env(&dsi_uevent_device->kobj, KOBJ_CHANGE, envp);
	else
		pr_info("[MSM_LCD] dsi_uevent_device is NULL, LCD state send faild\n");
}
/********************read lcm hardware info begin****************/
/*file path: proc/driver/lcd_id */
LCD_PROC_FILE_DEFINE(zte_lcd_info, ZTE_LCD_INFO_CTRL)

const char *zte_get_lcd_panel_name(void)
{
	if (g_zte_ctrl_pdata == NULL || g_zte_ctrl_pdata->zte_lcd_ctrl == NULL)
		return NULL;
	else
		return g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_info;
}
/********************read lcm hardware info end***********************/

#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
LCD_PROC_FILE_DEFINE(zte_lcd_bl_switch, ZTE_LCD_2D_3D_CTRL)
LCD_PROC_FILE_DEFINE(zte_lcd_gamma_switch, ZTE_LCD_DISP_GAMMA)
LCD_PROC_FILE_DEFINE(zte_lcd_dsi_id,ZTE_LCD_DIS_ID)
LCD_PROC_FILE_DEFINE(zte_lcd_off,ZTE_LCD_OFF)
LCD_PROC_FILE_DEFINE(zte_lcd_gesture,ZTE_LCD_GESTURE)

static int zte_gpio_ctrl_lcd_bl_gpio_for_leia(struct device_node *node)
{
	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_2dbl_en_gpio = of_get_named_gpio(node,
		"zte,disp_2dbl_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_2dbl_en_gpio)) {
		pr_info("MSM_LCD zte,disp_2dbl_en_gpio not specified\n");
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_2dbl_en_gpio, "disp_2dbl_en_gpio")) {
			gpio_free(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_2dbl_en_gpio);
			pr_info("MSM_LCD request disp_2dbl_en_gpio failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_2dbl_en_gpio, 1);
			pr_info("MSM_LCD request disp_2dbl_en_gpio success\n");
		}
	}

	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_en_gpio = of_get_named_gpio(node,
		"zte,disp_3dbl_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_en_gpio)) {
		pr_info("MSM_LCD zte,disp_3dbl_en_gpio not specified\n");
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_en_gpio, "disp_3dbl_en_gpio")) {
			gpio_free(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_en_gpio);
			pr_info("MSM_LCD request disp_3dbl_en_gpio failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_en_gpio, 1);
			pr_info("MSM_LCD request disp_3dbl_en_gpio success\n");
		}
	}

	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_adc_gpio = of_get_named_gpio(node,
		"zte,disp_3d3_adc_pin", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_adc_gpio)) {
		pr_info("MSM_LCD zte,disp_3d3_adc_pin not specified\n");
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_adc_gpio, "disp_3d3_adc_pin")) {
			gpio_free(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_adc_gpio);
			pr_info("MSM_LCD request zte,disp_3d3_adc_pin failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_adc_gpio, 1);
			pr_info("MSM_LCD request zte,disp_3d3_adc_pin success\n");
		}
	}
	return 0;
}

#define LP8555_REG_MAX 8
#define LP8555_3D_MAX 4
int zte_gpio_enable_lcd_power_for_leia(u32 zte_bl_status)
{
	u16 offset[LP8555_REG_MAX] = {0x10, 0x0, 0x11, 0x14, 0x16, 0x19, 0x1A, 0x0}; 
	u8 data[LP8555_REG_MAX] = {0x61, 0x0, 0x1, 0xBF, 0x97, 0x3F, 0x97, 0x1};
	int i,j;

	pr_info("MSM_LCD leia zte_bl_status = %d\n", zte_bl_status);
	switch(zte_bl_status) {
		case LCD_IN_2D_BL_OFF:
			pr_info("-------------MSM_LCD 2D OFF begin------------\n");
			usleep_range(1000, 1100);
			if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_2dbl_en_gpio > 0) {
				gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_2dbl_en_gpio, 0);
				usleep_range(5000, 5100);
			}
			pr_info("-------------MSM_LCD 2D OFF end------------\n");
			break;
		case LCD_IN_2D_BL_ON:
			pr_info("-------------MSM_LCD 2D ON begin------------\n");
			usleep_range(1000, 1100);
			if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_2dbl_en_gpio > 0) {
				gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_2dbl_en_gpio, 1);
				usleep_range(5000, 5100);
			}
			lp8555_set_cmds(0x0,0x0);
			lp8555_set_cmds(0x11,0x3);
			lp8555_set_cmds(0x15,0xE5);
			lp8555_set_cmds(0x0,0x1);
			pr_info("-------------MSM_LCD 2D ON end------------\n");
			break;
		case LCD_IN_3D_BL_OFF:
			pr_info("-------------MSM_LCD 3D OFF begin------------\n");
			usleep_range(1000, 1100);
			if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_en_gpio > 0) {
				gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_en_gpio, 0);
				usleep_range(5000, 5100);
			}
			pr_info("-------------MSM_LCD 3D OFF end------------\n");
			break;
		case LCD_IN_3D_BL_ON:
			pr_info("-------------MSM_LCD 3D ON begin------------\n");
			usleep_range(1000, 1100);
			if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_en_gpio > 0) {
				gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_en_gpio, 1);
				usleep_range(5000, 5100);
			}
			if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_adc_gpio > 0) {
				gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_3dbl_adc_gpio, 1);
				usleep_range(5000, 5100);
			}

			for (i = 0; i < LP8555_3D_MAX; i++) {
				for (j = 0; j < LP8555_REG_MAX; j++) {
					if (i == 0) {
						lp8555_3d1_set_cmds(offset[j], data[j]);
					}else if (i == 1){
						lp8555_3d2_set_cmds(offset[j], data[j]);
					}else if (i == 2){
						lp8555_3d3_set_cmds(offset[j], data[j]);
					} else {
						lp8555_3d4_set_cmds(offset[j], data[j]);
					}
				}
			}
			pr_info("-------------MSM_LCD 3D ON end------------\n");
			break;
	}
	return 0;
}

int zte_bl_2d_3d_ctrl_display(u32 mode) {

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}

	if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_bl_switch != mode) {
		g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_bl_switch = mode;
		switch(mode) {
			case LCD_IN_2D_BL_OFF:
				zte_gpio_enable_lcd_power_for_leia(mode);
				break;
			case LCD_IN_2D_BL_ON:
				zte_gpio_enable_lcd_power_for_leia(mode);
				break;
			case LCD_IN_3D_BL_OFF:
				zte_gpio_enable_lcd_power_for_leia(mode);
				break;
			case LCD_IN_3D_BL_ON:
				zte_gpio_enable_lcd_power_for_leia(mode);
				break;
			default:
				pr_info("MSM_LCD do not support this change\n");
				break;
		}
		pr_info("MSM_LCD 2D BL_EN = %d,3D BL_EN = %d\n",
			 g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_2d,
			 g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_3d);
	}
	return 0;
}

int zte_bl_2d_3d_gamma_ctrl_display(u32 mode) {
	int err = 0;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}

	if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_gamma_switch != mode) {
		g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_gamma_switch = mode;
		switch(mode) {
			case LCD_IN_2D_GAMMA:
				err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_2D_GAMMA);
				break;
			case LCD_IN_3D_GAMMA:
				err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_3D_GAMMA);
				break;
			default:
				break;
		}
		if (err) {
			pr_err("[MSM_LCD] lcd_gamma: send cmds failed\n");
		} else {
			pr_info("MSM_LCD change gamma = %d\n", mode);
		}
	}
	return 0;
}

#endif

/********************HBM MODE begin****************/
/*file path: proc/driver/lcd_hbm*/
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
LCD_PROC_FILE_DEFINE(zte_lcd_hbm, ZTE_LCD_HBM_CTRL)

void zte_panel_hbm_send_uevent(int mode, int ret)
{
	char *envp[3];

	if (mode == LCD_STATUS_HBM_OFF_EVENT)
		envp[0] = "HBM_STATUS=OFF";
	else if (mode == LCD_STATUS_HBM_ON_EVENT)
		envp[0] = "HBM_STATUS=ON";
	else if (mode == LCD_STATUS_HBM_FG_RELEASE_EVENT)
		envp[0] = "HBM_STATUS=FG_RELEASE";
	else if (mode == LCD_STATUS_HBM_FG_PRESS_EVENT)
		envp[0] = "HBM_STATUS=FG_PRESS";
	else
		envp[0] = "HBM_STATUS=ERR";

	if (ret == 0)
		envp[1] = "HBM_SET_RESULT=SUCCESSFUL";
	else
		envp[1] = "HBM_SET_RESULT=FAILED";

	envp[2] = NULL;

	if (dsi_uevent_device)
		kobject_uevent_env(&dsi_uevent_device->kobj, KOBJ_CHANGE, envp);
	else
		pr_info("[MSM_LCD]HBM: dsi_uevent_device is NULL\n");
}
int zte_hbm_ctrl_display(u32 setHbm)
{
	int err = 0;
	struct mipi_dsi_device *dsi;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}
	dsi = &g_zte_ctrl_pdata->mipi_device;
	if (!dsi) {
		pr_info("MSM_LCD No dsi device\n");
		return -EINVAL;
	}
	// SDE_ATRACE_BEGIN("complete_hbm_commit");
	if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm != setHbm) {
		g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm = setHbm;
		if (setHbm == 1) {
			if (zte_panel_is_in_aod_mode()) {
				err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_NOLP);
				pr_info("[MSM_LCD]HBM: exit from LP first\n");
			}
			g_zte_ctrl_pdata->zte_lcd_ctrl->hbm_exit_need_dim = 0;
			if (!g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_bl_forbidden) {
				g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_real_bl = g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_max_bl;
				err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_HBM_ON);
				if (err) {
					goto error;
				}
				pr_info("[MSM_LCD]HBM: Send enable HBM bl_level=0xfff,lcd_close_dimreg\n");
			} else {
				pr_info("[MSM_LCD]HBM: lcd_aod_bl_forbidden Send enable HBM skiped\n");
			}
			zte_panel_hbm_send_uevent(LCD_STATUS_HBM_ON_EVENT, 0);
		} else {
			if (zte_panel_is_in_aod_mode()) {
				err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_LP1);
				if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_aod_brightness == AOD_BL_LEVEL_LOW) {
					err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_AOD_LOW);
				} else if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_aod_brightness == AOD_BL_LEVEL_HIGH) {
					err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_AOD_HIGH);
				} else {
					err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_AOD_MID);
				}
				if (err) {
					goto error;
				}
				pr_info("[MSM_LCD]HBM: enter LP again\n");
			} else {
				g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_bl_forbidden = false;
				zte_mipi_dsi_set_real_bl_level(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl);
				/*err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_HBM_OFF);
				if (err) {
					goto error;
				}*/
				g_zte_ctrl_pdata->zte_lcd_ctrl->hbm_exit_need_dim = 1;
				pr_info("[MSM_LCD]HBM: Send disable HBM hbm_exit_need_dim true bl_level=%d\n",
						g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl);
			}
			zte_panel_hbm_send_uevent(LCD_STATUS_HBM_OFF_EVENT, 0);
		}		
	} else {
		pr_info("[MSM_LCD]HBM: New mode is same as old,do nothing mode = %d\n",
				g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm);
	}
	// SDE_ATRACE_END("complete_hbm_commit");
	return err;
error:
	pr_err("[MSM_LCD]HBM: send cmds failed\n");
	return err;
}

int zte_local_hbm_ctrl_display(u32 setHbm)
{
	int err = 0;
	struct mipi_dsi_device *dsi;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}
	dsi = &g_zte_ctrl_pdata->mipi_device;
	if (!dsi) {
		pr_info("MSM_LCD No dsi device\n");
		return -EINVAL;
	}
	if (setHbm == 1) {
		err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_HBM_ON);
		if (err)
			goto error;
		pr_info("[MSM_LCD]HBM: local Send enable HBM cmds to panel\n");
	} else {
		err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_HBM_OFF);
		if (err)
			goto error;
		pr_info("[MSM_LCD]HBM: local Send disable HBM cmds to panel\n");
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm = setHbm;

	return err;
error:
	pr_err("[MSM_LCD]HBM: local send cmds failed\n");
	return err;
}

/*file path: proc/driver/lcd_fgp*/ /* zte_lcd_fgp */
LCD_PROC_FILE_DEFINE(zte_lcd_fgp, ZTE_LCD_FGP_CTRL)
int zte_fgp_ctrl_display(u32 setFgp)
{
	char readbuf[2];
	u8 cmd = 0x52;
	struct mipi_dsi_device *dsi;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}
	dsi = &g_zte_ctrl_pdata->mipi_device;
	if (!dsi) {
		pr_info("MSM_LCD No dsi device\n");
		return -EINVAL;
	}

	switch (setFgp) {
	case 0:
		zte_panel_hbm_send_uevent(LCD_STATUS_HBM_FG_RELEASE_EVENT, 0);
		break;
	case 1:
		zte_panel_hbm_send_uevent(LCD_STATUS_HBM_FG_PRESS_EVENT, 0);
		break;
	case 2:
		zte_dsi_panel_cmd_read(g_zte_ctrl_pdata, cmd, &readbuf[0], 2);
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_real_bl = readbuf[1] | readbuf[0] << 8;
		pr_info("MSM_LCD hbm=%d FGP forbid=%d bl_level=%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm,
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_bl_forbidden, g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_real_bl);
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm != 0) {
			if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_real_bl > 0 && \
				g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_real_bl != g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_max_bl) {
				pr_info("MSM_LCD hbm FGP bl:%d to bl_level=0xfff\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_real_bl);
				zte_mipi_dsi_set_real_bl_level(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_max_bl);
				g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_real_bl = g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_max_bl;
			}
		}
		break;
	default:
		pr_err("MSM_LCD FGP invalid flag: %d\n", setFgp);
		break;
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_fgp = setFgp;

	return 0;
}

/********************HDR MODE begin****************/
/*file path: proc/driver/lcd_hdr*/ /* zte_lcd_hdr */
LCD_PROC_FILE_DEFINE(zte_lcd_hdr, ZTE_LCD_HDR_CTRL)

/*setHdr flag for LCD brightness:
HDR OFF - 0 HDR ON - 1 AUTO SENSOR ON - 2 AUTO SENSOR OFF - 3
These should be same as the defination in sensor file, do not change.
*/
int zte_hdr_ctrl_display(u32 setHdr)
{
	char cal_value = g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hdr;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}
	switch (setHdr) {
	case 0:
		cal_value &= 0xfe;/* clear bit(0) */
		break;
	case 1:
		cal_value |= 0x01;/* set bit(0) */
		break;
	case 2:
		cal_value |= 0x02;/* set bit(1) */
		break;
	case 3:
		cal_value &= 0xfd;/* clear bit(1) */
		break;
	default:
		pr_err("MSM_LCD HDR invalid flag:  %d\n", setHdr);
	break;
	}

	if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hdr != cal_value) {
		g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hdr = cal_value;
		if (g_zte_ctrl_pdata->bl_config.bl_level != 0)
			dsi_panel_set_backlight(g_zte_ctrl_pdata, g_zte_ctrl_pdata->bl_config.bl_level);
		pr_info("MSM_LCD HDR flag: new value: %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hdr);
	} else {
		pr_info("MSM_LCD HDR flag same as old: %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hdr);
	}

	return 0;
}

#endif
/****************************HBM HDR MODE end***************************/

/*****************************AOD BRIGHTNESS begin*********************/
/*file path: proc/driver/lcd_aod_bl/*/ /*zte_lcd_aod_brightness*/
#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
LCD_PROC_FILE_DEFINE(zte_lcd_aod_brightness, ZTE_LCD_AOD_BRIGHTNESS_CTRL)

int zte_panel_is_in_aod_mode(void)
{
	if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_panel_state == 1) /*SDE_MODE_DPMS_LP1*/
		return 1;
	else
		return 0;
}
int zte_aod_brightness_ctrl_display(u32 level)
{
	int err = 0;
	struct mipi_dsi_device *dsi;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}
	dsi = &g_zte_ctrl_pdata->mipi_device;
	if (!dsi) {
		pr_info("MSM_LCD No dsi device");
		return -EINVAL;
	}

	if (!zte_panel_is_in_aod_mode()) {
		if (level == AOD_BL_LEVEL_EXIT) {
			if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl != 0
				&& g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_bl_forbidden) {
				cancel_delayed_work(&g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_exit_aod_delayed_work);
				usleep_range(32*1000, (32*1000+10));
				g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_bl_forbidden = false;
			#ifdef CONFIG_ZTE_LCD_HBM_CTRL
				pr_info("MSM_LCD AODNode restore brightness: lcd_restore_bl bl_level=%d hbm=%d\n",
						g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl,
						g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm);
				if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm == 1) {
					g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_real_bl = g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_max_bl;
					zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_HBM_ON);
				} else {
					zte_mipi_dsi_set_real_bl_level(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl);
				}
			#else
				pr_info("MSM_LCD AODNode restore bl_level=%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl);
				zte_mipi_dsi_set_real_bl_level(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl);
			#endif
			}
		} else {
			pr_info("MSM_LCD AOD: already exit aod mode don't need set aod brightness");
		}
		return 0;
	}
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
	if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm != 0) {
		pr_info("MSM_LCD AOD: don't update brightness when HBM on lcd_aod_brightness=%d\n",
			g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_aod_brightness);
		return 0;
	}
#endif
	if (level == AOD_BL_LEVEL_LOW) {
		err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_AOD_LOW);
	} else if (level == AOD_BL_LEVEL_HIGH) {
		err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_AOD_HIGH);
	} else {
		err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_AOD_MID);
	}
	if (err) {
		goto error;
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_enter_aod_first_set_bl = false;
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_aod_brightness = level;
	pr_info("MSM_LCD AOD: set brightness successful, new level  = %d\n",
			g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_aod_brightness);

	return err;
error:
	pr_err("MSM_LCD AOD: send cmds failed");
	return err;
}

static void lcd_exit_aod_delayed_work(struct work_struct *work)
{
	struct mipi_dsi_device *dsi;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return;
	}
	dsi = &g_zte_ctrl_pdata->mipi_device;
	if (!dsi) {
		pr_info("MSM_LCD No dsi device");
		return;
	}

	mutex_lock(&g_zte_ctrl_pdata->panel_lock);
	if (!g_zte_ctrl_pdata->panel_initialized) {
		pr_err("MSM_LCD exit_aod: Panel not initialized\n");
	} else if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl != 0) {
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_bl_forbidden = false;
	#ifdef CONFIG_ZTE_LCD_HBM_CTRL
		pr_info("MSM_LCD schedule restore brightness:lcd_restore_bl bl_level=%d hbm=%d\n",
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl, g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm);
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm == 1) {
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_real_bl = g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_max_bl;
			zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_HBM_ON);
		} else {
			zte_mipi_dsi_set_real_bl_level(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl);
		}
	#else
		pr_info("MSM_LCD schedule restore bl_level=%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl);
		zte_mipi_dsi_set_real_bl_level(dsi, g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_restore_bl);
	#endif
	} else {
		pr_info("MSM_LCD schedule_delayed_work do nothing exit aod to on mode\n");
	}
	mutex_unlock(&g_zte_ctrl_pdata->panel_lock);

	return;
}
#endif
/***********************AOD BRIGHTNESS end***********************/

/********************COLOR GAMUT MODE start***********************/
#ifdef CONFIG_ZTE_LCD_COLOR_GAMUT_CTRL
/*file path: proc/driver/lcd_color_gamut*//* zte_lcd_color_gamut */
LCD_PROC_FILE_DEFINE(zte_lcd_color_gamut, ZTE_LCD_COLOR_GAMUT_CTRL)

int zte_color_gamut_ctrl_display(u32 index)
{
	int err = 0;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}
	switch (index) {
	case COLOR_GAMUT_ORIGINAL:
		err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_COLOR_ORIGINAL);
		break;
	case COLOR_GAMUT_SRGB:
		err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_COLOR_SRGB);
		break;
	case COLOR_GAMUT_P3:
		err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_COLOR_P3);
		break;
	default:
		pr_err("MSM_LCD Color gamut index %d not supported\n", index);
		break;
	}
	if (err) {
		goto error;
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_color_gamut = index;
	pr_info("msm_lcd Send gamut cmds index=%d ok\n", index);

	return err;
error:
	pr_err("msm_lcd Color gamut: send cmds failed\n");
	return err;
}
#endif
/********************COLOR GAMUT MODE end***********************/

/********************add ACL node start*****************/
#ifdef CONFIG_ZTE_LCD_ACL_CTRL
/*file path: proc/driver/lcd_acl */ 
LCD_PROC_FILE_DEFINE(zte_lcd_acl, ZTE_LCD_ACL_CTRL)

int zte_acl_ctrl_display(u32 acl_level)
{
	int err = 0;

	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return -EINVAL;
	}
	if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_acl != acl_level) {
		g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_acl = acl_level;
		if (acl_level == 1) {
			err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_ACL_LOW);
		} else if (acl_level == 2) {
			err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_ACL_MID);
		} else if (acl_level == 3) {
			err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_ACL_HIGH);
		} else {
			err = zte_dsi_panel_tx_cmd_set(g_zte_ctrl_pdata, DSI_CMD_SET_ZTE_ACL_OFF);
		}
		if (err) {
			goto error;
		}
		pr_info("[MSM_LCD]ACL: Send ACL cmds end ok\n");
	} else {
		pr_info("[MSM_LCD]ACL: New mode is same as old,do nothing mode = %d\n",
				g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_acl);
	}

	return err;
error:
	pr_err("[MSM_LCD]ACL: send cmds failed\n");
	return err;
}
#endif
/*************************add ACL node end**********************/

/********************report current fps start*******************/
/*file path: proc/driver/lcd_fps */ 
#ifdef CONFIG_ZTE_LCD_REPORT_CURRENT_FPS
LCD_PROC_FILE_DEFINE(zte_lcd_cur_fps, ZTE_LCD_FPS_CTRL)

void zte_panel_fps_send_uevent(int fps)
{
	char *envp[3];
	if (fps == 60)
		envp[0] = "LCD_FPS=60";
	else if (fps == 90)
		envp[0] = "LCD_FPS=90";
	else if (fps == 120)
		envp[0] = "LCD_FPS=120";
	else if (fps == 144)
		envp[0] = "LCD_FPS=144";

	envp[1] = NULL;
	envp[2] = NULL;

	if (dsi_uevent_device){
		kobject_uevent_env(&dsi_uevent_device->kobj, KOBJ_CHANGE, envp);
		pr_info("[MSM_LCD]FPS: send fps %d\n",fps);
	} else
		pr_info("[MSM_LCD]FPS: uevent_device is NULL, LCD state send faild\n");
}
enum dsi_cmd_set_type zte_dsi_panel_get_dfps_switch_index(void)
{
	enum dsi_cmd_set_type zte_dfps_switch_index = DSI_CMD_SET_TIMING_SWITCH;

	switch (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_cur_fps) {
	case ZTE_60FPS:
			switch (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_new_fps) {
			case ZTE_90FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_60FPS_TO_90FPS;
				break;
			case ZTE_120FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_60FPS_TO_120FPS;
				break;
			case ZTE_144FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_60FPS_TO_144FPS;
				break;
			default:
				DSI_ERR("MSM_LCD ERROR NEW FPS(%d) not supported\n", 
					g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_new_fps);
			}
		break;
	case ZTE_90FPS:
			switch (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_new_fps) {
			case ZTE_60FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_90FPS_TO_60FPS;
				break;
			case ZTE_120FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_90FPS_TO_120FPS;
				break;
			case ZTE_144FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_90FPS_TO_144FPS;
				break;
			default:
				DSI_ERR("MSM_LCD ERROR NEW FPS(%d) not supported\n", 
					g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_new_fps);
			}
		break;
	case ZTE_120FPS:
			switch (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_new_fps) {
			case ZTE_60FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_120FPS_TO_60FPS;
				break;
			case ZTE_90FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_120FPS_TO_90FPS;
				break;
			case ZTE_144FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_120FPS_TO_144FPS;
				break;
			default:
				DSI_ERR("MSM_LCD ERROR NEW FPS(%d) not supported\n", 
					g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_new_fps);
			}
		break;
	case ZTE_144FPS:
			switch (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_new_fps) {
			case ZTE_60FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_144FPS_TO_60FPS;
				break;
			case ZTE_90FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_144FPS_TO_90FPS;
				break;
			case ZTE_120FPS:
				zte_dfps_switch_index = DSI_CMD_SET_ZTE_144FPS_TO_120FPS;
				break;
			default:
				DSI_ERR("MSM_LCD ERROR NEW FPS(%d) not supported\n", 
					g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_new_fps);
			}
		break;
	default:
		DSI_ERR("MSM_LCD ERROR OLD FPS(%d) not supported\n", g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_cur_fps);
	}
	pr_info("MSM_LCD ZTE dfps switch index = %d\n", zte_dfps_switch_index);

	return zte_dfps_switch_index;
}
/*add for dfps by zte end*/
#endif
/********************report current fps end*****************/

/********************lcd backlight level curve begin*****************/
#ifdef CONFIG_ZTE_LCD_BACKLIGHT_LEVEL_CURVE
enum {	/* lcd curve mode */
	CURVE_MATRIX_MAX_350_LUX = 1,
	CURVE_MATRIX_MAX_400_LUX,
	CURVE_MATRIX_MAX_450_LUX,
	CURVE_MATRIX_MAX_RM692C9_LUX,
};
int zte_backlight_curve_matrix_max_350_lux[256] = {
0, 1, 2, 3, 3, 3, 4, 5, 6, 6, 7, 7, 8, 8, 9, 9,
10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 16, 16, 17, 17, 18,
18, 19, 19, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 26, 27,
28, 28, 29, 29, 30, 31, 31, 32, 33, 33, 34, 35, 35, 36, 36, 37,
38, 38, 39, 40, 40, 41, 42, 43, 43, 44, 45, 45, 46, 47, 47, 48,
49, 50, 50, 51, 52, 52, 53, 54, 55, 55, 56, 57, 58, 58, 59, 60,
61, 61, 62, 63, 64, 64, 65, 66, 67, 68, 68, 69, 70, 71, 72, 73,
74, 75, 76, 77, 77, 78, 79, 80, 81, 82, 82, 83, 84, 85, 86, 87,
88, 88, 89, 90, 91, 92, 93, 94, 95, 96, 96, 97, 98, 99, 100, 101,
102, 103, 104, 105, 106, 107, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132,
133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 146, 147,
148, 149, 151, 152, 153, 155, 156, 157, 159, 160, 162, 163, 164, 166, 167, 169,
170, 172, 173, 175, 176, 178, 179, 181, 183, 184, 186, 187, 189, 191, 192, 194,
196, 197, 199, 201, 203, 204, 206, 208, 210, 212, 214, 215, 217, 219, 221, 223,
225, 227, 229, 231, 233, 235, 237, 239, 241, 243, 245, 248, 250, 252, 254, 255
};

int zte_backlight_curve_matrix_max_400_lux[256] = {
0, 1, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8,
8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16,
16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24,
25, 25, 26, 26, 27, 27, 28, 28, 29, 30, 30, 31, 31, 32, 32, 33,
34, 34, 35, 35, 36, 37, 37, 38, 38, 39, 40, 40, 41, 42, 42, 43,
43, 44, 45, 45, 46, 47, 47, 48, 49, 49, 50, 51, 51, 52, 53, 53,
54, 55, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 63, 64, 64,
65, 66, 66, 67, 68, 69, 69, 70, 71, 72, 72, 73, 74, 74, 75, 76,
77, 78, 78, 79, 80, 81, 81, 82, 83, 84, 84, 85, 86, 87, 88, 88,
89, 90, 91, 92, 92, 93, 94, 95, 96, 96, 97, 98, 99, 100, 101, 101,
102, 103, 104, 105, 106, 107, 107, 108, 109, 110, 111, 112, 113, 113, 114, 115,
116, 117, 118, 119, 120, 121, 121, 122, 123, 124, 125, 126, 127, 128, 129, 129,
130, 132, 133, 134, 136, 137, 139, 140, 142, 143, 145, 147, 148, 150, 151, 153,
155, 156, 158, 160, 162, 163, 165, 167, 169, 170, 172, 174, 176, 178, 180, 182,
184, 186, 188, 190, 192, 194, 196, 198, 200, 203, 205, 207, 209, 212, 214, 216,
219, 221, 223, 226, 228, 231, 233, 236, 238, 241, 243, 246, 249, 252, 254, 255
};

int zte_backlight_curve_matrix_max_450_lux[256] = {
0, 1, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7,
8, 8, 8, 9, 9, 10, 10, 10, 11, 11, 12, 12, 13, 13, 13, 14,
14, 15, 15, 16, 16, 17, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21,
22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29,
30, 30, 31, 31, 32, 32, 33, 33, 34, 34, 35, 36, 36, 37, 37, 38,
38, 39, 39, 40, 41, 41, 42, 42, 43, 43, 44, 45, 45, 46, 46, 47,
48, 48, 49, 49, 50, 51, 51, 52, 52, 53, 54, 54, 55, 56, 56, 57,
57, 58, 59, 59, 60, 61, 61, 62, 63, 63, 64, 65, 65, 66, 67, 67,
68, 69, 69, 70, 71, 71, 72, 73, 73, 74, 75, 75, 76, 77, 78, 78,
79, 80, 80, 81, 82, 83, 83, 84, 85, 85, 86, 87, 88, 88, 89, 90,
91, 91, 92, 93, 94, 94, 95, 96, 97, 97, 98, 99, 100, 101, 101, 102,
103, 104, 105, 105, 106, 107, 108, 109, 109, 110, 111, 112, 112, 112, 113, 113,
114, 116, 117, 119, 120, 122, 123, 125, 126, 128, 130, 131, 133, 135, 136, 138,
140, 142, 143, 145, 147, 149, 151, 153, 155, 157, 159, 161, 163, 165, 167, 169,
171, 173, 176, 178, 180, 183, 185, 187, 190, 192, 194, 197, 199, 202, 205, 207,
210, 213, 215, 218, 221, 224, 226, 229, 232, 235, 238, 241, 244, 248, 251, 255
};

int zte_backlight_curve_matrix_max_rm692c9_lux[256] = {
0, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 12,
13, 13, 13, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17,
17, 18, 18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 21, 22, 22, 22,
23, 23, 23, 24, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29,
30, 30, 31, 31, 32, 32, 33, 33, 34, 34, 35, 36, 36, 37, 37, 38,
38, 39, 39, 40, 41, 41, 42, 42, 43, 43, 44, 45, 45, 46, 46, 47,
48, 48, 49, 49, 50, 51, 51, 52, 52, 53, 54, 54, 55, 56, 56, 57,
57, 58, 59, 59, 60, 61, 61, 62, 63, 63, 64, 65, 65, 66, 67, 67,
68, 69, 69, 70, 71, 71, 72, 73, 73, 74, 75, 75, 76, 77, 78, 78,
79, 80, 80, 81, 82, 83, 83, 84, 85, 85, 86, 87, 88, 88, 89, 90,
91, 91, 92, 93, 94, 94, 95, 96, 97, 97, 98, 99, 100, 101, 101, 102,
103, 104, 105, 105, 106, 107, 108, 109, 109, 110, 111, 112, 112, 112, 113, 113,
114, 116, 117, 119, 120, 122, 123, 125, 126, 128, 130, 131, 133, 135, 136, 138,
140, 142, 143, 145, 147, 149, 151, 153, 155, 157, 159, 161, 163, 165, 167, 169,
171, 173, 176, 178, 180, 183, 185, 187, 190, 192, 194, 197, 199, 202, 205, 207,
210, 213, 215, 218, 221, 224, 226, 229, 232, 235, 238, 241, 244, 248, 251, 255
};

static int zte_convert_backlevel_function(int level, u32 bl_max)
{
	int bl, convert_level;

	if (level == 0)
		return 0;

	bl = level * 255 / bl_max;

	if (!bl && level)
		bl = 1;/*ensure greater than 0 and less than 16 equal to 1*/

	switch (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode) {
	case CURVE_MATRIX_MAX_350_LUX:
		convert_level = zte_backlight_curve_matrix_max_350_lux[bl];
		break;
	case CURVE_MATRIX_MAX_400_LUX:
		convert_level = zte_backlight_curve_matrix_max_400_lux[bl];
		break;
	case CURVE_MATRIX_MAX_450_LUX:
		convert_level = zte_backlight_curve_matrix_max_450_lux[bl];
		break;
	case CURVE_MATRIX_MAX_RM692C9_LUX:
		convert_level = zte_backlight_curve_matrix_max_rm692c9_lux[bl];
		break;
	default:
		convert_level = zte_backlight_curve_matrix_max_450_lux[bl];
		break;
	}

	convert_level = convert_level * bl_max / 255;

	return convert_level;
}
#endif
/********************lcd backlight level curve end*****************/

/********************lcd gpio power ctrl begin***********************/
#ifdef CONFIG_ZTE_LCD_GPIO_CTRL_POWER
static int zte_gpio_enable_lcd_power(int enable)
{
	pr_info("[MSM_LCD] %s:%s\n", __func__, enable ? "enable":"disable");
	if (enable) {
		usleep_range(1000, 1100);
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, 1);
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio > 0) {
			if(!g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_always_on){
               gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, 1);
			   usleep_range(5000, 5100);
			}
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio, 1);
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio, 1);
			usleep_range(5000, 5100);
		}
	} else {
		usleep_range(1000, 1100);
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio, 0);
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio, 0);
			usleep_range(20000, 20100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio > 0) {
			if(!g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_always_on){
               gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, 0);
			   usleep_range(5000, 5100);
			}
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, 0);
			usleep_range(5000, 5100);
		}
	}
	return 0;
}

static int zte_gpio_ctrl_lcd_power_gpio_dt(struct device_node *node)
{
	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio = of_get_named_gpio(node,
		"zte,disp_avdd_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio)) {
		pr_info("MSM_LCD zte,disp_avdd_en_gpio not specified\n");
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, "disp_avdd_en_gpio")) {
			gpio_free(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio);
			pr_info("MSM_LCD request disp_avdd_en_gpio failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, 1);
			pr_info("MSM_LCD request disp_avdd_en_gpio success\n");
		}
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio = of_get_named_gpio(node,
		"zte,disp_iovdd_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio)) {
		pr_info("MSM_LCD zte,disp_iovdd_en_gpio not specified\n");
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, "disp_iovdd_en_gpio")) {
			gpio_free(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio);
			pr_info("MSM_LCD request disp_iovdd_en_gpio failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, 1);
			pr_info("MSM_LCD request disp_iovdd_en_gpio success\n");
		}
		g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_always_on = of_property_read_bool(node,
			"zte,disp_iovdd_always_on");
		pr_info("MSM_LCD disp_iovdd_always_on = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_always_on);
	}

	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio = of_get_named_gpio(node,
		"zte,disp_vsp_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio)) {
		pr_info("MSM_LCD zte,disp_vsp_en_gpio not specified\n");
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio, "disp_vsp_en_gpio")) {
			gpio_free(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio);
			pr_info("MSM_LCD request disp_vsp_en_gpio failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio, 1);
			pr_info("MSM_LCD:request disp_vsp_en_gpio success\n");
		}
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio = of_get_named_gpio(node,
		"zte,disp_vsn_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio)) {
		pr_info("MSM_LCD zte,disp_vsn_en_gpio not specified\n");
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio, "disp_vsn_en_gpio")) {
			gpio_free(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio);
			pr_info("[MSM_LCD]request disp_vsn_en_gpio failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio, 1);
			pr_info("MSM_LCD request disp_vsn_en_gpio success\n");
		}
	}

	return 0;
}
#endif
/********************lcd gpio power ctrl end***********************/

/********************lcd common function start*****************/
static void zte_lcd_panel_parse_dt(struct device_node *node)
{
#ifdef CONFIG_ZTE_LCD_BACKLIGHT_LEVEL_CURVE
	const char *data;
#endif
#ifdef CONFIG_ZTE_LCD_VSP_VSN_VALUE_BY_I2C
	int rc;
	u32 vsp_vsn_value;
#endif

	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_info = of_get_property(node,
		"qcom,mdss-dsi-panel-name", NULL);
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_reset_high_sleeping = of_property_read_bool(node,
										"zte,lcm_reset_pin_keep_high_sleeping");
	of_property_read_u32(node, "zte,lcd_dimreg_value",
			&g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_dimreg_value);
	of_property_read_u32(node, "zte,lcd_close_dimreg_value",
			&g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_close_dimreg_value);
	pr_info("MSM_LCD lcd_dimreg_value = %x, lcd_close_dimreg_value=%x\n",
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_dimreg_value,
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_close_dimreg_value);
#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_bl_reg51_control = of_property_read_bool(node,
										"zte,lcd_aod_bl_reg51_control");
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_bl_reg53_control = of_property_read_bool(node,
										"zte,lcd_aod_bl_reg53_control");
#endif
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
	of_property_read_u32(node, "zte,lcd_hbm_max_bl",
		&g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_max_bl);
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_bl_reg51_control = of_property_read_bool(node,
										"zte,lcd_hbm_bl_reg51_control");
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_bl_reg53_control = of_property_read_bool(node,
										"zte,lcd_hbm_bl_reg53_control");
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_reg51_instant_enabled = of_property_read_bool(node,
										"zte,lcd_hbm_reg51_instant_enabled");
#endif
#ifdef CONFIG_ZTE_LCD_GPIO_CTRL_POWER
	zte_gpio_ctrl_lcd_power_gpio_dt(node);
#endif

#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
	zte_gpio_ctrl_lcd_bl_gpio_for_leia(node);
#endif

#ifdef CONFIG_ZTE_LCD_BACKLIGHT_LEVEL_CURVE
	data = of_get_property(node, "zte,lcm_backlight_curve_mode", NULL);
	if (data) {
		if (!strcmp(data, "lcd_brightness_max_350_lux"))
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_350_LUX;
		else if (!strcmp(data, "lcd_brightness_max_400_lux"))
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_400_LUX;
		else if (!strcmp(data, "lcd_brightness_max_450_lux"))
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_450_LUX;
		else if (!strcmp(data, "lcd_brightness_max_rm692c9_lux"))
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_RM692C9_LUX;
		else
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_450_LUX;
	} else
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_450_LUX;

	pr_info("MSM_LCD curve=%s matrix=%d\n", data, g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode);
#endif

#ifdef CONFIG_ZTE_LCD_VSP_VSN_VALUE_BY_I2C
	rc = of_property_read_u32(node, "zte,lcd_vsp_vsn_voltage", &vsp_vsn_value);
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_vsp_vsn_voltage = (!rc ? vsp_vsn_value : 0xf);
	pr_info("MSM_LCD rc=%d,lcd_vsp_vsn_voltage = %d\n", rc,
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_vsp_vsn_voltage);
#endif

	return;
}
void zte_lcd_common_func(struct dsi_panel *panel, struct device_node *node)
{
	g_zte_ctrl_pdata = panel;
	if (!g_zte_ctrl_pdata) {
		pr_info("MSM_LCD No panel device\n");
		return;
	}
	/*kzalloc zte_lcd_ctrl,must use always in whole life,don't need to free zte_lcd_ctrl*/
	g_zte_ctrl_pdata->zte_lcd_ctrl = kzalloc(sizeof(struct zte_lcd_ctrl_data), GFP_KERNEL);

	if (!g_zte_ctrl_pdata->zte_lcd_ctrl) {
		pr_err("%s: %d MSM_LCD kzalloc memory failed\n", __func__, __LINE__);
		return;
	}

	zte_lcd_panel_parse_dt(node);
	zte_lcd_info_init(node);
#ifdef CONFIG_ZTE_LCD_GPIO_CTRL_POWER
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_gpio_enable_lcd_power = zte_gpio_enable_lcd_power;
#endif
#ifdef CONFIG_ZTE_LCD_BACKLIGHT_LEVEL_CURVE
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_convert_brightness = zte_convert_backlevel_function;
#endif
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
	zte_lcd_hbm_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hbm = 0;
	zte_lcd_hdr_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_hdr = 0;
	zte_lcd_fgp_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_fgp = 0;
#endif
#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
	zte_lcd_aod_brightness_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_aod_brightness = 1;
	INIT_DELAYED_WORK(&g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_exit_aod_delayed_work, lcd_exit_aod_delayed_work);
#endif
#ifdef CONFIG_ZTE_LCD_COLOR_GAMUT_CTRL
	zte_lcd_color_gamut_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_color_gamut = COLOR_GAMUT_P3;
#endif
#ifdef CONFIG_ZTE_LCD_ACL_CTRL
	zte_lcd_acl_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_acl = 0;
#endif
#ifdef CONFIG_ZTE_LCD_REPORT_CURRENT_FPS
	zte_lcd_cur_fps_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_cur_fps = 60;
#endif
#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
	zte_lcd_bl_switch_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_bl_switch = LCD_IN_2D_BL_OFF;
	zte_lcd_gamma_switch_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_gamma_switch = LCD_IN_2D_GAMMA;
	zte_lcd_dsi_id_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_dsi_id = 0;
	zte_lcd_off_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_off = 0;
	zte_lcd_gesture_init(node);
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_gesture = 0;
#endif
#ifdef CONFIG_ZTE_LCD_REG_DEBUG
	zte_lcd_reg_debug_func();
#endif
#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
	zte_3d_device_register();
#endif
	return;
}
/********************lcd common function end*****************/
