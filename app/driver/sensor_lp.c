#include "ets_sys.h"
#include "osapi.h"
#include "c_types.h"

#include "user_interface.h"

#include "user_devicefind.h"
#include "user_webserver.h"

#if ESP_PLATFORM
#include "user_esp_platform.h"
#endif

#include "driver/sensor_lp.h"

void ICACHE_FLASH_ATTR
    disp_sensor_struct(SENSOR_DATA_RTC_MEM* sdata)
{
     DBG("sdata->init_flg: 0x%04x\r\n",sdata->init_flg);
     DBG("sdata->wifi_config: %d \r\n",sdata->wifi_config);
     DBG("sdata->cnt  :    %d    \r\n",sdata->cnt);
     int i;
     for(i=0;i<SENSOR_DATA_NUM;i++){
        DBG("sdata data[%d]: %d\r\n",i,sdata->data[i]);
    }

}

SENSOR_DATA_RTC_MEM sensor_data ;
SENSOR_DATA_RTC_MEM* ICACHE_FLASH_ATTR 
    get_sensor_data()
{
    return &sensor_data;

}
void ICACHE_FLASH_ATTR data_func() {
    // Read out the sensor data structure from RTC memory
    system_rtc_mem_read(SENSOR_DATA_MEM_ADDR,&sensor_data,sizeof(SENSOR_DATA_RTC_MEM));
    
    // When the system powers on for the first time, the data in the rtc memory is random.
    if(sensor_data.init_flg != INIT_MAGIC ||sensor_data.cnt>=SENSOR_DATA_NUM ){
        // This case only runs once, as long as a power supply remains connected to the chip.
        DBG("init sensor_data\r\n");
        sensor_data.init_flg = INIT_MAGIC;
        sensor_data.cnt = 0;
        // Load user params to confirm if the device is successfully registered to the server
        struct esp_platform_saved_param esp_param_t;
        user_esp_platform_load_param(&esp_param_t);
        // If registered & activated
        if(esp_param_t.activeflag==1) {
            sensor_data.wifi_config=0;//do not need to re-config router params
            DBG("ACTIVE FLAG ==1 \r\n");
        } else {
        // Not registered yet, set config flag,
        // Enable the device's softap and webserver wait for config
            if(sensor_data.wifi_config != 1) {
                sensor_data.wifi_config=1;
                system_rtc_mem_write(SENSOR_DATA_MEM_ADDR,&sensor_data,sizeof(SENSOR_DATA_RTC_MEM));
                DBG("ACTIVE FLAG ==0 \r\n");
            }
        }
    }

    if(sensor_data.wifi_config !=1) {
        uint16 vdd_val =0;
        if(sensor_data.cnt>=0 && sensor_data.cnt<SENSOR_DATA_NUM){
        /*Reads power supply voltage, the 107th byte of init_data.bin should be set to 0xFF
        * In this demo we read the power supply voltage on pin3 and pin4.
        * replace with your sensor data here; */
        vdd_val = phy_get_vdd33();
        sensor_data.data[sensor_data.cnt]=vdd_val;
        os_printf("v[%d]:%d\r\n",sensor_data.cnt,vdd_val);
        }
        if(sensor_data.cnt == SENSOR_DATA_NUM-2){
                __DEEP_SLEEP__WAKEUP_NORMAL__; 
                DBG("READY TO UPLOAD DATA NEXT TIME\r\n");
        } else {
                __DEEP_SLEEP__WAKEUP_NO_RF__; 
                DBG("DISABLE RF MODULE FOR NEXT TIME\r\n");
        }
    }
    disp_sensor_struct(&sensor_data);
    
    if(sensor_data.cnt == SENSOR_DATA_NUM-1 || sensor_data.wifi_config==1){
        /*case 1: device not activated -- open the softap interface and wait for config command.
         *case 2: device activated -- station interface connect to the saved router*/
        DBG("test : in lp sensor: cnt: %d ;  config: %d \r\n",sensor_data.cnt,sensor_data.wifi_config);
        sensor_data.cnt = 0;    
        sensor_data.wifi_config = 0;
        system_rtc_mem_write(SENSOR_DATA_MEM_ADDR,&sensor_data,sizeof(SENSOR_DATA_RTC_MEM));

        #if ESP_PLATFORM
            user_esp_platform_init();
        #endif
        DBG("test : WEB SERVER INIT\r\n");
        user_devicefind_init();
        #ifdef SERVER_SSL_ENABLE
            user_webserver_init(SERVER_SSL_PORT);
        #else
            user_webserver_init(SERVER_PORT);
        #endif
    } else {
        //case 3: activated and do not upload data in this round.
        sensor_data.cnt += 1;
        system_rtc_mem_write(SENSOR_DATA_MEM_ADDR,&sensor_data,sizeof(SENSOR_DATA_RTC_MEM));
        system_deep_sleep(DSLEEP_TIME); //sleep immediately
    }
}






