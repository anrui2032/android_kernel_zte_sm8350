#include "zte_lcd_common.h"

struct dsi_panel *g_zte_ctrl_pdata;

#ifdef CONFIG_ZTE_LCD_REG_DEBUG
extern void zte_lcd_reg_debug_func(void);
#endif
#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
#define AOD_BL_LEVEL_LOW  0
#define AOD_BL_LEVEL_MIDDLE  1
#define AOD_BL_LEVEL_HIGH  2
#define AOD_BL_LEVEL_OFF  3
#endif
extern u32 zte_old_fps;
const char *zte_get_lcd_panel_name(void)
{
	if (g_zte_ctrl_pdata == NULL || g_zte_ctrl_pdata->zte_lcd_ctrl == NULL)
		return NULL;
	else
		return g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name;
}

/********************read lcm hardware info begin****************/
/*file path: proc/driver/lcd_id/ or proc/msm_lcd*/
static int zte_lcd_proc_info_show(struct seq_file *m, void *v)
{
	if (!g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_init_code_version) {
		seq_printf(m, "panel_name=%s\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name);
	} else {
		seq_printf(m, "panel_name=%s,version=%s\n",
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name,
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_init_code_version);
	}
	return 0;
}
static int zte_lcd_proc_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_info_show, NULL);
}
static const struct file_operations zte_lcd_common_func_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release		= single_release,
};
static int zte_lcd_proc_info_display(struct device_node *node)
{
	proc_create("driver/lcd_id", 0664, NULL, &zte_lcd_common_func_proc_fops);

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name = of_get_property(node,
		"qcom,mdss-dsi-panel-name", NULL);
	if (!g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name) {
		pr_info("%s:%d, panel name not found!\n", __func__, __LINE__);
		return -ENODEV;
	}

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_init_code_version = of_get_property(node,
		"zte,lcd-init-code-version", NULL);

	pr_info("[MSM_LCD]%s: Panel Name = %s,init code version=%s\n", __func__,
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name,
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_init_code_version);

	return 0;
}
/********************read lcm hardware info end***********************/

/********************HBM MODE begin****************/
/*file path: proc/driver/lcd_hbm/*/
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
struct device *dsi_uevent_device = NULL;

