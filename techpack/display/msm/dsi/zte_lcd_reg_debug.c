#include "zte_lcd_common.h"

/*
*echo ff988100 > gwrite (0x13,0x29) or echo 51ff > dwrite (0x15,0x39)
*echo 5401 > dread(0x14,0x24), then cat dread
*dread (0x06) sometimes read nothing,return error
*file path: sys/reg_debug
*/

#define ZTE_REG_LEN 64
#define REG_MAX_LEN 16 /*one lcd reg display info max length*/
enum {	/* read or write mode */
	REG_WRITE_MODE = 0,
	REG_READ_MODE
};
struct zte_lcd_reg_debug {
	int is_read_mode;  /*if 1 read ,0 write*/
	unsigned char length;
	char rbuf[ZTE_REG_LEN];
	char wbuf[ZTE_REG_LEN];
#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
	int i2c_index;
#endif
};

#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
enum {
    I2C_2D = 0,
    I2C_3D_1,
    I2C_3D_2,
    I2C_3D_3,
    I2C_3D_4,
    I2C_3D_MAX
};
#endif

/*WARNING: Single statement macros should not use a do {} while (0) loop*/
//#define ZTE_LCD_INFO(fmt, args...) {pr_info("[MSM_LCD][Info]"fmt, ##args); }
struct zte_lcd_reg_debug zte_lcd_reg_debug;
extern struct dsi_panel *g_zte_ctrl_pdata;
#define SYSFS_FOLDER_NAME "reg_debug"

#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
/* note: return value <= 0 is error; > 0 is success */
extern int dsi_panel_read_cmd_set(struct dsi_panel *panel, struct dsi_read_config *read_config);
ssize_t zte_dual_dsi_reg_dread(struct zte_lcd_reg_debug *reg_debug)
{
	int rc = 0;
	int i = 0;
	u8 *tx_buf;
	struct dsi_read_config ld_read_config;

	while(!g_zte_ctrl_pdata->cur_mode || !g_zte_ctrl_pdata->cur_mode->priv_info || !g_zte_ctrl_pdata->panel_initialized) {
		pr_debug("[%s][%s] waitting for panel priv_info initialized!\n", __func__, g_zte_ctrl_pdata->name);
		msleep_interruptible(1000);
	}

	mutex_lock(&g_zte_ctrl_pdata->panel_lock);
	if (g_zte_ctrl_pdata->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_REG_READ].cmds) {
		ld_read_config.is_read = 1;
		ld_read_config.cmds_rlen = reg_debug->wbuf[1];
		ld_read_config.read_cmd.cmds = g_zte_ctrl_pdata->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_REG_READ].cmds;
		ld_read_config.read_cmd.count = 1;
		ld_read_config.read_cmd.state = g_zte_ctrl_pdata->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_REG_READ].state;
		tx_buf = (u8 *)ld_read_config.read_cmd.cmds[0].msg.tx_buf;
		tx_buf[0] = reg_debug->wbuf[0];
		pr_info("MSM_LCD read reg addr 0x%02x, length = %d", tx_buf[0], ld_read_config.cmds_rlen);
		rc = dsi_panel_read_cmd_set(g_zte_ctrl_pdata, &ld_read_config);
		if (rc <= 0) {
			pr_err("[%s][%s] failed to read cmds, rc=%d\n", __func__, g_zte_ctrl_pdata->name, rc);
			rc = -EIO;
			goto done;
		}

		for(i = 0; i < reg_debug->length; i++) {
			pr_info("[%s][%d]0x%02x", __func__, __LINE__, ld_read_config.rbuf[i]);
			reg_debug->rbuf[i] = ld_read_config.rbuf[i];
		}
	}
done:
	mutex_unlock(&g_zte_ctrl_pdata->panel_lock);
	return rc;
}

