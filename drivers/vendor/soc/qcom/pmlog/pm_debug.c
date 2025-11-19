/*
 *This program is used for recode the ap and modem's sleep and wake time.
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/tick.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/pm.h>
#include <soc/qcom/qseecom_scm.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/system_misc.h>
#include <soc/qcom/socinfo.h>
#include <linux/seq_file.h>
#include <linux/fb.h>
#include <linux/version.h>
#include <linux/syscore_ops.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 0)
#include "../../../../clk/msm/clock.h"
#endif

#if LINUX_VERSION_CODE>= KERNEL_VERSION(4,9,106)
#define MODULE_PARAM_V2
#endif

static uint32_t showmodemsleep = 0;
static uint32_t showmodemawake = 0;
static uint32_t showmodemsleeporawake = 0;
static uint32_t showphyslinktime = 0;
static int msm_pm_debug_mask = 1;
module_param_named(
	debug_mask, msm_pm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

/*ZTE_PM ++++ Interface For Ril Open F3 Log*/
static int apSleep_modemAwake_timeThreshold = 10;
static int apSleep_modemAwake_precent = 900;
static int apSleep_modemAwake_count = 5;
module_param_named(zte_amss_time_threshold, apSleep_modemAwake_timeThreshold, int, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(zte_amss_awake_precent, apSleep_modemAwake_precent, int, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(zte_amss_awake_acount, apSleep_modemAwake_count, int, S_IRUGO | S_IWUSR | S_IWGRP);
static int zte_amss_acount;
static struct device  *msm_cpu_pm_dev;


enum {
	MSM_PM_DEBUG_ZTE_IDLE_CLOCK = BIT(10),/* LOG default not open*/
};

typedef struct {
	uint32_t app_suspend_state;
	uint32_t modemsleeptime;
	uint32_t modemawaketime;
	uint32_t modemsleep_or_awake;/*1 sleep,2 awake,0 never enter sleep*/
	uint32_t physlinktime;
	uint32_t modemawake_timeout_crash;
} pm_count_time;

static pm_count_time *zte_imem_ptr = NULL;

static int kernel_sleep_count;

/*ZTE LCD ++++ */
/*screenontime record each on -off time*/
static long screenontime = 0;
static long screenontimebeforesuspend = 0;
static bool screenofffirstime = true;
static void update_screenon_time(bool lcdonoff)
{
	/*pr_info("[PM_V] turn LCD %s %s\n", lcdonoff ? "ON" : "OFF", screenofffirstime ? " first time":" ");*/
	if (screenofffirstime) {
		if (!lcdonoff)
			screenofffirstime = false;
		return;
	}
	if (lcdonoff) {
		screenontime = current_kernel_time().tv_sec;
	} else {
		screenontime = current_kernel_time().tv_sec - screenontime;
		screenontimebeforesuspend += screenontime;
	}
}

static int lcd_fb_callback(struct notifier_block *nfb,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		/*pr_info("[PM_V] %s enter , blank=%d\n", __func__, *blank);*/

		if (*blank == FB_BLANK_UNBLANK) {
			/*notes:update resume time,
			indicate the LCD will turn on.*/
			update_screenon_time(true);
		} else if ((*blank == FB_BLANK_POWERDOWN) || (*blank == FB_BLANK_NORMAL)) {
			/*notes:update suspend time,
			indicate the LCD will turn off.*/
			update_screenon_time(false);
		}
	}
	return 0;
}

static struct notifier_block __refdata lcd_fb_notifier = {
	.notifier_call = lcd_fb_callback,
};
/*ZTE LCD ---- */

/* This value control the lines of output logs */
static int glink_count = 0;
bool can_glink_output(void)
{
	if (glink_count > 0) {
		glink_count = glink_count - 1;
		return true;
	} else {
		return false;
	}
}

/* This value control the lines of output logs */
static int qrtr_count = 0;
bool can_qrtr_output(void)
{
	if (qrtr_count > 0) {
		qrtr_count = qrtr_count - 1;
		return true;
	} else {
		return false;
	}
}
EXPORT_SYMBOL(can_qrtr_output);

static void output_count_set(void) {
	glink_count = 3;
	qrtr_count = 3;
}

static void output_count_clear(void) {
	glink_count = 0;
	qrtr_count = 0;
}


#define MSM_PM_DPRINTK(mask, level, message, ...) \
	do { \
		if ((mask) & msm_pm_debug_mask) \
			printk(level message, ## __VA_ARGS__); \
	} while (0)

static unsigned pm_modem_sleep_time_get(void)
{
	if (!zte_imem_ptr) {
		pr_err("zte_imem_ptr is null,pm_modem_sleep_time_get return\n");
		return 0;
	}
	pr_info("[PM_V] get modemsleeptime %d\n", zte_imem_ptr->modemsleeptime);
	return zte_imem_ptr->modemsleeptime;
}

static unsigned pm_modem_phys_link_time_get(void)
{
	if (!zte_imem_ptr) {
		pr_err("zte_imem_ptr is null,pm_modem_phys_link_time_get return\n");
		return 0;
	}
	pr_info("[PM_V] get physlinktime %d\n", zte_imem_ptr->physlinktime);
	return zte_imem_ptr->physlinktime;
}

static unsigned pm_modem_awake_time_get(int *current_sleep)
{
	if (!zte_imem_ptr) {
		pr_err("zte_imem_ptr is null,pm_modem_awake_time_get return\n");
		return 0;
	}
	*current_sleep =  zte_imem_ptr->modemsleep_or_awake;
	pr_info("[PM_V] get modemawaketime %d,current_sleep=%d\n", zte_imem_ptr->modemawaketime, *current_sleep);
	return zte_imem_ptr->modemawaketime;
}
#ifdef MODULE_PARAM_V2
static int pm_modem_sleep_time_show(char *buffer, const struct kernel_param *kp)
#else
static int pm_modem_sleep_time_show(char *buffer, struct kernel_param *kp)
#endif
{
	if (!zte_imem_ptr) {
		pr_err("zte_imem_ptr is null,pm_modem_sleep_time_get return\n");
		return 0;
	}
	pr_info("[PM_V] get modemsleeptime %d\n", zte_imem_ptr->modemsleeptime);
	return  snprintf(buffer, 8,  "%d", (zte_imem_ptr->modemsleeptime / 1000));
}
#ifdef MODULE_PARAM_V2
static int pm_modem_awake_time_show(char *buffer, const struct kernel_param *kp)
#else
static int pm_modem_awake_time_show(char *buffer, struct kernel_param *kp)
#endif
{
	if (!zte_imem_ptr) {
		pr_err("zte_imem_ptr is null,modemawaketime return\n");
		return 0;
	}
	pr_info("[PM_V] get modemawaketime %d\n", zte_imem_ptr->modemawaketime);
	return  snprintf(buffer, 8, "%d", (zte_imem_ptr->modemawaketime / 1000));
}
#ifdef MODULE_PARAM_V2
static int pm_modem_sleep_or_awake_show(char *buffer, const struct kernel_param *kp)
#else
static int pm_modem_sleep_or_awake_show(char *buffer, struct kernel_param *kp)
#endif
{
	if (!zte_imem_ptr) {
		pr_err("zte_imem_ptr is null,pm_modem_sleep_or_awake_get return\n");
		return 0;
	}
	pr_info("[PM_V] get modemsleep_or_awake %d,\n", zte_imem_ptr->modemsleep_or_awake);
	return  snprintf(buffer, 8, "%d", zte_imem_ptr->modemsleep_or_awake);
}

#ifdef MODULE_PARAM_V2
static int pm_modem_phys_link_time_show(char *buffer, const struct kernel_param *kp)
#else
static int pm_modem_phys_link_time_show(char *buffer, struct kernel_param *kp)
#endif
{
	if (!zte_imem_ptr) {
		pr_err("zte_imem_ptr is null,pm_modem_phys_link_time_get return\n");
		return 0;
	}
	pr_info("[PM_V] get physlinktime %d\n", zte_imem_ptr->physlinktime);
	return  snprintf(buffer, 8, "%d", zte_imem_ptr->physlinktime);
}


module_param_call(showmodemsleep, NULL, pm_modem_sleep_time_show,
						&showmodemsleep, 0644);
module_param_call(showmodemawake, NULL, pm_modem_awake_time_show,
						&showmodemawake, 0644);
module_param_call(showmodemsleeporawake, NULL, pm_modem_sleep_or_awake_show,
						&showmodemsleeporawake, 0644);
module_param_call(showphyslinktime, NULL, pm_modem_phys_link_time_show,
						&showphyslinktime, 0644);


/*ZTE_PM ++++ */

/*Interface For Ril Open F3 Log*/
static int zte_amss_invalid_parameter(void)
{
	if (apSleep_modemAwake_count <= 0)
		return 0;
	else if (apSleep_modemAwake_precent < 500 || apSleep_modemAwake_precent > 1000)
		return 0;
	else if (apSleep_modemAwake_timeThreshold <= 0)
		return 0;
	else
		return 1;
}

static  void  zte_amss_updateEvent(int modemState)
{
	char *event = NULL;
	char *envp[2];
	const char *name;

	if (msm_cpu_pm_dev == NULL) {
		pr_info("amss, msm_cpu_pm_dev is NULL");
	} else {
		name = modemState?"OPEN":"CLOSE";
		event = kasprintf(GFP_KERNEL, "AMSS_PM_STATE=%s", name);
		envp[0] = event;
		envp[1] = NULL;
		kobject_uevent_env(&msm_cpu_pm_dev->kobj, KOBJ_CHANGE, envp);
		kfree(event);
	}
}

static int  zte_amss_needF3log(int apSleep_time_s, int modemAwake_percent)
{
	int invalidParameter = 0;

	invalidParameter = zte_amss_invalid_parameter();

	if (apSleep_time_s < 0 || (modemAwake_percent < 900 || modemAwake_percent > 1000))
		return 0;
	if (invalidParameter == 0)
		return 0;

	if (zte_amss_acount > apSleep_modemAwake_count)
		zte_amss_acount = 0;

	zte_amss_acount = zte_amss_acount + 1;
	if ((zte_amss_acount == apSleep_modemAwake_count) && invalidParameter == 1)
		return 1;
	else
		return 0;
}

#define AMSS_NEVER_ENTER_SLEEP 0x4
#define AMSS_NOW_SLEEP 0x0
#define AMSS_NOW_AWAKE 0x1
#define THRESOLD_FOR_OFFLINE_AWAKE_TIME 100 /*ms*/
#define THRESOLD_FOR_OFFLINE_TIME 5000 /*s*/

static int mEnableRrecordFlag_ZTE = 0;
module_param_named(zte_enableRecord,
	mEnableRrecordFlag_ZTE, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define RECORED_TOTAL_TIME
static struct timespec time_updated_when_sleep_awake;

/*ZTE_PM: called after exit PowerCollapse from suspend,
which will inform modem app has exit suspend.*/
static void inform_modem_app_enter_suspend(bool entersuspend)
{
	if (zte_imem_ptr) {
		if (entersuspend) {/*true?sleep:resume*/
			zte_imem_ptr->app_suspend_state = 0xAA;
		} else {
			zte_imem_ptr->app_suspend_state = 0;
			pr_info("PM notify app resume\n");
		}
	}
}

/*enter_sleep?record how long app keep sleep for: record how long app awake for.*/
static void record_sleep_awake_time_vendor(bool enter_sleep)
{
	struct timespec ts;
	unsigned time_updated_when_sleep_awake_s;
#ifdef RECORED_TOTAL_TIME
	static bool record_firsttime = true;
	static bool record_firsttime_modem = true;
	static	unsigned time_modem_firsttime_awake_s;
	static	unsigned time_modem_firsttime_sleep_s;
	static	unsigned time_app_total_awake_s;
	static	unsigned time_app_total_sleep_s;
	static unsigned time_lcdon_total_s;
#endif
	unsigned time_updated_when_sleep_awake_ms;
	unsigned time_updated_when_sleep_awake_ms_temp;
	static unsigned amss_sleep_time_ms = 0;
	static unsigned amss_physlink_current_total_time_s;
	static unsigned amss_physlink_last_total_time_s;
	unsigned amss_sleep_time_ms_temp = 0;
	unsigned deta_sleep_ms = 0;
	unsigned deta_awake_ms = 0;
	unsigned deta_physlink_s = 0;
	unsigned amss_awake_last = 0;
	int result_state = 0;

	unsigned amss_current_sleep_or_awake = 0;/*1 never enter sleep,2 sleep,3 awake*/
	static unsigned  amss_current_sleep_or_awake_previous;

	static unsigned amss_awake_time_ms;
	unsigned amss_awake_time_ms_temp = 0;
	bool get_amss_awake_ok = false;

	unsigned percentage_amss_not_sleep_while_app_suspend = 0;
	static bool sleep_success_flag;

	ts = current_kernel_time();

	time_updated_when_sleep_awake_ms_temp =	(unsigned) ((ts.tv_sec - time_updated_when_sleep_awake.tv_sec) *
		MSEC_PER_SEC + ((ts.tv_nsec / NSEC_PER_MSEC) -
		(time_updated_when_sleep_awake.tv_nsec / NSEC_PER_MSEC)));
	time_updated_when_sleep_awake_s = (time_updated_when_sleep_awake_ms_temp/MSEC_PER_SEC);
	time_updated_when_sleep_awake_ms = (time_updated_when_sleep_awake_ms_temp -
		time_updated_when_sleep_awake_s * MSEC_PER_SEC);

	/*ZTE:record app awake time before enter sleep*/
	if (!enter_sleep) {
		sleep_success_flag = true;
		amss_sleep_time_ms_temp = amss_sleep_time_ms;
		amss_sleep_time_ms = pm_modem_sleep_time_get();
		deta_sleep_ms = amss_sleep_time_ms - amss_sleep_time_ms_temp;

		amss_awake_time_ms_temp = amss_awake_time_ms;
		amss_awake_time_ms  = pm_modem_awake_time_get(&amss_current_sleep_or_awake);
		deta_awake_ms = amss_awake_time_ms - amss_awake_time_ms_temp;

		amss_physlink_current_total_time_s = pm_modem_phys_link_time_get();
		deta_physlink_s = amss_physlink_current_total_time_s - amss_physlink_last_total_time_s;
		amss_physlink_last_total_time_s = amss_physlink_current_total_time_s;

		/*
		amss_current_sleep_or_awake_previous  amss_current_sleep_or_awake
		X 4 ---modem not enter sleep yet
		0 0 ---previous is sleep,curret is sleep,
				modem awake time is updated,get awake deta directly.
		otherwise get modem sleep time.
		if modem is set to offline,print offline in the log
		*/

		if ((amss_current_sleep_or_awake_previous == AMSS_NOW_SLEEP) &&
				(amss_current_sleep_or_awake == AMSS_NOW_SLEEP)) {
			/*ZTE:if sleep time is 0 and awake is 0,offline mode*/
			if (deta_awake_ms < THRESOLD_FOR_OFFLINE_AWAKE_TIME) {
				if (time_updated_when_sleep_awake_ms_temp > THRESOLD_FOR_OFFLINE_TIME)
					pr_info("[PM_V] offline mode\n");
			}
			get_amss_awake_ok = true;
			amss_awake_last = deta_awake_ms;
		} else if (amss_current_sleep_or_awake == AMSS_NEVER_ENTER_SLEEP) {
			pr_info("[PM_V] modem not enter sleep yet\n");
		}

		if (!get_amss_awake_ok) {
			amss_awake_last = time_updated_when_sleep_awake_ms_temp - deta_sleep_ms;
		}
		percentage_amss_not_sleep_while_app_suspend =
				(amss_awake_last * 1000/(time_updated_when_sleep_awake_ms_temp + 1));

#ifdef RECORED_TOTAL_TIME
		if (!record_firsttime) {
			time_app_total_awake_s += time_updated_when_sleep_awake_s;
			time_lcdon_total_s += screenontimebeforesuspend;
		}
		record_firsttime = false;
#endif
		pr_info("[PM_V] APP wake for %6d.%03d s, lcd on for %5d s %3d %%\n",
			time_updated_when_sleep_awake_s, time_updated_when_sleep_awake_ms,
			(int) screenontimebeforesuspend,
			(int)(screenontimebeforesuspend * 100/(time_updated_when_sleep_awake_s + 1)));
		pr_info("[PM_V] modem wake for %10d ms(%s) %4d %%o,modem sleep for %10d --%d%d\n",
			amss_awake_last, get_amss_awake_ok ? "get_directly " : "from sleep_time",
			percentage_amss_not_sleep_while_app_suspend,
			deta_sleep_ms, amss_current_sleep_or_awake_previous,
			amss_current_sleep_or_awake);/*in case Division by zero, +1*/

		pr_info("[PM_V] modem_phys_link_total_time %4d min %4d s\n",
			amss_physlink_current_total_time_s/60,
			amss_physlink_current_total_time_s%60);
		pr_info("[PM_V] deta_physlink_s %4d min %4d s during app wake\n",
			deta_physlink_s/60,	deta_physlink_s%60);

		time_updated_when_sleep_awake = ts;
		screenontimebeforesuspend = 0;/*ZTE:clear how long the lcd keeps on*/
	} else {
		/*ZTE:record app sleep time*/
		if (!sleep_success_flag) {
			pr_info("[PM_V] app resume due to fail to suspend\n");
			return;
		}
		sleep_success_flag = false;
		amss_sleep_time_ms_temp = amss_sleep_time_ms;
		amss_sleep_time_ms  = pm_modem_sleep_time_get();
		deta_sleep_ms = amss_sleep_time_ms - amss_sleep_time_ms_temp;
		amss_awake_time_ms_temp = amss_awake_time_ms;
		amss_awake_time_ms  = pm_modem_awake_time_get(&amss_current_sleep_or_awake);
		deta_awake_ms = amss_awake_time_ms - amss_awake_time_ms_temp;

		amss_physlink_current_total_time_s = pm_modem_phys_link_time_get();
		deta_physlink_s = amss_physlink_current_total_time_s - amss_physlink_last_total_time_s;
		amss_physlink_last_total_time_s = amss_physlink_current_total_time_s;

		/*ZTE:00,get modem awake time*/
		if ((amss_current_sleep_or_awake_previous == AMSS_NOW_SLEEP) &&
			(amss_current_sleep_or_awake == AMSS_NOW_SLEEP)) {
			/*ZTE:if sleep time is 0 and awake is 0,offline mode*/
			if ((deta_awake_ms < THRESOLD_FOR_OFFLINE_AWAKE_TIME)
				&& (time_updated_when_sleep_awake_ms_temp > THRESOLD_FOR_OFFLINE_TIME)) {
				pr_info("[PM_V] offline mode\n");
			}
			get_amss_awake_ok = true;
			amss_awake_last = deta_awake_ms;
		} else if (amss_current_sleep_or_awake == AMSS_NEVER_ENTER_SLEEP) {
			pr_info("[PM_V] modem not enter sleep yet\n");
		}

		if (!get_amss_awake_ok)
			amss_awake_last = time_updated_when_sleep_awake_ms_temp - deta_sleep_ms;

#ifdef RECORED_TOTAL_TIME
		time_app_total_sleep_s += time_updated_when_sleep_awake_s;
		if (record_firsttime_modem) {
			time_modem_firsttime_awake_s = amss_awake_last/1000;
			time_modem_firsttime_sleep_s = amss_sleep_time_ms/1000;
			record_firsttime_modem = false;
		}
		pr_info("[PM_V] modem total sleep: %d s,modem total awake %d s\n",
			(amss_sleep_time_ms/1000 - time_modem_firsttime_sleep_s),
			(amss_awake_time_ms/1000 - time_modem_firsttime_awake_s));

		pr_info("[PM_V] app total sleep: %d s,app total awake: %d s,lcd on total: %d s\n",
			time_app_total_sleep_s, time_app_total_awake_s, time_lcdon_total_s);
#endif

		if (kernel_sleep_count > 10000) {
			kernel_sleep_count = 1;
			pr_info("[PM_V] init again, kernel_sleep_count=%d\n", kernel_sleep_count);
		} else {
			kernel_sleep_count = kernel_sleep_count+1;
			if (kernel_sleep_count%5 == 0)
				pr_info("[PM_V] kernel_sleep_count=%d\n", kernel_sleep_count);
		}

		percentage_amss_not_sleep_while_app_suspend =
				(amss_awake_last * 1000/(time_updated_when_sleep_awake_ms_temp + 1));

		pr_info("[PM_V] APP sleep for %3d.%03d s, modem wake %6d ms,(%s),%3d %%o\n",
			time_updated_when_sleep_awake_s,
			time_updated_when_sleep_awake_ms, amss_awake_last,
			get_amss_awake_ok ? "get_directly " : "from sleep_time",
			percentage_amss_not_sleep_while_app_suspend);
		pr_info("[PM_V] modem_sleep for %3d ---%d%d\n",
			deta_sleep_ms, amss_current_sleep_or_awake_previous,
			amss_current_sleep_or_awake);

		pr_info("[PM_V] PhysLinkTotalTime %4d min %4d, DetaPhyslink %4d min %4d in this time\n",
			amss_physlink_current_total_time_s/60,
			amss_physlink_current_total_time_s%60,
			deta_physlink_s/60,	deta_physlink_s%60);

		time_updated_when_sleep_awake = ts;

		/*Interface For Ril Open F3 Log*/
		result_state = zte_amss_needF3log(time_updated_when_sleep_awake_s,
								percentage_amss_not_sleep_while_app_suspend);
		if (result_state == 1) {
			pr_info("[PM_V] Uevent sent to capture f3 log\n");
			zte_amss_updateEvent(result_state);
		}
	}

	amss_current_sleep_or_awake_previous = amss_current_sleep_or_awake;
}

static int zte_pm_debug_probe(struct platform_device *pdev)
{
	int ret = 0;

	/*zte_pm Interface for Ril F3 LOG*/
	msm_cpu_pm_dev = &pdev->dev;
	record_sleep_awake_time_vendor(true);
	inform_modem_app_enter_suspend(false);
	pr_info("PM notify app resume when %s \n", __func__);
	return ret;
}

static int zte_pm_debug_suspend(struct device *dev)
{
	pr_info("zte_pm_debug_suspend\n");
	output_count_set();
	if (mEnableRrecordFlag_ZTE == 0) {
		return 0;
	}
	record_sleep_awake_time_vendor(false);
	inform_modem_app_enter_suspend(true);
	return 0;
}

static int zte_pm_debug_resume(struct device *dev)
{
	pr_info("zte_pm_debug_resume\n");
	output_count_clear();
	if (mEnableRrecordFlag_ZTE == 0) {
		pr_info("[PM_V]: not enable to record zte_pm_debug_resume vendor!\n");
		return 0;
	}
	record_sleep_awake_time_vendor(true);
	inform_modem_app_enter_suspend(false);
	return 0;
}

static int  zte_pm_debug_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id zte_pm_debug_table[] = {
	{.compatible = "zte_pm_debug_vendor"},
	{},
};

static const struct dev_pm_ops zte_pm_debug_ops = {
	.suspend	= zte_pm_debug_suspend,
	.resume		= zte_pm_debug_resume,
};

#ifdef CONFIG_PM
static int pm_debug_suspend(void)
{
	pr_info("pm_debug_suspend in syscore\n");
	return 0;
}

static void pm_debug_resume(void)
{
	pr_info("pm_debug_resume in syscore\n");
}

static struct syscore_ops pm_debug_syscore_ops = {
	.suspend = pm_debug_suspend,
	.resume = pm_debug_resume,
};
#endif

static struct platform_driver zte_pm_debug_driver = {
	.probe = zte_pm_debug_probe,
	.remove	= zte_pm_debug_remove,
	.driver = {
		.name = "zte_pm_debug_vendor",
		.owner = THIS_MODULE,
		.pm	= &zte_pm_debug_ops,
		.of_match_table = zte_pm_debug_table,
	},
};

int __init zte_pm_debug_vendor_init(void)
{
	static bool registered;
	struct device_node *np; /*ZTE_PM*/
#ifdef CONFIG_PM
	register_syscore_ops(&pm_debug_syscore_ops);
#endif
	if (registered)
		return 0;
	registered = true;

	fb_register_client(&lcd_fb_notifier);

	pr_info("%s: msm-vendor-imem-pm-count-time\n", __func__);
	np = of_find_compatible_node(NULL, NULL, "qcom,msm-vendor-imem-pm-count-time");
	if (!np) {
		pr_err("unable to find DT imem msm-imem-pm-count-time node\n");
	} else {
		zte_imem_ptr = (pm_count_time  *)of_iomap(np, 0);
		if (!zte_imem_ptr)
			pr_err("unable to map imem golden copyoffset\n");
	}

	return platform_driver_register(&zte_pm_debug_driver);
}
late_initcall(zte_pm_debug_vendor_init);

static void __exit zte_pm_debug_vendor_exit(void)
{
	platform_driver_unregister(&zte_pm_debug_driver);
}
module_exit(zte_pm_debug_vendor_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("pm sleep wake time for zte");
MODULE_ALIAS("platform:zte_pm_debug_vendor");
