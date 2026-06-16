/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include <linux/errno.h>

#define CMDLINE_DEBUG_POLCIY_PARA_PREFIX   "0x"

#define DEBUG_POLICY_USE_KERNEL_LOG_DRIVER                        0x00000001
#define DEBUG_POLICY_DISABLE_DM_VERITY                            0x00000002
#define DEBUG_POLICY_ENABLE_FASTBOOT                              0x00000004
#define DEBUG_POLICY_DISABLE_KERNEL_LOG_LIMIT                     0x00000008
#define DEBUG_POLICY_ENABLE_ADB_TRADEFED                          0x00000010


static unsigned long debug_policy = 0;

static int __init debug_policy_setup(char *str)
{
	unsigned long policy = 0;

	if (!str)
		return -EINVAL;

	if (kstrtoul(str+strlen(CMDLINE_DEBUG_POLCIY_PARA_PREFIX),
			16, &policy)) {
		pr_notice("%s can not convert %s in boot_command_line\n", __func__, str);
		return -EINVAL;
	}
	pr_notice("debug policy is 0x%lx\n", policy);
	debug_policy = policy;
	return 0;
}


early_param("androidboot.debug.policy", debug_policy_setup);


static int __init build_type_setup(char *str)
{
	if (!str)
		return -EINVAL;

	pr_notice("build_type_setup is %s\n", str);
	return 0;
}

early_param("buildvariant=", build_type_setup);


bool is_kernel_log_driver_enabled(void) {
	return (debug_policy & DEBUG_POLICY_USE_KERNEL_LOG_DRIVER);
}

bool is_dm_verity_disabled(void) {
	return (debug_policy & DEBUG_POLICY_DISABLE_DM_VERITY);
}

bool is_fastboot_enabled(void) {
	return (debug_policy & DEBUG_POLICY_ENABLE_FASTBOOT);
}

bool is_kernel_log_limit_disabled(void) {
	return (debug_policy & DEBUG_POLICY_DISABLE_KERNEL_LOG_LIMIT);
}

bool is_adb_tradefed_enabled(void) {
	return (debug_policy & DEBUG_POLICY_ENABLE_ADB_TRADEFED);
}

bool is_user_build(void) {
  return false;
}