static void zte_lcd_i2c_rw_func(struct zte_lcd_reg_debug *i2c_debug)
{

	int i;
    bool read_mode = false;
    int i2c_index = 0;
	int data = 0x0;

	if (!i2c_debug)
		return;

    read_mode = i2c_debug->is_read_mode == REG_READ_MODE ? true : false;
    i2c_index = i2c_debug->i2c_index;

	for (i = 0; i < i2c_debug->length; i++)
		pr_info("wbuf[%d]= %x\n", i, i2c_debug->wbuf[i]);

    if (i2c_debug->is_read_mode == REG_READ_MODE) {
        switch(i2c_index) {
            case I2C_2D:
                data = lp8555_read(i2c_debug->wbuf[0]);
                break;
            case I2C_3D_1:
                data = lp8555_3d1_read(i2c_debug->wbuf[0]);
                break;
            case I2C_3D_2:
                data = lp8555_3d2_read(i2c_debug->wbuf[0]);
                break;
            case I2C_3D_3:
                data = lp8555_3d3_read(i2c_debug->wbuf[0]);
                break;
            case I2C_3D_4:
                data = lp8555_3d4_read(i2c_debug->wbuf[0]);
                break;
        }
		i2c_debug->rbuf[0] = data;
    } else {
        switch(i2c_index) {
            case I2C_2D:
                lp8555_set_cmds(i2c_debug->wbuf[0],i2c_debug->wbuf[1]);
                break;
            case I2C_3D_1:
                lp8555_3d1_set_cmds(i2c_debug->wbuf[0],i2c_debug->wbuf[1]);
                break;
            case I2C_3D_2:
                lp8555_3d2_set_cmds(i2c_debug->wbuf[0],i2c_debug->wbuf[1]);
                break;
            case I2C_3D_3:
                lp8555_3d3_set_cmds(i2c_debug->wbuf[0],i2c_debug->wbuf[1]);
                break;
            case I2C_3D_4:
                lp8555_3d4_set_cmds(i2c_debug->wbuf[0],i2c_debug->wbuf[1]);
                break;
        }
    }
}
#endif

static void zte_lcd_reg_rw_func(struct dsi_panel *ctrl, struct zte_lcd_reg_debug *reg_debug)
{

	int i;
	struct mipi_dsi_device *dsi;

	if ((!reg_debug) || (!ctrl))
		return;

	dsi = &ctrl->mipi_device;

	/*if debug this func,define ZTE_LCD_REG_DEBUG 1*/
	for (i = 0; i < reg_debug->length; i++)
		pr_info("wbuf[%d]= %x\n", i, reg_debug->wbuf[i]);

	if (!g_zte_ctrl_pdata->panel_initialized) {
		pr_err("MSM_LCD panel is off or not initialized reg read write\n");
		return;
	}

	switch (reg_debug->is_read_mode) {
	case REG_READ_MODE:
		#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
			zte_dual_dsi_reg_dread(reg_debug);
		#else
			mipi_dsi_dcs_read(dsi, reg_debug->wbuf[0], &reg_debug->rbuf[0], reg_debug->wbuf[1]);
		#endif
		for (i = 0; i < reg_debug->wbuf[1]; i++)
			pr_info("dcs0 rbuf[%d]= %x\n", i, reg_debug->rbuf[i]);
		break;
	case REG_WRITE_MODE:
		mipi_dsi_dcs_write(dsi, reg_debug->wbuf[0], &reg_debug->wbuf[1], reg_debug->length - 1);
		break;
	default:
		pr_err("%s:rw error\n", __func__);
		break;
	}
}

