# include header files

ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen/ilitek/firmware_config/$(CONFIG_TS_FIRMWARE)/

obj-$(CONFIG_TOUCHSCREEN_ILITEK)	+=  ilitek_main.o
obj-$(CONFIG_TOUCHSCREEN_ILITEK)	+=  ilitek_plat_qcom.o
obj-$(CONFIG_TOUCHSCREEN_ILITEK_I2C)	+=  ilitek_i2c.o
obj-$(CONFIG_TOUCHSCREEN_ILITEK_I2C)	+=  ilitek_flash.o
obj-$(CONFIG_TOUCHSCREEN_ILITEK_SPI)	+=  ilitek_spi.o
obj-$(CONFIG_TOUCHSCREEN_ILITEK_SPI)	+=  ilitek_hostdl.o
obj-$(CONFIG_TOUCHSCREEN_ILITEK)	+=  ilitek_ic.o
obj-$(CONFIG_TOUCHSCREEN_ILITEK)	+=  ilitek_touch.o
obj-$(CONFIG_TOUCHSCREEN_ILITEK)	+=  ilitek_mp.o
obj-$(CONFIG_TOUCHSCREEN_ILITEK)	+=  ilitek_node.o
obj-$(CONFIG_TOUCHSCREEN_ILITEK)	+=  ilitek_common_interface.o
