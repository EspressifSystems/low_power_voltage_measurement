#include "ets_sys.h"
#include "osapi.h"
#include "c_types.h"
#include "user_interface.h"
#include "user_devicefind.h"
#include "user_webserver.h"
#include "user_esp_platform.h"
#include "driver/sensor_lp.h"

SENSOR_DATA_RTC_MEM sensor_data ;
SENSOR_DATA_RTC_MEM* ICACHE_FLASH_ATTR get_sensor_data() { return &sensor_data; }

void ICACHE_FLASH_ATTR data_func() {
    // Read out the sensor data structure from RTC memory
    system_rtc_mem_read( SENSOR_DATA_MEM_ADDR, &sensor_data, sizeof(SENSOR_DATA_RTC_MEM) );
    
    // When the system powers on for the first time, the data in the rtc memory is random.
    struct esp_platform_saved_param esp_param_t;
    user_esp_platform_load_param(&esp_param_t);  // Stored in flash
    // Load user params to check if the device was successfully registered to the server
    // If it wasn't, it usually returns 255 (from the flash.)

    if(sensor_data.init_flg!=INIT_MAGIC || sensor_data.cnt>SENSOR_DATA_NUM ) {
        // This case runs when we first power on or when it time to flush the RTC memory of old data.
        if(esp_param_t.activeflag!=1) {   // If registered & activated
            user_esp_platform_init();     // Router is not configured. Setup softAP. Wait for config. 
            #ifdef SERVER_SSL_ENABLE
            user_webserver_init(SERVER_SSL_PORT);
            #else
            user_webserver_init(SERVER_PORT);
            #endif
        } else { // was connected! So we set init magic to exit the setup loop
            sensor_data.init_flg = INIT_MAGIC;
            sensor_data.cnt = 0;
            system_rtc_mem_write(SENSOR_DATA_MEM_ADDR, &sensor_data, sizeof(SENSOR_DATA_RTC_MEM));
            __SET__DEEP_SLEEP__WAKEUP_NO_RF__; 
            system_deep_sleep(100000); 
        }
    } else { // This is where the measurements are made
        uint16 vdd_val = 0;
        if(sensor_data.cnt<0 || sensor_data.cnt>=SENSOR_DATA_NUM) 
            sensor_data.cnt=0; // range check and resets counter if needed

        /* Reads power supply voltage, byte 107 of init_data.bin should be set to 0xFF.
        *  Replace with your own code.*/
        sensor_data.data[sensor_data.cnt++] = (uint16)(phy_get_vdd33());
        system_rtc_mem_write( SENSOR_DATA_MEM_ADDR, &sensor_data, sizeof(SENSOR_DATA_RTC_MEM) );

        // Setup next sleep cycle
        if(sensor_data.cnt==SENSOR_DATA_NUM-1) { __SET__DEEP_SLEEP__WAKEUP_NORMAL__; }
        else { __SET__DEEP_SLEEP__WAKEUP_NO_RF__; }

        // Uploads or go to sleep
        if(sensor_data.cnt == SENSOR_DATA_NUM) { user_esp_platform_init(); }
        else { system_deep_sleep(SENSOR_DEEP_SLEEP_TIME); }
    }
}

