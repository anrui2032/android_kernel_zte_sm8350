#include <linux/timekeeper_internal.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/input/mt.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/slab.h>
#include "zte_edge_suppression.h"
#include "tpd_sys.h"

#define TYPE_B_PROTOCOL
#define ZTE_DEFAULT_LEN_LIMIT 3
#define ZTE_DEFAULT_HOR_PREX	25
#define ZTE_DEFAULT_MID_LEVEL	5

static zte_point_info *point_info_arry;
static zte_point_fifo *point_fifo_array;

static int zte_edge_prex = 0;
static int zte_edge_prey = 0;
static int zte_edge_len_limit = 0;

static int zte_mrotation = 0;

static struct zte_edge_suppress_struct *zte_edge_param;

fact_edge_func zte_fef = NULL;

static void zte_set_edge_param(int prex, int prey, int len_limit)
{
	zte_edge_prex = prex;
	zte_edge_prey = prey;
	zte_edge_len_limit = len_limit;
}

static zte_point_info *get_free_pi(void)
{
	int i;

	for (i = 0; i < MAX_SUPPORT_FINGER * FIFO_LEN + 1; i++) {
		if (!point_info_arry[i].is_use)
			return &(point_info_arry[i]);
	}

	return NULL;
}

static void point_add_fifo(zte_point_info *pi)
{
	zte_point_fifo *temp = &(point_fifo_array[pi->id]);

	if (temp->size == 2) {
		(temp->data)[temp->front]->is_use = 0;
		temp->front = (temp->front + 1) % 2;
		temp->size = 1;
	}

	(temp->data)[temp->rear] = pi;
	temp->rear = (temp->rear + 1) % 2;
	temp->size += 1;
}

static int is_report_point(unsigned int x, unsigned int y,
							unsigned int idx, int len_lim)
{
	zte_point_fifo *temp = &(point_fifo_array[idx]);
	int diffx1, diffx2, diffy1, diffy2, diffx3, diffy3;
	int rear, front;

	if (temp->is_report)/* If the current finger is no longer suppressed, no need judge */
		return 1;

	if (temp->size != 2)
		return 0;

	front = temp->front;
	rear = (temp->front + 1) % 2;

	diffx1 = (temp->data)[rear]->x - (temp->data)[front]->x;
	diffy1 = (temp->data)[rear]->y - (temp->data)[front]->y;

	diffx2 = x - (temp->data)[front]->x;
	diffy2 = y - (temp->data)[front]->y;

	diffx3 = x - (temp->data)[rear]->x;
	diffy3 = y - (temp->data)[rear]->y;

	/* There are only two points here, simplified */
	if (((diffx1 > len_lim || diffy1 > len_lim) &&
		(diffx3 > len_lim || diffy3 > len_lim)) ||
		((diffx1 < -len_lim || diffy1 < -len_lim) &&
		(diffx3 < -len_lim || diffy3 < -len_lim))) { /* Enough distance */
		if (diffx1 == 0 && diffx2 == 0)	/* The slopes should be similar */
			goto report_point;
		if (diffx1 == 0 && ((diffy2 / diffx2) == 0))
			goto report_point;
		if (((diffy1 / diffx1) == 0) && diffx2 == 0)
			goto report_point;
		if ((diffy1 / diffx1) == (diffy2 / diffx2))
			goto report_point;
	} else
		EDGE_ERR("distance is too small!");

	return 0;

report_point:
	temp->is_report = 1;
	return 1;
}

static inline void clear_point_fifo(int idx)
{
	int rear, front;
	zte_point_fifo *temp = &(point_fifo_array[idx]);

	front = temp->front;
	rear = (temp->front + 1) % 2;

	if (temp->size == 2) {
		(temp->data)[front]->is_use = 0;
		(temp->data)[rear]->is_use = 0;
	} else if (temp->size) {
		(temp->data)[front]->is_use = 0;
	}

	temp->is_report = 0;
	temp->size = 0;
	temp->front = 0;
	temp->rear = 0;
}