int zte_hbm_ctrl_display_default(struct dsi_panel *panel, u32 setHbm)
{
	int err = 0;
	unsigned long mode_flags = 0;
	struct mipi_dsi_device *dsi;
	u8 dcs_bl_cmd[3] = {0x51, 0x0f, 0xff};/*default hbm max brightness 0xfff*/
	u8 dcs_dim_cmd[2] = {0x53, 0x20};

	dsi = &panel->mipi_device;
	if (!dsi) {
		pr_info("[MSM_LCD]HBM: No device");
		return -ENOMEM;
	}
	mutex_lock(&panel->panel_lock);
	if (!panel->panel_initialized) {
		err = -EPERM;
		pr_err("[MSM_LCD]HBM: panel is off or not initialized1\n");
		goto error;
	}

	if (setHbm == 1) {
		if (panel_is_in_aod_mode()) {
			err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP);
			if (err) {
				pr_info("[MSM_LCD]HBM: exit from LP error\n");
				goto error;
			}
			pr_info("[MSM_LCD]HBM: exit from LP first\n");
		}
		//dcs_dim_cmd[1] = g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_close_dimreg_value;
		panel->zte_lcd_ctrl->hbm_exit_need_dim = 0;
		if ((!strcmp(panel->name, "Visionox-NT37701-1080-2400-6P67Inch")) ||
			(!strcmp(panel->name, "Visionox-NT37701-GAMUT-1080-2400-6P67Inch"))) {
			if (zte_old_fps == 60)
				usleep_range(3000, 3100);
			else if (zte_old_fps == 90)
				usleep_range(3000, 3100);
		}
		err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_HBM_ON);
		if (err) {
			pr_info("[MSM_LCD]HBM: ON mipi_dsi_dcs_write write bl error\n");
			goto error;
		}

		pr_info("[MSM_LCD]HBM: Send enable HBM 3ms bl_level=0xfff,lcd_close_dimreg\n");
	} else {
		if (panel_is_in_aod_mode()) {
			if ((!strcmp(panel->name, "Visionox-RM692E1-1080-2400-6P67Inch")) ||
				(!strcmp(panel->name, "Visionox-RM692E1-HBM51-1080-2400-6P67Inch")))
				err = zte_dsi_panel_tx_cmd_set(panel, dsi_panel_get_zte_dfps_aod_switch_index());
			else
				err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_LP1);
			if (err) {
				pr_info("[MSM_LCD]HBM: enter LP again error\n");
				goto error;
			}
			if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness == AOD_BL_LEVEL_LOW) {
				err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_AOD_LOW);
				if (err) {
					pr_info("[MSM_LCD]HBM: set LP brightness error\n");
					goto error;
				}
			} else if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness == AOD_BL_LEVEL_HIGH) {
				err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_AOD_HIGH);
				if (err) {
					pr_info("[MSM_LCD]HBM: set LP brightness error\n");
					goto error;
				}
			} else {
				err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_AOD_MID);
				if (err) {
					pr_info("[MSM_LCD]HBM: set LP brightness error\n");
					goto error;
				}
			}
			pr_info("[MSM_LCD]HBM: enter LP again\n");
		} else {
			dcs_bl_cmd[1] = panel->zte_lcd_ctrl->hbm_keep_bl >> 8;
			dcs_bl_cmd[2] = panel->zte_lcd_ctrl->hbm_keep_bl & 0xff;
			dcs_dim_cmd[1] = g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_close_dimreg_value;
			if (unlikely(panel->bl_config.lp_mode)) {
				mode_flags = dsi->mode_flags;
				dsi->mode_flags |= MIPI_DSI_MODE_LPM;
			}
			err = mipi_dsi_dcs_write(dsi, dcs_dim_cmd[0], &dcs_dim_cmd[1], sizeof(dcs_dim_cmd) - 1);
			err = mipi_dsi_dcs_write(dsi, dcs_bl_cmd[0], &dcs_bl_cmd[1], sizeof(dcs_bl_cmd) - 1);
			if (unlikely(panel->bl_config.lp_mode))
				dsi->mode_flags = mode_flags;

			if (err < 0) {
				pr_info("[MSM_LCD]HBM: OFF mipi_dsi_dcs_write write bl error\n");
				goto error;
			}
			if ((!strcmp(panel->name, "Visionox-RM692E1-1080-2400-6P67Inch")) ||
				(!strcmp(panel->name, "Visionox-RM692E1-HBM51-1080-2400-6P67Inch"))) {
				if (zte_old_fps == 60)
					usleep_range(12000, 12100);
				else if (zte_old_fps == 90)
					usleep_range(5000, 5100);
			}
		}
		panel->zte_lcd_ctrl->hbm_exit_need_dim = 1;
		pr_info("[MSM_LCD]HBM: Send disable HBM hbm_exit_need_dim true hbm_keep_bl bl_level=%d\n",
				 panel->zte_lcd_ctrl->hbm_keep_bl);
	}

	mutex_unlock(&panel->panel_lock);
	return err;
error:
	mutex_unlock(&panel->panel_lock);
	pr_err("[MSM_LCD]HBM: send cmds failed");
	return err;
}

int zte_hbm_ctrl_display_global(struct dsi_panel *panel, u32 setHbm){
	int err = 0;
	struct mipi_dsi_device *dsi;

	dsi = &panel->mipi_device;
	if (!dsi) {
		pr_info("[MSM_LCD]HBM: global No device\n");
		return -ENOMEM;
	}

	mutex_lock(&panel->panel_lock);
	if (!panel->panel_initialized) {
		err = -EPERM;
		pr_err("[MSM_LCD]HBM: global panel is off or not initialized1\n");
		goto error;
	}

	if (setHbm == 1) {
		err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_GLOBAL_HBM_ON);
		if (err)
			goto error;
		pr_info("[MSM_LCD]HBM: global Send enable HBM cmds to panel\n");
	} else {
		err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_GLOBAL_HBM_OFF);
		if (err)
			goto error;
		pr_info("[MSM_LCD]HBM: global Send disable HBM cmds to panel\n");
	}

	mutex_unlock(&panel->panel_lock);
	return err;
