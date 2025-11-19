#ifndef __HYNITRON_EX_FUN_H__
#define __HYNITRON_EX_FUN_H__

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#ifndef CONFIG_64BIT
/*#include <mach/irqs.h>*/
#endif
#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>


/*****************************************************************************/
#define HYN_PAGE_SIZE				128
#define HYN_PACKET_LENGTH			128
#define HYN_SETTING_BUF_LEN			128
#define HYN_DMA_BUF_SIZE			1024

/*
*hyn_write_reg- write register
*@client: handle of i2c
*@regaddr: register address
*@regvalue: register value
*
*/

int hyn_i2c_Read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen);
int hyn_i2c_Write(struct i2c_client *client, char *writebuf, int writelen);
int hyn_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue);
int hyn_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue);
int ctp_hynitron_update(struct i2c_client *mclient);
int  cst8xx_proc_fs_init(void);
int hyn_create_sysfs(struct i2c_client *client);
void hyn_release_sysfs(struct i2c_client *client);

#endif
