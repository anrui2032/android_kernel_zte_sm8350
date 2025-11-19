# When adding new entries keep the list in alphabetical order
CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)

obj-$(CONFIG_VENDOR_SDLOG)				+= f3mem_dump.o
obj-$(CONFIG_VENDOR_SDLOG)				+= sdlog_mem_reserve.o
obj-$(CONFIG_VENDOR_SDLOG)				+= msm_tb.o