error:
	mutex_unlock(&panel->panel_lock);
	pr_err("[MSM_LCD]HBM: global send cmds failed\n");
	return err;
}

static int zte_lcd_proc_hbm_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode);
	pr_info("[MSM_LCD]HBM: read value = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode);

	return 0;
}
void panel_hbm_send_uevent(int mode, int ret)
{
	char *envp[3];

	if (mode == 0)
		envp[0] = "HBM_STATUS=OFF";
	else
		envp[0] = "HBM_STATUS=ON";

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

void panel_state_send_uevent(int state)
{
	char *envp[3];

	if (state == 0)
		envp[0] = "LCD_STATUS=OFF";
	else if (state == 1)
		envp[0] = "LCD_STATUS=ON";
	else if (state == 2)
		envp[0] = "LCD_STATUS=AOD";

	envp[1] = NULL;
	envp[2] = NULL;

	if (dsi_uevent_device)
		kobject_uevent_env(&dsi_uevent_device->kobj, KOBJ_CHANGE, envp);
	else
		pr_info("[MSM_LCD]HBM: dsi_uevent_device is NULL, LCD state send faild\n");
}
static ssize_t zte_lcd_proc_hbm_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *f_pos)
{
	char *tmp = kzalloc((count+1), GFP_KERNEL);
	u32 mode;
	int ret = 0, retry_times = 3;

	if (!tmp)
		return -ENOMEM;

	if (!g_zte_ctrl_pdata->panel_initialized) {
		pr_info("[MSM_LCD]HBM: Panel not initialized\n");
		kfree(tmp);
		return -ENOMEM;
	}

	if (copy_from_user(tmp, buffer, count)) {
		kfree(tmp);
		return -EFAULT;
	}

	mode = *tmp - '0';
	if (mode > 1) {
		pr_info("[MSM_LCD]HBM: Send HBM wrong mode return\n");
		kfree(tmp);
		return count;
	}

	if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode != mode) {
		while (retry_times--) {
			pr_info("[MSM_LCD]HBM: Send HBM cmds START\n");
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode = mode;
			if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_bl_reg53_control) {
				ret = zte_hbm_ctrl_display_global(g_zte_ctrl_pdata, mode);
			} else {
				ret = zte_hbm_ctrl_display_default(g_zte_ctrl_pdata, mode);
			}
			if (ret == 0) {
				pr_info("[MSM_LCD]HBM: Set new mode successful, new mode = %d\n",
						g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode);
				break;
			}
			pr_info("[MSM_LCD]HBM: Set new mode failed, mode = %d, retry_times = %d\n",
						g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode, retry_times);
		}
		panel_hbm_send_uevent(g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode, ret);

	} else {
		pr_info("[MSM_LCD]HBM: New mode is same as old ,do nothing! mode = %d\n",
				g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode);
	}

	kfree(tmp);
	return count;
}

static int zte_lcd_proc_hbm_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_hbm_show, NULL);
}

static const struct file_operations zte_lcd_hbm_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_hbm_open,
	.read		= seq_read,
	.write		= zte_lcd_proc_hbm_write,
	.llseek		= seq_lseek,
	.release		= single_release,
};

