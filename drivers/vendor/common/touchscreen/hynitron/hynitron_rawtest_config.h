#ifndef __HYNITRON_RAWTEST_CONFIG_H__
#define __HYNITRON_RAWTEST_CONFIG_H__


struct _hynitron_selftest_str_ {
	unsigned short chan_num;
	unsigned short cmod_num;
	int screen_idac_list[60];
	int test_cp_max[60];
	int test_cp_min[60];
	int test_cmod_max[4];
	int test_cmod_min[4];
	char fw_version[6];
	int test_param_num;
	int test_param_cnt;
};

struct _hynitron_selftest_str_ cst8xx_test_info = {
	.chan_num	=  36,
	.cmod_num	=  4,
	.screen_idac_list = {7434, 8247, 12917, 13063, 13058, 13244, 13204, 13412, 13384, 13451,
						14140, 14630, 14209, 14419, 14008, 14225, 9178, 8452,
						5481, 5690, 10463, 10253, 10553, 10438, 10750, 10608, 10947, 10828,
						11116, 11006, 11397, 11213, 11567, 11407, 6982, 6574},
	.test_cp_max = {9292, 10308, 16146, 16328, 16322, 16555, 16505, 16765, 16730, 16813,
					17675, 18287, 17761, 18023, 17510, 17781, 11472, 10565,
					6851, 7112, 13078, 12816, 13191, 13047, 13437, 13260, 13683, 13535,
					13895, 13757, 14246, 14016, 14458, 14258, 8727, 8217},
	.test_cp_min = {6319, 7010, 10980, 11104, 11100, 11258, 11224, 11401, 11377, 11434,
					12019, 12436, 12078, 12257, 11907, 12092, 7802, 7185,
					4659, 4837, 8894, 8716, 8971, 8873, 9138, 9017, 9305, 9204,
					9449, 9356, 9688, 9532, 9832, 9696, 5935, 5588},
	.test_cmod_max = {8122, 12816, 12862, 8026},
	.test_cmod_min = {4874, 7690, 7718, 4816},
	.fw_version    = {'0', 'x', '1', '4'},
	.test_param_num = 6,
	.test_param_cnt = 0
};
#endif

