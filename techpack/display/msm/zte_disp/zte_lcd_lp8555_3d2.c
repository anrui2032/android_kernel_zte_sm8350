#include "zte_disp_i2c.h"

struct i2c_client *i2c_lp8555_3d2_client = NULL;
bool tilp8555_3d2_probe = false;

int lp8555_3d2_read_reg(struct i2c_client *client, u8 reg, u8 *data)
{
	struct i2c_msg msgs[2];
	int ret;
	u8 retries = 0;

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = 1;
	msgs[0].buf   = &reg;

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = 1;
	msgs[1].buf   = data;

	while (retries < 3) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
		msleep_interruptible(5);
	}
	pr_info("[MSM_LCD] lp8555_3d2 read reg retries=%d,ret=%d\n", retries, ret);
	if (ret != 2) {
		pr_err("lp8555_3d2 read transfer error\n");
		ret = -1;
	}

	return ret;
}
int lp8555_3d2_write_reg(struct i2c_client *client, u8 *buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = len + 1,
			.buf = buf,
		},
	};

	do {
		err = i2c_transfer(client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(5);
	} while ((err != 1) && (++tries < 3));

	if (err != 1) {
		pr_err("[MSM_LCD] lp8555_3d2 write transfer error\n");
		err = -1;
	}

	return err;
}

int lp8555_3d2_read(u16 offset)
{
	u8 data = 0x0;
	if (lp8555_3d2_read_reg(i2c_lp8555_3d2_client, offset, &data)) {
		pr_info("[MSM_LCD] lp8555 3d2 read offset = %x, data = %x\n", offset, data);
	} else {
		pr_info("[MSM_LCD]lp8555 3d2 read fail\n");
	}
	return data;
}
EXPORT_SYMBOL(lp8555_3d2_read);

void lp8555_3d2_set_cmds(int offset, int data)
{
	u8 buf[2] = {0x0, 0x0};

	if (tilp8555_3d2_probe) {
		buf[0] = offset;
		buf[1] = data;
		lp8555_3d2_write_reg(i2c_lp8555_3d2_client, buf, 1);
		usleep_range(5000, 5100);
	}
}
EXPORT_SYMBOL(lp8555_3d2_set_cmds);

static int lp8555_3d2_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("tilp8555_3d2_probe,client not i2c capable\n");
		return -EIO;
	}
	i2c_lp8555_3d2_client = client;

	tilp8555_3d2_probe = true;
	pr_info("[MSM_LCD]lp8555_3d2 tilp8555_3d2_probe ok\n");
	return 0;
}

static int lp8555_3d2_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id lp8555_3d2_id_table[] = {
	{"lp8555_3d2", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, lp8555_3d2_id_table);

static const struct of_device_id lp8555_3d2_of_id_table[] = {
	{.compatible = "ti,lp8555_3d2"},
	{ },
};

static struct i2c_driver lp8555_3d2_i2c_driver = {
	.driver = {
		.name = "lp8555_3d2",
		.owner = THIS_MODULE,
		.of_match_table = lp8555_3d2_of_id_table,
	},
	.probe = lp8555_3d2_probe,
	.remove = lp8555_3d2_remove,
	.id_table = lp8555_3d2_id_table,
};

int __init lp8555_3d2_i2c_driver_init(void)
{
	return i2c_add_driver(&lp8555_3d2_i2c_driver);
}
EXPORT_SYMBOL(lp8555_3d2_i2c_driver_init);

void __exit lp8555_3d2_i2c_driver_exit(void)
{
	i2c_del_driver(&lp8555_3d2_i2c_driver);
}
EXPORT_SYMBOL(lp8555_3d2_i2c_driver_exit);
/*
module_i2c_driver(lp8555_3d2_i2c_driver);

MODULE_DESCRIPTION("lp8555_3d2 chip driver");
MODULE_LICENSE("GPL v2");*/