static void get_user_sapce_data(const char *buf, size_t count)
{
	int i = 0, length = 0;
	char lcd_status[ZTE_REG_LEN*2] = { 0 };

	if (count >= sizeof(lcd_status)) {
		pr_info("count=%zu,sizeof(lcd_status)=%zu\n", count, sizeof(lcd_status));
		return;
	}

	strlcpy(lcd_status, buf, count);
	memset(zte_lcd_reg_debug.wbuf, 0, ZTE_REG_LEN);
	memset(zte_lcd_reg_debug.rbuf, 0, ZTE_REG_LEN);

	/*if debug this func,define ZTE_LCD_REG_DEBUG 1*/
	#ifdef ZTE_LCD_REG_DEBUG
	for (i = 0; i < count; i++)
		pr_info("lcd_status[%d]=%c  %d\n", i, lcd_status[i], lcd_status[i]);
	#endif
	for (i = 0; i < count; i++) {
		if (isdigit(lcd_status[i]))
			lcd_status[i] -= '0';
		else if (isalpha(lcd_status[i]))
			lcd_status[i] -= (isupper(lcd_status[i]) ? 'A' - 10 : 'a' - 10);
	}
	for (i = 0, length = 0; i < (count-1); i = i+2, length++) {
		zte_lcd_reg_debug.wbuf[length] = lcd_status[i]*16 + lcd_status[1+i];
	}

	zte_lcd_reg_debug.length = length; /*length is use space write data number*/
}

static ssize_t sysfs_show_read(struct kobject *kobj,
		 struct kobj_attribute *attr, char *buf)
{
	int i = 0, len = 0, count = 0;
	char *s = NULL;
	char *data_buf = NULL;

	data_buf = kzalloc(ZTE_REG_LEN * REG_MAX_LEN, GFP_KERNEL);
	if (!data_buf)
		return -ENOMEM;

	s = data_buf;
	pr_info("MSM_LCD read len = %d\n",zte_lcd_reg_debug.length);
	for (i = 0; i < zte_lcd_reg_debug.length; i++) {
		len = snprintf(s, 20, "rbuf[%02d]=%02x ", i, zte_lcd_reg_debug.rbuf[i]);
		s += len;
		if ((i+1)%8 == 0) {
			len = snprintf(s, 20, "\n");
			s += len;
		}
	}

	count = snprintf(buf, PAGE_SIZE, "read back:\n%s\n", data_buf);
	kfree(data_buf);
	return count;
}
static ssize_t sysfs_store_dread(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i = 0, length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.wbuf[1];
	if (length < 1) {
		pr_err("%s:read length is 0\n", __func__);
		return count;
	}

	zte_lcd_reg_debug.is_read_mode = REG_READ_MODE;
	pr_info("read cmd = %x length = %x\n", zte_lcd_reg_debug.wbuf[0], length);
	zte_lcd_reg_rw_func(g_zte_ctrl_pdata, &zte_lcd_reg_debug);

	zte_lcd_reg_debug.length = length;
	for (i = 0; i < length; i++)
		pr_info("read zte_lcd_reg_debug.rbuf[%d]=0x%02x\n", i, zte_lcd_reg_debug.rbuf[i]);

	return count;
}

static ssize_t sysfs_store_dwrite(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.length;

	zte_lcd_reg_debug.is_read_mode = REG_WRITE_MODE; /* if 1 read ,0 write*/
	zte_lcd_reg_rw_func(g_zte_ctrl_pdata, &zte_lcd_reg_debug);
	pr_info("write cmd = 0x%02x,length = 0x%02x\n", zte_lcd_reg_debug.wbuf[0], length);

	return count;
}

#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
/*********************add 2d i2c rw function begin***********************/
static ssize_t sysfs_store_dread2d(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i = 0, length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.wbuf[1];
	if (length < 1) {
		pr_err("%s:read length is 0\n", __func__);
		return count;
	}

	zte_lcd_reg_debug.is_read_mode = REG_READ_MODE;
    zte_lcd_reg_debug.i2c_index = I2C_2D;
	zte_lcd_i2c_rw_func(&zte_lcd_reg_debug);

	zte_lcd_reg_debug.length = length;
	for (i = 0; i < length; i++)
		pr_info("MSM_LCD read zte_lcd_reg_debug.rbuf[%d]=0x%02x\n", i, zte_lcd_reg_debug.rbuf[i]);

	return count;
}

