# Makefile for the Himax touchscreen drivers.
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen/
ccflags-y += -I$(srctree)/drivers/vendor/common/touchscreen/hxchipset_v2/firmware_config/$(CONFIG_TS_FIRMWARE)/

ifneq ($(filter y, $(CONFIG_KALLSYMS_ALL)),)
	ccflags-y += -D__KERNEL_KALLSYMS_ALL_ENABLED__
endif

ifneq ($(filter y, $(CONFIG_FB)),)
	ccflags-y += -DHX_CONFIG_FB
endif

ifneq ($(filter y, $(CONFIG_DRM)),)
	ccflags-y += -DHX_CONFIG_DRM
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)),)
	himax_mmi-objs += himax_debug.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)),)
	himax_mmi-objs += himax_inspection.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INCELL)),)
	himax_mmi-objs += himax_ic_incell_core.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_ONCELL)),)
	himax_mmi-objs += himax_ic_oncell_core.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xG)),)
  ccflags-y += -D__HIMAX_HX852xG_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX852xG.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX852xG.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xH)),)
  ccflags-y += -D__HIMAX_HX852xH_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX852xH.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX852xH.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102)),)
  ccflags-y += -D__HIMAX_HX83102_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83102.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83102.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83103)),)
  ccflags-y += -D__HIMAX_HX83103_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83103.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83103.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83106)),)
  ccflags-y += -D__HIMAX_HX83106_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83106.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83106.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83111)),)
  ccflags-y += -D__HIMAX_HX83111_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83111.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83111.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112)),)
  ccflags-y += -D__HIMAX_HX83112_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83112.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83112.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83113)),)
  ccflags-y += -D__HIMAX_HX83113_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83113.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83113.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83192)),)
  ccflags-y += -D__HIMAX_HX83192_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83192.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83192.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83191)),)
  ccflags-y += -D__HIMAX_HX83191_MOD__
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83191.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83191.o
  endif
endif

ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
  ccflags-y += -D__HIMAX_MOD__
	himax_mmi-objs += himax_common.o
	himax_mmi-objs += himax_platform.o
ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
	himax_mmi-objs += Himax_firmware.o
endif
	obj-m += himax_mmi.o
endif
ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	himax_mmi-objs += himax_common.o
	ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_SPI)),)
	himax_mmi-objs += himax_platform_SPI.o
	else
	himax_mmi-objs += himax_platform.o
	endif
	himax_mmi-objs += himax_common_interface.o
ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
	himax_mmi-objs += Himax_firmware.o
endif
	obj-y += himax_mmi.o
endif

