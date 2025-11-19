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


#ifndef DEBUG_POLCIY_H
#define DEBUG_POLCIY_H

bool is_kernel_log_driver_enabled(void);
bool is_dm_verity_disabled(void);
bool is_fastboot_enabled(void);
bool is_kernel_log_limit_disabled(void);
bool is_adb_tradefed_enabled(void);

#endif