static int zte_lcd_hbm_ctrl(struct device_node *node)
{
	proc_create("driver/lcd_hbm", 0664, NULL, &zte_lcd_hbm_proc_fops);

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode = 0;
	pr_info("[MSM_LCD]HBM: = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode);

	return 0;
}
/*
setHdr flag for LCD brightness:
HDR OFF - 0
HDR ON - 1
AUTO SENSOR ON - 2
AUTO SENSOR OFF - 3
These should be same as the defination in sensor file, do not change.
*/
void panel_set_hdr_flag(struct dsi_panel *panel, u32 setHdr)
{
	u32 bl_lvl_bak = panel->bl_config.bl_level;
	char cal_value = g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on;

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
		pr_err("HDR invalid flag:  %d\n", setHdr);
	break;
	}

	if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on != cal_value) {
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on = cal_value;
		if (bl_lvl_bak != 0)
			dsi_panel_set_backlight(panel, bl_lvl_bak);
		pr_info("HDR flag:  new value: %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on);
	} else {
		pr_info("HDR flag is same as old: %d, do nothing\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on);
	}

}
static ssize_t zte_lcd_proc_hdr_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *f_pos)
{
	char *tmp = kzalloc((count+1), GFP_KERNEL);
	u32 mode;

	if (!tmp)
		return -ENOMEM;

	if (!g_zte_ctrl_pdata->panel_initialized) {
		pr_info("HDR: Panel not initialized\n");
		kfree(tmp);
		return -ENOMEM;
	}

	if (copy_from_user(tmp, buffer, count)) {
		kfree(tmp);
		return -EFAULT;
	}

	mode = *tmp - '0';

	panel_set_hdr_flag(g_zte_ctrl_pdata, mode);

	kfree(tmp);
	return count;
}
static int zte_lcd_proc_hdr_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on);
	pr_info("HDR flag:  %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on);
	return 0;
}
static int zte_lcd_proc_hdr_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_hdr_show, NULL);
}
static const struct file_operations zte_lcd_hdr_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_hdr_open,
	.read		= seq_read,
	.write		= zte_lcd_proc_hdr_write,
	.llseek		= seq_lseek,
	.release		= single_release,
};
static int zte_lcd_set_hdr_flag(struct device_node *node)
{
	proc_create("driver/lcd_hdr", 0664, NULL, &zte_lcd_hdr_proc_fops);

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on = 0;
	pr_info("HDR flag:  = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on);

	return 0;
}
#endif

/********************COLOR GAMUT MODE start***********************/
#ifdef CONFIG_ZTE_LCD_COLOR_GAMUT_CTRL
int rm692c9_color_gamut_set(struct dsi_panel *panel, u32 index){
	int err = 0;
	struct mipi_dsi_device *dsi;

	dsi = &panel->mipi_device;
	if (!dsi) {
		pr_info("msm_lcd Sec panel: No device\n");
		return -ENOMEM;
	}

	mutex_lock(&panel->panel_lock);
	if (!panel->panel_initialized) {
		err = -EPERM;
		pr_err("msm_lcd Sec panel: panel is off or not initialized1\n");
		goto error;
	}

	switch (index) {
	case COLOR_GAMUT_ORIGINAL:
		err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_COLOR_ORIGINAL);
		if (err) {
			pr_err("msm_lcd send DSI_CMD_SET_ZTE_COLOR_ORIGINAL failed");
			goto error;
		}
		pr_info("msm_lcd Send original gamut cmds ok\n");
		break;
	case COLOR_GAMUT_SRGB:
		err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_COLOR_SRGB);
		if (err) {
			pr_err("msm_lcd send DSI_CMD_SET_ZTE_COLOR_SRGB failed");
			goto error;
		}
		pr_info("msm_lcd Send srgb gamut cmds ok\n");
		break;
	case COLOR_GAMUT_P3:
		err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_COLOR_P3);
		if (err) {
			pr_err("msm_lcd send DSI_CMD_SET_ZTE_COLOR_P3 failed");
			goto error;
		}
		pr_info("msm_lcd Send dcip3 gamut cmds ok\n");
		break;
	default:
		pr_err("Color gamut index %d  not supported\n", index);
		break;
	}
	mutex_unlock(&panel->panel_lock);

	return err;

error:
	mutex_unlock(&panel->panel_lock);
	pr_err("msm_lcd Color gamut: send cmds failed\n");
	return err;
}

