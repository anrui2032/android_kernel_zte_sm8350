#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>

/* 2D */
int lp8555_read(u16 offset);
void lp8555_set_cmds(int offset, int data);
int __init lp8555_i2c_driver_init(void);
void __exit lp8555_i2c_driver_exit(void);

/* 3D1 */ 
int lp8555_3d1_read(u16 offset);
void lp8555_3d1_set_cmds(int offset, int data);
int __init lp8555_3d1_i2c_driver_init(void);
void __exit lp8555_3d1_i2c_driver_exit(void);
/* 3D2 */
int lp8555_3d2_read(u16 offset);
void lp8555_3d2_set_cmds(int offset, int data);
int __init lp8555_3d2_i2c_driver_init(void);
void __exit lp8555_3d2_i2c_driver_exit(void);
/* 3D3 */
int lp8555_3d3_read(u16 offset);
void lp8555_3d3_set_cmds(u16 offset, u8 data);
int __init lp8555_3d3_i2c_driver_init(void);
void __exit lp8555_3d3_i2c_driver_exit(void);
/* 3D4 */
int lp8555_3d4_read(u16 offset);
void lp8555_3d4_set_cmds(int offset, int data);
int __init lp8555_3d4_i2c_driver_init(void);
void __exit lp8555_3d4_i2c_driver_exit(void);
/* vsp/vsn */
int __init tps65132b_i2c_driver_init(void);
void __exit tps65132b_i2c_driver_exit(void);
void tps65132b_set_vsp_vsn_level(int level);