#include "zte_lcd_common.h"
/*
*echo ff988100 > gwrite (0x13,0x29) or echo 51ff > dwrite (0x15,0x39)
*echo 5401 > dread(0x14,0x24), then cat dread
*dread (0x06) sometimes read nothing,return error
*file path: sys/reg_debug
*/

#define ZTE_REG_LEN 64
#define BRTLO 0x03
#define BRTHI 0x04
#define BLMAX 3490

/*WARNING: Single statement macros should not use a do {} while (0) loop*/
//#define ZTE_LCD_INFO(fmt, args...) {pr_info("[MSM_LCD][Info]"fmt, ##args); }
#define SYSFS_FOLDER_NAME "leia_3d_device"
extern struct dsi_panel *g_zte_ctrl_pdata;
void i2c_set_brightness(u16 brightness) {
	static u16 pre_brightness = 0x0;

	u8 payload[2] = { brightness >> 4, (brightness << 4) & 0xFF};

	if (pre_brightness == 0 && brightness > 0){
		g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_3d = true;
		zte_gpio_enable_lcd_power_for_leia(LCD_IN_3D_BL_ON);
		pr_info("[MSM_LCD] enable 3D backlight because of first 3d bl = %d\n", brightness);
	}

	//pr_info("MSM_LCD 3D brightness %d payload[0] = %x, payload[1] = %x\n", brightness, payload[0], payload[1]);

	lp8555_3d1_set_cmds(BRTHI, payload[0]);
	lp8555_3d1_set_cmds(BRTLO, payload[1]);
	lp8555_3d2_set_cmds(BRTHI, payload[0]);
	lp8555_3d2_set_cmds(BRTLO, payload[1]);
	lp8555_3d3_set_cmds(BRTHI, payload[0]);
	lp8555_3d3_set_cmds(BRTLO, payload[1]);
	lp8555_3d4_set_cmds(BRTHI, payload[0]);
	lp8555_3d4_set_cmds(BRTLO, payload[1]);

	if (pre_brightness > 0 && brightness == 0){
		g_zte_ctrl_pdata->zte_lcd_ctrl->zte_lcd_3d = false;
		zte_gpio_enable_lcd_power_for_leia(LCD_IN_3D_BL_OFF);
		pr_info("[MSM_LCD] disable 3D backlight because of bl = %d\n", brightness);
	}

	pre_brightness = brightness;
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_3d_bl = brightness;
}

static ssize_t bl_3d_set(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i = 0;
	int brightness = 0;
	int convert_brightness = 0;
	char lcd_status[ZTE_REG_LEN*2] = { 0 };

	if (count >= sizeof(lcd_status)) {
		pr_info("count=%zu,sizeof(lcd_status)=%zu\n", count, sizeof(lcd_status));
		return count;
	}

	sprintf(lcd_status,"%s",buf);

	for (i = 0; i < count; i++) {
		if (isdigit(lcd_status[i]))
			brightness = brightness * 10 + lcd_status[i] - '0';
	}

	convert_brightness = brightness * 1.0 / 4095 * BLMAX;

	pr_info("MSM_LCD 3D brightness from %d into %d\n", brightness, convert_brightness);
    g_zte_ctrl_pdata->zte_lcd_ctrl->zte_3d_bl = convert_brightness;
	i2c_set_brightness(convert_brightness);
	return count;
}

struct kobj_attribute leia_3d_attrs[] = {
    __ATTR(3dbl_set, 0664, NULL, bl_3d_set),
};

void zte_3d_device_register(void)
{
	int ret = -1;
	int attr_count;
	struct kobject *vkey_obj = NULL;

	vkey_obj = kobject_create_and_add(SYSFS_FOLDER_NAME, NULL);/*g_zte_ctrl_pdata->zte_lcd_ctrl->kobj*/
	if (!vkey_obj) {
		pr_err("%s:unable to create kobject\n", __func__);
		return;
	}
	for (attr_count = 0; attr_count < ARRAY_SIZE(leia_3d_attrs); attr_count++) {
		ret = sysfs_create_file(vkey_obj, &leia_3d_attrs[attr_count].attr);
		if (ret < 0) {
			pr_err("failed to create sysfs attributes\n");
		}
	}
}
