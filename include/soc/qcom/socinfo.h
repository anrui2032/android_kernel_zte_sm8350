/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SOC_QCOM_SOCINFO_H__
#define __SOC_QCOM_SOCINFO_H__

#include <linux/types.h>

#if IS_ENABLED(CONFIG_QCOM_SOCINFO)
uint32_t socinfo_get_id(void);
uint32_t socinfo_get_serial_number(void);
const char *socinfo_get_id_string(void);
#else
static inline uint32_t socinfo_get_id(void)
{
	return 0;
}

static inline uint32_t socinfo_get_serial_number(void)
{
	return 0;
}

static inline const char *socinfo_get_id_string(void)
{
	return "N/A";
}
#endif /* CONFIG_QCOM_SOCINFO */

/* ZTE ADD for BOOT_MODE start */
#ifdef CONFIG_ZTE_BOOT_MODE
#define ANDROID_BOOT_MODE              "androidboot.mode="
#define ANDROID_BOOT_MODE_FTM          "ffbm-99"
#define ANDROID_BOOT_MODE_FFBM         "ffbm-00"
#define ANDROID_BOOT_MODE_RECOVERY	"recovery"
#define ANDROID_BOOT_MODE_CHARGER      "charger"

#define MAGIC_NUM_FFBM_MODE          0x6D6D5446 /*FFBM*/
#define MAGIC_NUM_NON_FFBM_MODE      0x4D54464E /*NFFBM*/

/*
 * Boot mode definition
 */
enum {
	ENUM_BOOT_MODE_NORMAL            = 0,
	ENUM_BOOT_MODE_FTM               = 1,
	ENUM_BOOT_MODE_RTC_ALARM         = 2,
	ENUM_BOOT_MODE_CHARGER           = 3,
	ENUM_BOOT_MODE_RECOVERY          = 4,
	ENUM_BOOT_MODE_FFBM              = 5,
	ENUM_BOOT_MODE_UNKNOWN,
	ENUM_BOOT_MODE_MAX
};

extern void socinfo_set_boot_mode(int boot_mode);
extern int socinfo_get_ftm_flag(void);
extern int socinfo_get_ffbm_flag(void);
extern int socinfo_get_charger_flag(void);
extern int zte_get_boot_mode(void);
#endif
/* ZTE ADD for BOOT_MODE end */

#endif /* __SOC_QCOM_SOCINFO_H__ */
