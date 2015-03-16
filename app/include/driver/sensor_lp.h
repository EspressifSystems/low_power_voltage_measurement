#ifndef _SENSOR_LP_H
#define _SENSOR_LP_H

#define DBG  //os_printf   

#define SENSOR_DATA_NUM 20
#define SENSOR_DATA_MEM_ADDR 120
#define INIT_MAGIC 0x7e7e55aa

#define RTC_CNT_ADDR 120
#define DSLEEP_TIME SENSOR_DEEP_SLEEP_TIME
#define SENSOR_DATA_SET_LEN      50 // max LEN OF STR LIKE :  {"x":1,"y":351}
#define SENSOR_DATA_STREAM "supply-voltage"  //in accord with the data-point name on the server
#define    __DEEP_SLEEP__WAKEUP_NO_RF__  system_deep_sleep_set_option(4)
#define    __DEEP_SLEEP__WAKEUP_NORMAL__	system_deep_sleep_set_option(1)


typedef struct{
	uint32 init_flg;
	uint16 cnt;
	uint16 wifi_config;
	uint16 data[SENSOR_DATA_NUM];
}SENSOR_DATA_RTC_MEM;


void data_func();
SENSOR_DATA_RTC_MEM* get_sensor_data();


#endif

