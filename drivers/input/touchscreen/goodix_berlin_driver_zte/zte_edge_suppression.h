#ifndef __ZTE_EDGE_SUPPRESSION_H
#define __ZTE_EDGE_SUPPRESSION_H

#define MAX_SUPPORT_FINGER	10
#define FIFO_LEN	2

typedef struct zte_point_info {
	unsigned int x;
	unsigned int y;
	unsigned int m;
	unsigned int p;
	/*struct timespec64 time_stap;*/
	int is_use;
	unsigned int id;
} zte_point_info;

typedef struct zte_point_fifo {
	zte_point_info *data[FIFO_LEN];
	int front;
	int rear;
	int size;
	int is_report;
} zte_point_fifo;

struct zte_edge_suppress_struct {
	unsigned int zte_lcd_width;
	unsigned int zte_lcd_height;
	unsigned int zte_suppress_height;
	unsigned int s_level;
	unsigned int s_mrotation;
	void *prive_data;
};

typedef int (*fact_edge_func)(int level, int is_hor, void *priv_data);

extern fact_edge_func zte_fef;

int zte_touch_report(unsigned int x, unsigned int y, unsigned int idx,
					unsigned int touch_major, unsigned int pressure,
					struct input_dev *dev);
void zte_touch_up(unsigned int idx);
void zte_edge_suppress_set(unsigned int width, unsigned int height, unsigned int sh,
						unsigned int default_level, void *priv_data);

/* Log define */
#define EDGE_INFO(fmt, arg...)	pr_info("tpd_edge_info: "fmt"\n", ##arg)
#define EDGE_ERR(fmt, arg...)	pr_err("tpd_edge_err: "fmt"\n", ##arg)

#endif /* __ZTE_EDGE_SUPPRESSION_H */