static ssize_t zte_lcd_proc_color_gamut_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *f_pos)
{
	char *tmp = kzalloc((count+1), GFP_KERNEL);
	u32 index;
	int ret = 0, retry_times = 3;

	if (!tmp)
		return -ENOMEM;

	if (!g_zte_ctrl_pdata->panel_initialized) {
		pr_info("msm_lcd Color gamut: Panel not initialized\n");
		kfree(tmp);
		return -ENOMEM;
	}

	if (copy_from_user(tmp, buffer, count)) {
		kfree(tmp);
		return -EFAULT;
	}

	pr_info("msm_lcd Color gamut: Panel Name is %s\n", g_zte_ctrl_pdata->name);

	index = *tmp - '0';
	if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_color_gamut_index != index) {
		while (retry_times--) {
			ret = rm692c9_color_gamut_set(g_zte_ctrl_pdata, index);
			if (ret == 0) {
				g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_color_gamut_index = index;
				pr_info("msm_lcd Set new color gamut successful, new index = %d\n",
					g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_color_gamut_index);
				break;
			}
			pr_info("msm_lcd Set new color gamut failed, index = %d, retry_times = %d\n",
					g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_color_gamut_index, retry_times);
		}

	} else {
		pr_info("msm_lcd New color gamut index is same as old ,do nothing! index = %d\n",
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_color_gamut_index);
	}
	kfree(tmp);
	return count;
}
static int zte_lcd_proc_color_gamut_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_color_gamut_index);
	pr_info("msm_lcd Color gamut: state = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_color_gamut_index);
	return 0;
}
static int zte_lcd_proc_color_gamut_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_color_gamut_show, NULL);
}

static const struct file_operations zte_lcd_color_gamut_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_color_gamut_open,
	.read		= seq_read,
	.write		= zte_lcd_proc_color_gamut_write,
	.llseek		= seq_lseek,
	.release		= single_release,
};

static int zte_lcd_color_gamut_ctrl(struct device_node *node)
{
	proc_create("driver/lcd_color_gamut", 0664, NULL, &zte_lcd_color_gamut_fops);

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_color_gamut_index = COLOR_GAMUT_P3;
	pr_info("Color gamut: state = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_color_gamut_index);

	return 0;
}
#endif
/********************COLOR GAMUT MODE end***********************/

/********************AOD BRIGHTNESS begin****************/
/*file path: proc/driver/lcd_aod_bl/*/
#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
int panel_set_aod_brightness(struct dsi_panel *panel, u32 level)
{
	int err = 0;
	struct mipi_dsi_device *dsi;

	dsi = &panel->mipi_device;
	if (!dsi) {
		pr_info("MSM_LCD AOD: No device");
		return -ENOMEM;
	}
	if (!panel->panel_initialized) {
		pr_info("MSM_LCD AOD: Panel not initialized\n");
		return -ENOMEM;
	}
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
	if (!panel->zte_lcd_ctrl->lcd_hbm_bl_reg53_control) {
		if (panel->zte_lcd_ctrl->lcd_hbm_mode != 0) {
			pr_info("MSM_LCD AOD: don't update brightness when HBM on lcd_aod_brightness=%d\n",
				panel->zte_lcd_ctrl->lcd_aod_brightness);
			return err;
		}
	}
#endif
	mutex_lock(&panel->panel_lock);
	if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_panel_state != 1) {
		pr_err("MSM_LCD AOD: already exit aod mode don't need set aod brightness");
		mutex_unlock(&panel->panel_lock);
		return err;
	}

	if (level == AOD_BL_LEVEL_LOW) {
		err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_AOD_LOW);
			if (err) {
				pr_err("MSM_LCD AOD: send DSI_CMD_SET_ZTE_AOD_LOW failed");
				goto error;
			}
	} else if (level == AOD_BL_LEVEL_MIDDLE) {
		err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_AOD_MID);
			if (err) {
				pr_err("MSM_LCD AOD: send DSI_CMD_SET_ZTE_AOD_MID failed");
				goto error;
			}
	} else if (level == AOD_BL_LEVEL_HIGH) {
		err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_AOD_HIGH);
			if (err) {
				pr_err("MSM_LCD AOD: send DSI_CMD_SET_ZTE_AOD_HIGH failed");
				goto error;
			}
	} else {
		err = zte_dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ZTE_AOD_MID);
			if (err) {
				pr_err("MSM_LCD AOD: send DSI_CMD_SET_ZTE_AOD_MID default failed");
				goto error;
			}
	}
	mutex_unlock(&panel->panel_lock);
	pr_info("MSM_LCD AOD: set brightness successful, new level  = %d\n",
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness);

	return err;

error:
	mutex_unlock(&panel->panel_lock);
	pr_err("MSM_LCD AOD: send cmds failed");
	return err;
}

int panel_is_in_aod_mode(void)
{
	if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_panel_state == 1)
		return 1;
	else
		return 0;
}