static inline int point_is_press(int x, int y, int pre_x, int pre_y)
{
	int ret = 0;

	if (((pre_x > x) || (zte_edge_param->zte_lcd_width - pre_x < x)) &&
		(zte_edge_param->zte_lcd_height - pre_y < y)) {
			ret = 1;
	}

	return ret;
}

static inline void zte_input_report(unsigned int x, unsigned int y, unsigned int idx,
									unsigned int m, unsigned int p,
									struct input_dev *input_dev)
{
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(input_dev, idx);
			input_mt_report_slot_state(input_dev,
					MT_TOOL_FINGER, 1);
#endif
			input_report_key(input_dev, BTN_TOUCH, 1);
			input_report_key(input_dev, BTN_TOOL_FINGER, 1);
			input_report_abs(input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
			if (m != -1)
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, m);
			if (p != -1)
				input_report_abs(input_dev, ABS_MT_PRESSURE, p);
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(input_dev);
#endif
}

void zte_edge_suppress_set(unsigned int width, unsigned int height, unsigned int sh,
						unsigned int default_level, void *priv_data)
{
	zte_edge_param->zte_lcd_width = width;
	zte_edge_param->zte_lcd_height = height;
	zte_edge_param->zte_suppress_height = sh;
	zte_edge_param->prive_data = priv_data;
	zte_edge_param->s_level = default_level;
}

void zte_touch_up(unsigned int idx)
{
	clear_point_fifo(idx);
}

int zte_touch_report(unsigned int x, unsigned int y, unsigned int idx,
					unsigned int touch_major, unsigned int pressure,
					struct input_dev *dev)
{
	zte_point_info *pi;
	zte_point_fifo *temp = &(point_fifo_array[idx]);

	if (temp->is_report)/* If the current finger is no longer suppressed, no longer judge */
		goto input_report_point;

	if (point_is_press(x, y, zte_edge_prex, zte_edge_prey) &&
		(!is_report_point(x, y, idx, zte_edge_len_limit))) {
		pi = get_free_pi();
		if (pi == NULL) {
			EDGE_ERR("pi is full!\n");
			goto input_report_point;
		}

		pi->id = idx;
		pi->x = x;
		pi->y = y;
		pi->m = touch_major;
		pi->p = pressure;

		pi->is_use = 1;

		point_add_fifo(pi);
		goto exit;
	} else {	/* If the current point is not suppressed */
		if (temp->size) {
/* The effect of the supplementary point does not seem to be so good
			zte_input_report((temp->data)[temp->front]->x,
							(temp->data)[temp->front]->y,
							(temp->data)[temp->front]->id,
							(temp->data)[temp->front]->m,
							(temp->data)[temp->front]->p,
							dev);
			clear_point_fifo(idx);
			usleep_range(1000, 1500);
*/
			x = (temp->data)[temp->front]->x;
			y = (temp->data)[temp->front]->y;
			touch_major = (temp->data)[temp->front]->m;
			pressure = (temp->data)[temp->front]->p;
			clear_point_fifo(idx);
		}
		temp->is_report = 1;
	}

input_report_point:
	zte_input_report(x, y, idx, touch_major, pressure, dev);
	return 0;
exit:
	return 1;
}

/**
 * if zte_fef set failed, zte_fef need to return a no zero;
 * if level  > *_MAX_EDGE_LEVEL, zte_fef need to return a no zero too
*/
static inline void __zte_set_edge_limit_level(u8 level)
{
	int ret = 1;
	int suppress_len;

	if (zte_fef != NULL) {
		ret = zte_fef(level, 0, zte_edge_param->prive_data);
	}

	if (ret) {
		if (level <= ZTE_DEFAULT_MID_LEVEL)
			zte_set_edge_param(level * zte_edge_param->zte_lcd_width / 100,
							zte_edge_param->zte_suppress_height, ZTE_DEFAULT_LEN_LIMIT);
		else {
			suppress_len = ZTE_DEFAULT_MID_LEVEL * zte_edge_param->zte_lcd_width / 100 +
						(level - ZTE_DEFAULT_MID_LEVEL) * zte_edge_param->zte_lcd_width / 200;
			zte_set_edge_param(suppress_len,
							zte_edge_param->zte_suppress_height, ZTE_DEFAULT_LEN_LIMIT);
		}
	} else
		zte_set_edge_param(0, 0, 0);
}

