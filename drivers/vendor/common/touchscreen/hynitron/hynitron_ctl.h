#ifndef __HYNITRON_CTL_H__
#define __HYNITRON_CTL_H__

#include  "hynitron_config.h"
#ifdef HYN_CTL_IIC
#define  HYN_RW_IIC_DRV  "hyn_rw_iic_drv"
#define HYN_RW_IIC_DRV_MAJOR	210    /*预设的hyn_rw_iic_drv的主设备号*/

#define HYN_I2C_RDWR_MAX_QUEUE	36
#define HYN_I2C_SLAVEADDR		11
#define HYN_I2C_RW				12

typedef struct hyn_rw_i2c {
	u8 *buf;
	u8 flag;	/*0-write 1-read*/
	__u16 length; /* the length of data */
} *phyn_rw_i2c;

typedef struct hyn_rw_i2c_queue {
	struct hyn_rw_i2c __user *i2c_queue;
	int queuenum;
} *phyn_rw_i2c_queue;

int hyn_rw_iic_drv_init(struct i2c_client *client);
void  hyn_rw_iic_drv_exit(void);
#endif
#endif