static ssize_t zte_lcd_proc_aod_bl_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *f_pos)
{
	char *tmp = kzalloc((count+1), GFP_KERNEL);
	u32 level;

	if (!tmp)
		return -ENOMEM;

	if (copy_from_user(tmp, buffer, count)) {
		kfree(tmp);
		return -EFAULT;
	}

	level = *tmp - '0';
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness = level;
	pr_info("AOD set new bl level: %d\n", level);

	if (g_zte_ctrl_pdata->zte_lcd_ctrl->zte_panel_state == 1) /* set brightness here if already in aod mode*/
		panel_set_aod_brightness(g_zte_ctrl_pdata, level);
	else
		pr_info("AOD save new bl level, not in aod mode now.\n");

	kfree(tmp);
	return count;
}
static int zte_lcd_proc_aod_bl_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness);
	pr_info("AOD brightness:  = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness);
	return 0;
}
static int zte_lcd_proc_aod_bl_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_aod_bl_show, NULL);
}
static const struct file_operations zte_lcd_aod_bl_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_aod_bl_open,
	.read		= seq_read,
	.write		= zte_lcd_proc_aod_bl_write,
	.llseek		= seq_lseek,
	.release		= single_release,
};
static int zte_lcd_set_aod_brightness(struct device_node *node)
{
	proc_create("driver/lcd_aod_bl", 0664, NULL, &zte_lcd_aod_bl_proc_fops);

	/* 0 - low level, 1 - middle level(default), 2 - high level*/
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness = 1;

	pr_info("AOD brightness level:  = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness);

	return 0;
}
#endif
/********************AOD BRIGHTNESS end***********************/

/********************report current fps start*****************/
#ifdef CONFIG_ZTE_LCD_REPORT_CURRENT_FPS
static int zte_lcd_proc_fps_show(struct seq_file *m, void *v)
{
	u32 refresh_rate = zte_old_fps;
	seq_printf(m, "%d\n", refresh_rate);
	pr_info("MSM_LCD current kernel fps:  %d\n", refresh_rate);
	return 0;
}
static int zte_lcd_proc_fps_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_fps_show, NULL);
}
static const struct file_operations zte_lcd_fps_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_fps_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
static int zte_lcd_report_fps(struct device_node *node)
{
	proc_create("driver/lcd_fps", 0664, NULL, &zte_lcd_fps_proc_fops);
	pr_info("MSM_LCD create fps node\n");

	return 0;
}
#endif
/********************report current fps end*****************/

/********************add ACL node start*****************/
#ifdef CONFIG_ZTE_LCD_ACL_CTRL
static int zte_lcd_proc_acl_show(struct seq_file *m, void *v)
{
	u32 acl_level = g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_acl_level;
	seq_printf(m, "%d\n", acl_level);
	pr_info("MSM_LCD ACL level:  %d\n", acl_level);
	return 0;
}
int zte_lcd_set_acl_level(struct dsi_panel *panel, u32 acl_level)
{
	int err = 0;
	unsigned long mode_flags = 0;
	struct mipi_dsi_device *dsi;
	u8 acl_cmd[2] = {0x55, 0x00};

	dsi = &panel->mipi_device;
	if (!dsi) {
		pr_info("[MSM_LCD] ACL: No device");
		return -ENOMEM;
	}
	mutex_lock(&panel->panel_lock);
	if (!panel->panel_initialized) {
		err = -EPERM;
		pr_err("[MSM_LCD] ACL: panel is not initialized\n");
		goto error;
	}

	acl_cmd[1] = acl_level;
	if (unlikely(panel->bl_config.lp_mode)) {
		mode_flags = dsi->mode_flags;
		dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	}
	err = mipi_dsi_dcs_write(dsi, acl_cmd[0], &acl_cmd[1], sizeof(acl_cmd) - 1);
	if (unlikely(panel->bl_config.lp_mode))
		dsi->mode_flags = mode_flags;
	if (err < 0) {
		pr_info("[MSM_LCD]ACL: mipi_dsi_dcs_write write  error\n");
		goto error;
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_acl_level = acl_level;
	pr_info("[MSM_LCD]ACL: acl_level = %d\n", acl_level);

	mutex_unlock(&panel->panel_lock);
	return err;

error:
	mutex_unlock(&panel->panel_lock);
	pr_err("[MSM_LCD]ACL: send cmds failed");
	return err;
}
static ssize_t zte_lcd_proc_acl_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *f_pos)
{
	char *val = kzalloc((count+1), GFP_KERNEL);
	u32 level;

	if (!val)
		return -ENOMEM;

	if (!g_zte_ctrl_pdata->panel_initialized) {
		pr_info("HDR: Panel not initialized\n");
		kfree(val);
		return -ENOMEM;
	}

	if (copy_from_user(val, buffer, count)) {
		kfree(val);
		return -EFAULT;
	}

	level = *val - '0';
	if ((!strcmp(g_zte_ctrl_pdata->name, "Visionox-RM692E1-1080-2400-6P67Inch")) ||
		(!strcmp(g_zte_ctrl_pdata->name, "Visionox-RM692E1-HBM51-1080-2400-6P67Inch")))
		zte_lcd_set_acl_level(g_zte_ctrl_pdata, level);
	else
		pr_err("[MSM_LCD] ACL: This panel does not support now\n");

	kfree(val);
	return count;
}
static int zte_lcd_proc_acl_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_acl_show, NULL);
}
static const struct file_operations zte_lcd_acl_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_acl_open,
	.read		= seq_read,
	.write		= zte_lcd_proc_acl_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};
