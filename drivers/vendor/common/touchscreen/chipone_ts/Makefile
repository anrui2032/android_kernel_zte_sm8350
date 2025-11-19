# Makefile for the chipone touchscreen drivers.

# Each configuration option enables a list of files.
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen/chipone_ts/firmware_config/$(CONFIG_TS_FIRMWARE)/

obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += cts_i2c_driver.o
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += cts_core.o
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += cts_platform.o
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += cts_firmware.o
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += icnt8xxx_flash.o
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += cts_tool.o
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += cts_sysfs.o
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += cts_selftest.o
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += cts_selftest_ini.o
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += cts_test.o
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_TS) += cts_common_interface.o
