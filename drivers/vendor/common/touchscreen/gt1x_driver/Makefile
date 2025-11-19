# Makefile for the gt1x touchscreen drivers.

ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen/gt1x_driver/firmware_config/$(CONFIG_TS_FIRMWARE)/
obj-$(CONFIG_TOUCHSCREEN_GT1X_DRIVER) += gt1x.o
obj-$(CONFIG_TOUCHSCREEN_GT1X_DRIVER) += gt1x_extents.o
obj-$(CONFIG_TOUCHSCREEN_GT1X_DRIVER) += gt1x_generic.o
obj-$(CONFIG_TOUCHSCREEN_GT1X_DRIVER) += gt1x_tools.o
obj-$(CONFIG_TOUCHSCREEN_GT1X_DRIVER) += gt1x_update.o
