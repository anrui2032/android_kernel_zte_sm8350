/* Copyright (c) 2008-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/memblock.h>


#define CMDLINE_SDLOG_MODEM_PARA_PREFIX   "0x"
#define CMDLINE_SDLOG_ADSP_PARA_PREFIX   "0x"


static int sdlog_enable;
static unsigned int sdlog_modem_addr;
static int sdlog_modem_size;

static unsigned int sdlog_adsp_addr;
static int sdlog_adsp_size;


static unsigned int sdlog_sensor_addr;
static int sdlog_sensor_size;


static int __init sdlog_modem_size_setup(char *str)
{
	unsigned long size = 0;

	if (!str)
		return -EINVAL;

	if (kstrtoul(str+strlen(CMDLINE_SDLOG_MODEM_PARA_PREFIX),
			16, &size)) {
		pr_notice("%s can not convert %s in boot_command_line\n", __func__, str);
		return -EINVAL;
	}
	pr_notice("sdlog modem size is 0x%lx\n", size);
	sdlog_modem_size = size;
	return 0;
}

static int __init sdlog_modem_addr_setup(char *str)
{
	unsigned long addr = 0;

	if (!str)
		return -EINVAL;

	if (kstrtoul(str+strlen(CMDLINE_SDLOG_MODEM_PARA_PREFIX),
			16, &addr)) {
		pr_notice("%s can not convert %s in boot_command_line\n", __func__, str);
		return -EINVAL;
	}

	sdlog_modem_addr = addr;
	pr_notice("sdlog modem addr is 0x%lx\n", addr);
	return 0;
}

static int __init sdlog_adsp_size_setup(char *str)
{
	unsigned long size = 0;

	if (!str)
		return -EINVAL;

	if (kstrtoul(str+strlen(CMDLINE_SDLOG_ADSP_PARA_PREFIX),
			16, &size)) {
		pr_notice("%s can not convert %s in boot_command_line\n", __func__, str);
		return -EINVAL;
	}
	pr_notice("sdlog adsp size is 0x%lx\n", size);
	sdlog_adsp_size = size;
	return 0;
}

static int __init sdlog_adsp_addr_setup(char *str)
{
	unsigned long addr = 0;

	if (!str)
		return -EINVAL;

	if (kstrtoul(str+strlen(CMDLINE_SDLOG_ADSP_PARA_PREFIX),
			16, &addr)) {
		pr_notice("%s can not convert %s in boot_command_line\n", __func__, str);
		return -EINVAL;
	}

	sdlog_adsp_addr = addr;
	pr_notice("sdlog adsp addr is 0x%lx\n", addr);
	return 0;
}

static int __init sdlog_sensor_size_setup(char *str)
{
	unsigned long size = 0;

	if (!str)
		return -EINVAL;

	if (kstrtoul(str+strlen(CMDLINE_SDLOG_ADSP_PARA_PREFIX),
			16, &size)) {
		pr_notice("%s can not convert %s in boot_command_line\n", __func__, str);
		return -EINVAL;
	}
	pr_notice("sdlog sensor size is 0x%lx\n", size);
	sdlog_sensor_size = size;
	return 0;
}

static int __init sdlog_sensor_addr_setup(char *str)
{
	unsigned long addr = 0;

	if (!str)
		return -EINVAL;

	if (kstrtoul(str+strlen(CMDLINE_SDLOG_ADSP_PARA_PREFIX),
			16, &addr)) {
		pr_notice("%s can not convert %s in boot_command_line\n", __func__, str);
		return -EINVAL;
	}

	sdlog_sensor_addr = addr;
	pr_notice("sdlog sensor addr is 0x%lx\n", addr);
	return 0;
}


early_param("sdlog.size", sdlog_modem_size_setup);
early_param("sdlog.addr", sdlog_modem_addr_setup);
early_param("adsplog.size", sdlog_adsp_size_setup);
early_param("adsplog.addr", sdlog_adsp_addr_setup);
early_param("sensorlog.size", sdlog_sensor_size_setup);
early_param("sensorlog.addr", sdlog_sensor_addr_setup);


static int __init sdlog_modem_subsystem_memory_reserve(unsigned int addr, unsigned int size)
{
	int ret = 0;

	ret = memblock_reserve(addr, size);
	pr_notice("%s reserve 0x%16lx - 0x%16lx for sdlog (0x%lx byte)\n", __func__,
		(unsigned long)addr,
		(unsigned long)(addr + size),
		(unsigned long)size);
	return ret;
}


static int __init sdlog_subsystem_memory_reserve(unsigned int addr, unsigned int size)
{
	int ret = 0;

	if ((addr == 0) || (size == 0)) {
		pr_notice("sdlog_subsystem_memory_reserve addr or size is 0\n");
		return -EINVAL;
	}

	ret = memblock_reserve(addr, size);
	pr_notice("%s reserve 0x%16lx - 0x%16lx for sdlog (0x%lx byte)\n", __func__,
		(unsigned long)addr,
		(unsigned long)(addr + size),
		(unsigned long)size);
	return ret;
}



static int __init sdlog_adsp_subsystem_memory_reserve(unsigned int addr, unsigned int size)
{
	int ret = 0;

	if ((addr == 0) || (size == 0)) {
		pr_notice("sdlog_adsp_subsystem_memory_reserve addr or size is 0\n");
		return -EINVAL;
	}

	ret = memblock_reserve(addr, size);
	pr_notice("%s reserve 0x%16lx - 0x%16lx for sdlog (0x%lx byte)\n", __func__,
		(unsigned long)addr,
		(unsigned long)(addr + size),
		(unsigned long)size);
	return ret;
}

void __init sdlog_memory_reserve(void)
{
	/*
	* sdlog flag is passed from boot parameter
	* set the flag if sdlog is enabled
	*/
	int ret;

	if ((sdlog_modem_addr != 0) && (sdlog_modem_size != 0)) {
		ret = sdlog_modem_subsystem_memory_reserve(sdlog_modem_addr, sdlog_modem_size);
		if (ret < 0) {
			pr_notice("sdlog_modem_subsystem_memory_reserve failed sdlog_addr %ld, size %ld\n",
				(unsigned long)sdlog_modem_addr, (unsigned long)sdlog_modem_size);
		} else {
			sdlog_enable = 1;
		}
	}

	ret = sdlog_adsp_subsystem_memory_reserve(sdlog_adsp_addr, sdlog_adsp_size);
	if (ret < 0) {
		pr_notice("sdlog_adsp_subsystem_memory_reserve failed adsp_addr %ld, size %ld\n",
			(unsigned long)sdlog_adsp_addr, (unsigned long)sdlog_adsp_size);
	}

	ret = sdlog_subsystem_memory_reserve(sdlog_sensor_addr, sdlog_sensor_size);
	if (ret < 0) {
		pr_notice("sdlog_subsystem_memory_reserve failed adsp_addr %ld, size %ld\n",
			(unsigned long)sdlog_sensor_addr, (unsigned long)sdlog_sensor_size);
	}

}

int sdlog_memory_reserved(void)
{
	pr_notice("sdlog_memory_reserved sdlog_enable %d\n", sdlog_enable);
	return sdlog_enable;
}
EXPORT_SYMBOL(sdlog_memory_reserved);

unsigned int sdlog_memory_get_addr(void)
{
	return sdlog_modem_addr;
}
EXPORT_SYMBOL(sdlog_memory_get_addr);

int sdlog_memory_get_size(void)
{
	return sdlog_modem_size;
}
EXPORT_SYMBOL(sdlog_memory_get_size);

unsigned int sdlog_adsp_memory_get_addr(void)
{
	return sdlog_adsp_addr;
}
EXPORT_SYMBOL(sdlog_adsp_memory_get_addr);

int sdlog_adsp_memory_get_size(void)
{
	return sdlog_adsp_size;
}
EXPORT_SYMBOL(sdlog_adsp_memory_get_size);

