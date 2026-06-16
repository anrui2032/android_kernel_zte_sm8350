/*
 * Platform driver for reading nubia_hw_version
 *
 * when                  who             what
 * 2021-04-08            wangzy          add cpu_serial_num
 *
 *
 */

/*=========================================
*  Head Files :
* =========================================
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <soc/qcom/socinfo.h>

/*==========================================================================
*  Variables & Defines:
* ==========================================================================
*/
#define SYSFS_PATH_NAME "nubia_hw_version"


/*==========================================================================
*  Functions :
* ==========================================================================
*/
static ssize_t sysfs_show_cpu_serial_num(struct kobject *kobj,
		 struct kobj_attribute *attr, char *buf)
{
	int count = 0;

	count = snprintf(buf, PAGE_SIZE, "%u\n", socinfo_get_serial_number());
	return count;
}

struct kobj_attribute nubia_hw_version_attrs[] = {
    __ATTR(cpu_serial_num, 0444, sysfs_show_cpu_serial_num, NULL),
};

void cpu_serial_num_show_init(void)
{
	int ret = -1;
	int attr_count;

	struct kobject *key_obj = NULL;

	key_obj = kobject_create_and_add(SYSFS_PATH_NAME, NULL);
	if (!key_obj) {
		pr_err("%s: unable to create kobject\n", __func__);
		return;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(nubia_hw_version_attrs); attr_count++) {
		ret = sysfs_create_file(key_obj, &nubia_hw_version_attrs[attr_count].attr);
		if (ret < 0) {
			pr_err("failed to create sysfs attributes\n");
		}
	}
}

static int __init nubia_hw_version_show(void)
{
	cpu_serial_num_show_init();
	return 0;
}

fs_initcall(nubia_hw_version_show);