static int zte_lcd_create_acl_node(struct device_node *node)
{
	proc_create("driver/lcd_acl", 0664, NULL, &zte_lcd_acl_proc_fops);
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_acl_level = 0;
	pr_info("MSM_LCD create ACL node, lcd_acl_level = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_acl_level);

	return 0;
}
#endif
/********************add ACL node end*****************/

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

	if ((bl_max != 4095) && (bl_max != 1023) && (bl_max != 255)) {
		bl = level * 255 / bl_max;
	} else if (bl_max > 1023) {
		bl = level>>4;
	} else if (bl_max > 255) {
		bl = level >> 2;
	} else {
		bl = level;
	}

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

	if ((bl_max != 4095) && (bl_max != 1023) && (bl_max != 255)) {
		convert_level = convert_level * bl_max / 255;
	} else if (bl_max > 1023) {
		convert_level = (convert_level >= 255) ? 4095 : (convert_level<<4);
	} else if (bl_max > 255) {
		convert_level = (convert_level >= 255) ? 1023 : (convert_level<<2);
	}

	return convert_level;
}
#endif
/********************lcd backlight level curve end*****************/

/********************lcd gpio power ctrl begin***********************/
#ifdef CONFIG_ZTE_LCD_GPIO_CTRL_POWER
static int zte_gpio_ctrl_lcd_power_enable(int enable)
{
	pr_info("[MSM_LCD] %s:%s\n", __func__, enable ? "enable":"disable");
	if (enable) {
		usleep_range(5000, 5100);
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, 1);
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, 1);
			usleep_range(5000, 5100);
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
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, 0);
			usleep_range(5000, 5100);
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
		pr_info("[MSM_LCD]%s:%d, zte,disp_avdd_en_gpio not specified\n", __func__, __LINE__);
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, "disp_avdd_en_gpio")) {
			pr_info("request disp_avdd_en_gpio failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, 1);
			pr_info("%s:request disp_avdd_en_gpio success\n", __func__);
		}
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio = of_get_named_gpio(node,
		"zte,disp_iovdd_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio)) {
		pr_info("[MSM_LCD]%s:%d, zte,disp_iovdd_en_gpio not specified\n", __func__, __LINE__);
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, "disp_iovdd_en_gpio")) {
			pr_info("request disp_iovdd_en_gpio failed %d\n",
				g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio);
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, 1);
			pr_info("%s:request disp_iovdd_en_gpio success\n", __func__);
		}
	}

	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio = of_get_named_gpio(node,
		"zte,disp_vsp_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio)) {
		pr_info("[MSM_LCD]%s:%d, zte,disp_vsp_en_gpio not specified\n", __func__, __LINE__);
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio, "disp_vsp_en_gpio")) {
			pr_info("[MSM_LCD]request disp_vsp_en_gpio failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio, 1);
			pr_info("[MSM_LCD]%s:request disp_vsp_en_gpio success\n", __func__);
		}
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio = of_get_named_gpio(node,
		"zte,disp_vsn_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio)) {
		pr_info("[MSM_LCD]%s:%d, zte,disp_vsn_en_gpio not specified\n", __func__, __LINE__);
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio, "disp_vsn_en_gpio")) {
			pr_info("[MSM_LCD]request disp_vsn_en_gpio failed %d\n",
				g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio);
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio, 1);
			pr_info("[MSM_LCD]%s:request disp_vsn_en_gpio success\n", __func__);
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

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_reset_high_sleeping = of_property_read_bool(node,
										"zte,lcm_reset_pin_keep_high_sleeping");

	of_property_read_u32(node, "zte,lcd_dimreg_value",
			&g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_dimreg_value);
	of_property_read_u32(node, "zte,lcd_close_dimreg_value",
			&g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_close_dimreg_value);
	pr_info("[MSM_LCD]%s lcd_dimreg_value = %x, lcd_close_dimreg_value=%x\n", __func__,
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_dimreg_value,
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_close_dimreg_value);
#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_bl_reg51_control = of_property_read_bool(node,
										"zte,lcd_aod_bl_reg51_control");
