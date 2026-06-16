
extern char hx_criteria_csv_name[30];
extern char g_hx_save_file_path[20];
extern char g_hx_save_file_name[50];
extern uint32_t  himax_tptest_result;
extern bool fw_updating;

void himax_tpd_register_fw_class(void);
#ifdef HX_PINCTRL_EN
int himax_platform_pinctrl_init(struct himax_i2c_platform_data *pdata);
#endif
int himax_get_fw_by_lcminfo(void);
int himax_save_failed_node(int failed_node);
void himax_tpd_test_result_check(uint32_t test_result);