static ssize_t sysfs_store_dwrite2d(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.length;

	zte_lcd_reg_debug.is_read_mode = REG_WRITE_MODE; /* if 1 read ,0 write*/
    zte_lcd_reg_debug.i2c_index = I2C_2D;
	zte_lcd_i2c_rw_func(&zte_lcd_reg_debug);
	pr_info("MSM_LCD write cmd = 0x%02x,length = 0x%02x\n", zte_lcd_reg_debug.wbuf[0], length);

	return count;
}
/*********************add 2d i2c rw function end***********************/

/*********************add 3d1 i2c rw function begin***********************/
static ssize_t sysfs_store_dread3d1(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i = 0, length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.wbuf[1];
	if (length < 1) {
		pr_err("%s:read length is 0\n", __func__);
		return count;
	}

	zte_lcd_reg_debug.is_read_mode = REG_READ_MODE;
    zte_lcd_reg_debug.i2c_index = I2C_3D_1;
	zte_lcd_i2c_rw_func(&zte_lcd_reg_debug);

	zte_lcd_reg_debug.length = length;
	for (i = 0; i < length; i++)
		pr_info("MSM_LCD read zte_lcd_reg_debug.rbuf[%d]=0x%02x\n", i, zte_lcd_reg_debug.rbuf[i]);

	return count;
}

static ssize_t sysfs_store_dwrite3d1(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.length;

	zte_lcd_reg_debug.is_read_mode = REG_WRITE_MODE; /* if 1 read ,0 write*/
    zte_lcd_reg_debug.i2c_index = I2C_3D_1;
	zte_lcd_i2c_rw_func(&zte_lcd_reg_debug);
	pr_info("MSM_LCD write cmd = 0x%02x,length = 0x%02x\n", zte_lcd_reg_debug.wbuf[0], length);

	return count;
}
/*********************add 3d1 i2c rw function end***********************/

/*********************add 3d2 i2c rw function begin***********************/
static ssize_t sysfs_store_dread3d2(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i = 0, length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.wbuf[1];
	if (length < 1) {
		pr_err("%s:read length is 0\n", __func__);
		return count;
	}

	zte_lcd_reg_debug.is_read_mode = REG_READ_MODE;
    zte_lcd_reg_debug.i2c_index = I2C_3D_2;
	zte_lcd_i2c_rw_func(&zte_lcd_reg_debug);

	zte_lcd_reg_debug.length = length;
	for (i = 0; i < length; i++)
		pr_info("MSM_LCD read zte_lcd_reg_debug.rbuf[%d]=0x%02x\n", i, zte_lcd_reg_debug.rbuf[i]);

	return count;
}

static ssize_t sysfs_store_dwrite3d2(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.length;

	zte_lcd_reg_debug.is_read_mode = REG_WRITE_MODE; /* if 1 read ,0 write*/
    zte_lcd_reg_debug.i2c_index = I2C_3D_2;
	zte_lcd_i2c_rw_func(&zte_lcd_reg_debug);
	pr_info("MSM_LCD write cmd = 0x%02x,length = 0x%02x\n", zte_lcd_reg_debug.wbuf[0], length);

	return count;
}
/*********************add 3d1 i2c rw function end***********************/

/*********************add 3d3 i2c rw function begin***********************/
static ssize_t sysfs_store_dread3d3(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i = 0, length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.wbuf[1];
	if (length < 1) {
		pr_err("%s:read length is 0\n", __func__);
		return count;
	}

	zte_lcd_reg_debug.is_read_mode = REG_READ_MODE;
    zte_lcd_reg_debug.i2c_index = I2C_3D_3;
	zte_lcd_i2c_rw_func(&zte_lcd_reg_debug);

	zte_lcd_reg_debug.length = length;
	for (i = 0; i < length; i++)
		pr_info("MSM_LCD read zte_lcd_reg_debug.rbuf[%d]=0x%02x\n", i, zte_lcd_reg_debug.rbuf[i]);

	return count;
}