#endif
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
	of_property_read_u32(node, "zte,lcd_hbm_max_bl",
			&g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_max_bl);
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_bl_reg53_control = of_property_read_bool(node,
										"zte,lcd_hbm_bl_reg53_control");
#endif
#ifdef CONFIG_ZTE_LCD_GPIO_CTRL_POWER
	zte_gpio_ctrl_lcd_power_gpio_dt(node);
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

	pr_info("[MSM_LCD]%s:dtsi_mode=%s matrix_mode=%d\n", __func__, data,
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode);
#endif

#ifdef CONFIG_ZTE_LCD_VSP_VSN_VALUE_BY_I2C
	rc = of_property_read_u32(node, "zte,lcd_vsp_vsn_voltage", &vsp_vsn_value);
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_vsp_vsn_voltage = (!rc ? vsp_vsn_value : 0xf);
	pr_info("[MSM_LCD]%s rc=%d,lcd_vsp_vsn_voltage = %d\n", __func__, rc,
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_vsp_vsn_voltage);
#endif
}
void zte_lcd_common_func(struct dsi_panel *panel, struct device_node *node)
{
	g_zte_ctrl_pdata = panel;
	/*kzalloc zte_lcd_ctrl,must use always in whole life,don't need to free zte_lcd_ctrl*/
	g_zte_ctrl_pdata->zte_lcd_ctrl = kzalloc(sizeof(struct zte_lcd_ctrl_data), GFP_KERNEL);

	if (!g_zte_ctrl_pdata->zte_lcd_ctrl) {
		pr_err("%s:kzalloc memory failed\n", __func__);
		return;
	}

	zte_lcd_panel_parse_dt(node);
	zte_lcd_proc_info_display(node);
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
	zte_lcd_hbm_ctrl(node);
	zte_lcd_set_hdr_flag(node);
#endif
#ifdef CONFIG_ZTE_LCD_COLOR_GAMUT_CTRL
	zte_lcd_color_gamut_ctrl(node);
#endif
#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
	zte_lcd_set_aod_brightness(node);
#endif
#ifdef CONFIG_ZTE_LCD_GPIO_CTRL_POWER
	g_zte_ctrl_pdata->zte_lcd_ctrl->gpio_enable_lcd_power = zte_gpio_ctrl_lcd_power_enable;
#endif
#ifdef CONFIG_ZTE_LCD_BACKLIGHT_LEVEL_CURVE
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_convert_brightness = zte_convert_backlevel_function;
#endif
#ifdef CONFIG_ZTE_LCD_REPORT_CURRENT_FPS
	zte_lcd_report_fps(node);
#endif
#ifdef CONFIG_ZTE_LCD_ACL_CTRL
	zte_lcd_create_acl_node(node);
#endif
#ifdef CONFIG_ZTE_LCD_REG_DEBUG
	zte_lcd_reg_debug_func();
#endif
}
/********************lcd common function end*****************/
