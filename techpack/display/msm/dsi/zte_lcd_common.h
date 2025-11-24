
#ifndef _ZTE_LCD_COMMON_H_
#define _ZTE_LCD_COMMON_H_
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
#include "dsi_panel.h"

#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
#include "../zte_disp/zte_disp_i2c.h"
extern void zte_3d_device_register(void);
extern void i2c_set_brightness(u16 brightness);
#endif

#ifdef CONFIG_ZTE_LCD_HBM_CTRL
enum {
	LCD_STATUS_HBM_OFF_EVENT,
	LCD_STATUS_HBM_ON_EVENT,
	LCD_STATUS_HBM_FG_RELEASE_EVENT,
	LCD_STATUS_HBM_FG_PRESS_EVENT
};
void zte_panel_hbm_send_uevent(int mode, int ret);
int zte_hbm_ctrl_display(u32 setHbm);
int zte_local_hbm_ctrl_display(u32 setHbm);
int zte_hdr_ctrl_display(u32 setHdr);
int zte_fgp_ctrl_display(u32 setFgp);
#endif

#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
#define AOD_BL_LEVEL_LOW  0
#define AOD_BL_LEVEL_MIDDLE  1
#define AOD_BL_LEVEL_HIGH  2
#define AOD_BL_LEVEL_OFF  3
#define AOD_BL_LEVEL_EXIT 9
int zte_aod_brightness_ctrl_display(u32 level);
int zte_panel_is_in_aod_mode(void);
#endif

#ifdef CONFIG_ZTE_LCD_COLOR_GAMUT_CTRL
#define COLOR_GAMUT_ORIGINAL    0
#define COLOR_GAMUT_SRGB        1
#define COLOR_GAMUT_P3          2
int zte_color_gamut_ctrl_display(u32 index);
#endif

#ifdef CONFIG_ZTE_LCD_ACL_CTRL
#define LCD_ACL_OFF    0
#define LCD_ACL_LOW    1
#define LCD_ACL_MID    2
#define LCD_ACL_HIGH   3
int zte_acl_ctrl_display(u32 acl_level);
#endif

#ifdef CONFIG_ZTE_LCD_REPORT_CURRENT_FPS
#define ZTE_60FPS      60
#define ZTE_90FPS      90
#define ZTE_120FPS     120
#define ZTE_144FPS     144
enum dsi_cmd_set_type zte_dsi_panel_get_dfps_switch_index(void);
void zte_panel_fps_send_uevent(int fps);
#endif

enum {	/* zte_node_ctrl_panel */
	ZTE_LCD_INFO_CTRL = 0,
	ZTE_LCD_HBM_CTRL,
	ZTE_LCD_HDR_CTRL,
	ZTE_LCD_AOD_BRIGHTNESS_CTRL,
	ZTE_LCD_COLOR_GAMUT_CTRL,
	ZTE_LCD_ACL_CTRL,
	ZTE_LCD_FPS_CTRL,
	ZTE_LCD_FGP_CTRL,
	ZTE_LCD_2D_3D_CTRL,//leia 2d & 3d switch
	ZTE_LCD_DISP_GAMMA,
	ZTE_LCD_DIS_ID,
	ZTE_LCD_OFF,
	ZTE_LCD_GESTURE,
	ZTE_LCD_MAX_CTRL
};
static const char *zte_node_string[ZTE_LCD_MAX_CTRL] = {
	"driver/lcd_id",
	"driver/lcd_hbm",
	"driver/lcd_hdr",
	"driver/lcd_aod_bl",
	"driver/lcd_color_gamut",
	"driver/lcd_acl",
	"driver/lcd_fps",
	"driver/lcd_fgp",
	"driver/lcd_bl_switch",
	"driver/lcd_disp_gamma",
	"driver/lcd_dsi_id",
	"driver/lcd_off",
	"driver/lcd_gesture"
};

enum {	/* read or write mode */
	LCD_STATUS_POWER_OFF = 0,
	LCD_STATUS_POWER_ON,
	LCD_STATUS_ENTER_AOD,
	LCD_STATUS_EXIT_AOD,
	LCD_STATUS_MAX
};

#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
enum {
	LCD_IN_2D_BL_OFF = 0,
	LCD_IN_2D_BL_ON,
	LCD_IN_3D_BL_OFF,
	LCD_IN_3D_BL_ON,
	LCD_BL_MAX
};

enum {
	LCD_IN_2D_GAMMA = 0,
	LCD_IN_3D_GAMMA,
	LCD_IN_MAX
};

int zte_bl_2d_3d_ctrl_display(u32 mode);
int zte_gpio_enable_lcd_power_for_leia(u32 zte_bl_status);
int zte_bl_2d_3d_gamma_ctrl_display(u32 mode);
#endif

const char *zte_get_lcd_panel_name(void);
void zte_panel_status_send_uevent(int status);
void zte_lcd_common_func(struct dsi_panel *panel, struct device_node *node);
int zte_node_write_panel(u32 index, u32 mode);
int zte_dsi_panel_cmd_read(struct dsi_panel *panel, u8 cmd, void *data, size_t len);

#define LCD_PROC_FILE_DEFINE(name, nodeid) \
static ssize_t name##_proc_write(struct file *file, \
		const char __user *buffer, size_t count, loff_t *ppos) \
{ \
	char *tmp = kzalloc((count+1), GFP_KERNEL); \
	u32 mode; \
	if (!tmp) \
		return -ENOMEM; \
	if (copy_from_user(tmp, buffer, count)) { \
		kfree(tmp); \
		return -EFAULT; \
	} \
	mode = *tmp - '0'; \
	zte_node_write_panel(nodeid, mode); \
	kfree(tmp); \
	return count; \
} \
static int name##_show(struct seq_file *m, void *v) \
{ \
	if (nodeid == ZTE_LCD_INFO_CTRL) { \
		seq_printf(m, "panel_name=%s\n", g_zte_ctrl_pdata->zte_lcd_ctrl->name); \
		pr_info("MSM_LCD node name: %s\n", g_zte_ctrl_pdata->zte_lcd_ctrl->name); \
	} else { \
		seq_printf(m, "%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->name); \
		pr_info("MSM_LCD node value: %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->name); \
	} \
	return 0; \
} \
static int name##_proc_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, name##_show, NULL); \
} \
static const struct file_operations name##_proc_fops = { \
	.owner		= THIS_MODULE, \
	.open	= name##_proc_open, \
	.read	= seq_read, \
	.llseek	= seq_lseek, \
	.release	= single_release, \
	.write	= name##_proc_write, \
}; \
static void name##_init(struct device_node *node) \
{ \
	proc_create(zte_node_string[nodeid], 0664, NULL, & name##_proc_fops); \
	return; \
}

#endif /* _ZTE_LCD_COMMON_H_ */