static ssize_t sysfs_store_dwrite3d3(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.length;

	zte_lcd_reg_debug.is_read_mode = REG_WRITE_MODE; /* if 1 read ,0 write*/
    zte_lcd_reg_debug.i2c_index = I2C_3D_3;
	zte_lcd_i2c_rw_func(&zte_lcd_reg_debug);
	pr_info("MSM_LCD write cmd = 0x%02x,length = 0x%02x\n", zte_lcd_reg_debug.wbuf[0], length);

	return count;
}
/*********************add 3d3 i2c rw function end***********************/

/*********************add 3d4 i2c rw function begin***********************/
static ssize_t sysfs_store_dread3d4(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i = 0, length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.wbuf[1];
	if (length < 1) {
		pr_err("%s:read length is 0\n", __func__);
		return count;
	}

	zte_lcd_reg_debug.is_read_mode = REG_READ_MODE;
    zte_lcd_reg_debug.i2c_index = I2C_3D_4;
	zte_lcd_i2c_rw_func(&zte_lcd_reg_debug);

	zte_lcd_reg_debug.length = length;
	for (i = 0; i < length; i++)
		pr_info("MSM_LCD read zte_lcd_reg_debug.rbuf[%d]=0x%02x\n", i, zte_lcd_reg_debug.rbuf[i]);

	return count;
}

static ssize_t sysfs_store_dwrite3d4(struct kobject *kobj,
		 struct kobj_attribute *attr, const char *buf, size_t count)
{
	int length = 0;

	get_user_sapce_data(buf, count);
	length = zte_lcd_reg_debug.length;

	zte_lcd_reg_debug.is_read_mode = REG_WRITE_MODE; /* if 1 read ,0 write*/
    zte_lcd_reg_debug.i2c_index = I2C_3D_4;
	zte_lcd_i2c_rw_func(&zte_lcd_reg_debug);
	pr_info("MSM_LCD write cmd = 0x%02x,length = 0x%02x\n", zte_lcd_reg_debug.wbuf[0], length);

	return count;
}
/*********************add 3d4 i2c rw function end***********************/
#endif

struct kobj_attribute lcd_debug_attrs[] = {
    __ATTR(dread, 0664, sysfs_show_read, sysfs_store_dread),
    __ATTR(dwrite, 0664, NULL, sysfs_store_dwrite),
#ifdef CONFIG_ZTE_LCD_LEIA_EN_GPIO
    __ATTR(dread2D, 0664, sysfs_show_read, sysfs_store_dread2d),
    __ATTR(dwrite2D, 0664, NULL, sysfs_store_dwrite2d),
    __ATTR(dread3D1, 0664, sysfs_show_read, sysfs_store_dread3d1),
    __ATTR(dwrite3D1, 0664, NULL, sysfs_store_dwrite3d1),
    __ATTR(dread3D2, 0664, sysfs_show_read, sysfs_store_dread3d2),
    __ATTR(dwrite3D2, 0664, NULL, sysfs_store_dwrite3d2),
    __ATTR(dread3D3, 0664, sysfs_show_read, sysfs_store_dread3d3),
    __ATTR(dwrite3D3, 0664, NULL, sysfs_store_dwrite3d3),
    __ATTR(dread3D4, 0664, sysfs_show_read, sysfs_store_dread3d4),
    __ATTR(dwrite3D4, 0664, NULL, sysfs_store_dwrite3d4),
#endif
};

void zte_lcd_reg_debug_func(void)
{
	int ret = -1;
	int attr_count;
	struct kobject *vkey_obj = NULL;

	vkey_obj = kobject_create_and_add(SYSFS_FOLDER_NAME, NULL);/*g_zte_ctrl_pdata->zte_lcd_ctrl->kobj*/
	if (!vkey_obj) {
		pr_err("%s:unable to create kobject\n", __func__);
		return;
	}
	for (attr_count = 0; attr_count < ARRAY_SIZE(lcd_debug_attrs); attr_count++) {
		ret = sysfs_create_file(vkey_obj, &lcd_debug_attrs[attr_count].attr);
		if (ret < 0) {
			pr_err("failed to create sysfs attributes\n");
			}
	}
}
