/*
 *
 * FocalTech ft5x06 TouchScreen driver header file.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __LINUX_FT5X06_TS_H__
#define __LINUX_FT5X06_TS_H__

#ifdef CONFIG_BBRY
#define STATS_PERIOD	5000
#define STATS_SIZE		50
#endif

#define FT5X06_ID		0x55
#define FT5X16_ID		0x0A
#define FT5X36_ID		0x14
#define FT6X06_ID		0x06
#define FT6X36_ID       0x36
/* [PLATFORM]-Mod-BEGIN by TCTNB.YQJ, FR797197, 2014/11/28 add for tp gesture  */
#define FOCALTECH_PWRON_UPGRADE
#define FOCALTECH_TP_GESTURE
#define FOCALTECH_FAE_MOD
#define CONFIG_TOUCHPANEL_PROXIMITY_SENSOR
#define USB_CHARGE_DETECT
#define XINAN_5436_TEST        1//xinan
#define LEATHER_COVER
/*[FEATURE]-Modified-BEGIN by TCTSH.xingchen.wang for task 1238223, 2015/12/25, add vr funtion*/
#define VR_GLASS
/*[FEATURE]-Modified-END by TCTSH.xingchen.wang for task 1238223, 2015/12/25*/
/* [PLATFORM]-Mod-END by TCTNB.YQJ*/
//[PLATFORM] Add by wangxingchen 12/22/2014 PR.874996 Idol 3 5.5 TP Glove Function Development.
#define FOCALTECH_TP_GLOVE
//[PLATFORM] Add by wangxingchen 12/22/2014 PR.874996 end.
#include <linux/sensors.h>
struct fw_upgrade_info {
	bool auto_cal;
	u16 delay_aa;
	u16 delay_55;
	u8 upgrade_id_1;
	u8 upgrade_id_2;
	u16 delay_readid;
	u16 delay_erase_flash;
};

struct ft5x06_ts_platform_data {
	struct fw_upgrade_info info;
	const char *name;
	const char *fw_name;
	u32 irqflags;
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	u32 family_id;
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 panel_minx;
	u32 panel_miny;
	u32 panel_maxx;
	u32 panel_maxy;
	u32 group_id;
	u32 hard_rst_dly;
	u32 soft_rst_dly;
	u32 num_max_touches;
	bool fw_vkey_support;
	bool no_force_update;
	bool i2c_pull_up;
	bool ignore_id_check;
	int (*power_init) (bool);
	int (*power_on) (bool);
};
/*[FEATURE]-Modified-BEGIN by TCTSH.xingchen.wang for task 1208391, 2016/01/06, new rawdata interface*/
struct ft5x06_rawdata_test_result {
	int result;
	int min_limited_value;
	int max_limited_value;
	int min_value;
	int max_value;
	int index[350][3];
/*[FEATURE]-Modified-BEGIN by TCTSH.xingchen.wang for task 1356189, 2016/01/18, modify rawdata interface*/
	int key_index[25][3];
	int min_key_limited_value;
	int max_key_limited_value;
	int min_key_value;
	int max_key_value;
/*[FEATURE]-Modified-END by TCTSH.xingchen.wang for task 1356189, 2016/01/18*/
};
/*[FEATURE]-Modified-END by TCTSH.xingchen.wang for task 1208391, 2016/01/06*/
#endif
#ifdef CONFIG_BBRY
struct ft5x06_i2c_stats_entry {
	int total_int_cnt;
	int total_int_served_cnt;
	int touch_int_cnt;
	int touch_report_cnt;
	int touch_frelease_cnt;
	int touch_trelease_cnt;
};

struct ft5x06_i2c_stats {
	int total_int_cnt;
	int total_int_served_cnt;
	int touch_int_cnt;
	int touch_report_cnt;
	int touch_frelease_cnt;
	int touch_trelease_cnt;
	bool active;
	bool update;
	struct mutex mutex;
	struct timer_list timer;
	int num_stats_entries;
	int next_stats_entry;
	struct ft5x06_i2c_stats_entry list[STATS_SIZE];
	struct work_struct timeout_work;
};

struct ft5x06_i2c_mtouch_counter {
	int backlight_delay_300ms;
	int backlight_delay_500ms;
	int backlight_delay_750ms;
	int backlight_delay_1000ms;
	int backlight_delay_2000ms;
	int dtwakeup_delay_200ms;
	int dtwakeup_delay_400ms;
	int dtwakeup_delay_1sec;
	int dtwakeup_delay_2sec;
	int successful_double_tap;
	int swipe_wakeup;
	int double_tap;
	int upgrade_failure;
	int i2c_rw_error;
	int finger_cnt;
	bool double_tap_enabled;
	bool active;
	struct mutex mutex;
};
#endif
