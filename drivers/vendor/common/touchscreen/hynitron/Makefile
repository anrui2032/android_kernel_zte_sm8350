#
# Makefile for the input core drivers.
#
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen/hynitron/firmware_config/$(CONFIG_TS_FIRMWARE)/
# Each configuration option enables a list of files.
obj-$(CONFIG_TOUCHSCREEN_HYNITRON)	 += hynitron.o
obj-$(CONFIG_TOUCHSCREEN_HYNITRON)	 += hynitron_ctl.o
obj-$(CONFIG_TOUCHSCREEN_HYNITRON)	 += hynitron_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_HYNITRON)	 += hynitron_common_interface.o