static int zte_set_edge_limit_level(struct tpd_classdev_t *cdev, u8 level)
{
	zte_edge_param->s_level = level;
	if (!(zte_mrotation & 0x01))
		__zte_set_edge_limit_level(level);

	return 0;
}

static int zte_get_edge_limit_level(struct tpd_classdev_t *cdev)
{
	cdev->edge_limit_level = zte_edge_param->s_level;

	return 0;
}

/**
 * if zte_fef set failed, zte_fef need to return a no zero;
 * if success, zte_fef need to return  zero;
*/
static inline void zte_hor_screen_limit(int mrotation)
{
	int ret = 1;

	if (zte_fef != NULL) {
		ret = zte_fef(-1, mrotation, zte_edge_param->prive_data);
	}

	if (ret)
		zte_set_edge_param(ZTE_DEFAULT_HOR_PREX, zte_edge_param->zte_lcd_height,
						ZTE_DEFAULT_LEN_LIMIT);
	else
		zte_set_edge_param(0, 0, 0);
}

static int tpd_set_display_rotation(struct tpd_classdev_t *cdev, int mrotation)
{
	zte_mrotation = mrotation;

	if (mrotation & 0x01) {
		zte_hor_screen_limit(mrotation);
	} else {
		__zte_set_edge_limit_level(zte_edge_param->s_level);
	}
	return 0;
}

static int point_array_init(void)
{
	int i;

	point_info_arry = kcalloc(MAX_SUPPORT_FINGER * FIFO_LEN + 1,
							sizeof(struct zte_point_info),
							GFP_KERNEL);
	if (!point_info_arry) {
		EDGE_ERR("point_info_arry alloc failed!\n");
		goto EXIT;
	}

	point_fifo_array = kcalloc(MAX_SUPPORT_FINGER,
							sizeof(struct zte_point_fifo),
							GFP_KERNEL);
	if (!point_fifo_array) {
		EDGE_ERR("point_fifo_array alloc failed!\n");
		goto FIFO_ARRAY_FAIL;
	}

	for (i = 0; i < MAX_SUPPORT_FINGER * FIFO_LEN + 1; i++) {
		point_info_arry[i].is_use = 0;
	}

	for (i = 0; i < MAX_SUPPORT_FINGER; i++) {
		point_fifo_array[i].size = 0;
		point_fifo_array[i].front = 0;
		point_fifo_array[i].rear = 0;
	}

	return 0;

FIFO_ARRAY_FAIL:
	kfree(point_info_arry);
EXIT:
	return -ENOMEM;
}

static int __init edge_suppre_init(void)
{
	point_array_init();

	zte_edge_param = kzalloc(sizeof(struct zte_edge_suppress_struct), GFP_KERNEL);
	if (!zte_edge_param) {
		EDGE_ERR("zte_edge_param alloc failed!\n");
		goto EXIT;
	}

	tpd_fw_cdev.set_edge_limit_level = zte_set_edge_limit_level;
	tpd_fw_cdev.get_edge_limit_level = zte_get_edge_limit_level;
	tpd_fw_cdev.set_display_rotation = tpd_set_display_rotation;

	return 0;

EXIT:
	kfree(point_info_arry);
	kfree(point_fifo_array);
	return 0;
}

static void __exit touch_module_exit(void)
{
	kfree(point_info_arry);
	kfree(point_fifo_array);
	kfree(zte_edge_param);
}

module_init(edge_suppre_init);
module_exit(touch_module_exit);

MODULE_AUTHOR("zte");
MODULE_DESCRIPTION("zte edge suppression");
MODULE_LICENSE("GPL");
