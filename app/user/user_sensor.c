/******************************************************************************
 * Copyright 2013-2014 Espressif Systems 
 *
 * FileName: user_sensor.c
 *
 * Description: sensor demo's function realization
 *
 * Modification history:
 *     2014/5/1, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"

#include "user_config.h"

#if SENSOR_DEVICE
#include "user_sensor.h"

LOCAL struct keys_param keys;
LOCAL struct single_key_param *single_key[SENSOR_KEY_NUM];
LOCAL os_timer_t sensor_sleep_timer;
LOCAL os_timer_t link_led_timer;
LOCAL uint8 link_led_level = 0;
LOCAL uint32 link_start_time;


/******************************************************************************
 * FunctionName : user_sensor_long_press
 * Description  : sensor key's function, needed to be installed
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_sensor_long_press(void) {
    user_esp_platform_set_active(0);
    system_restore();
    system_restart();
}

LOCAL void ICACHE_FLASH_ATTR user_link_led_init(void) {
    PIN_FUNC_SELECT(SENSOR_LINK_LED_IO_MUX, SENSOR_LINK_LED_IO_FUNC);
    PIN_FUNC_SELECT(SENSOR_UNUSED_LED_IO_MUX, SENSOR_UNUSED_LED_IO_FUNC);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(SENSOR_UNUSED_LED_IO_NUM), 0);
}

void ICACHE_FLASH_ATTR user_link_led_output(uint8 level) {
    GPIO_OUTPUT_SET(GPIO_ID_PIN(SENSOR_LINK_LED_IO_NUM), level);
}

LOCAL void ICACHE_FLASH_ATTR user_link_led_timer_cb(void) {
    link_led_level = (~link_led_level) & 0x01;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(SENSOR_LINK_LED_IO_NUM), link_led_level);
}

void ICACHE_FLASH_ATTR user_link_led_timer_init(void) {
    link_start_time = system_get_time();
    os_timer_disarm(&link_led_timer);
    os_timer_setfn(&link_led_timer, (os_timer_func_t *)user_link_led_timer_cb, NULL);
    os_timer_arm(&link_led_timer, 50, 1);
    link_led_level = 0;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(SENSOR_LINK_LED_IO_NUM), link_led_level);
}

void ICACHE_FLASH_ATTR user_link_led_timer_done(void) {
    os_timer_disarm(&link_led_timer);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(SENSOR_LINK_LED_IO_NUM), 0);
}

void ICACHE_FLASH_ATTR user_sensor_deep_sleep_enter(void) {
    system_deep_sleep(SENSOR_DEEP_SLEEP_TIME > link_start_time \
    ? SENSOR_DEEP_SLEEP_TIME - link_start_time : 30000000);
}

void ICACHE_FLASH_ATTR user_sensor_deep_sleep_disable(void) {
    os_timer_disarm(&sensor_sleep_timer);
}

void ICACHE_FLASH_ATTR user_sensor_deep_sleep_init(uint32 time) {
    os_timer_disarm(&sensor_sleep_timer);
    os_timer_setfn(&sensor_sleep_timer, (os_timer_func_t *)user_sensor_deep_sleep_enter, NULL);
    os_timer_arm(&sensor_sleep_timer, time, 0);
}

/******************************************************************************
 * FunctionName : user_sensor_init
 * Description  : init sensor function, include key and mvh3004
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_sensor_init(uint8 active) {
    user_link_led_init();
    wifi_status_led_install(SENSOR_WIFI_LED_IO_NUM, SENSOR_WIFI_LED_IO_MUX, SENSOR_WIFI_LED_IO_FUNC);

    if (wifi_get_opmode() != SOFTAP_MODE) {
        single_key[0] = key_init_single(SENSOR_KEY_IO_NUM, SENSOR_KEY_IO_MUX, SENSOR_KEY_IO_FUNC,
                                        user_sensor_long_press, NULL);

        keys.key_num = SENSOR_KEY_NUM;
        keys.single_key = single_key;
        key_init(&keys);

        if (GPIO_INPUT_GET(GPIO_ID_PIN(SENSOR_KEY_IO_NUM)) == 0) {
            user_sensor_long_press();
        }
    }

    if (wifi_get_opmode() != STATIONAP_MODE) {
        if (active == 1) {
            user_sensor_deep_sleep_init(SENSOR_CONNECT_TIME / 1000);//exceed SENSOR_CONNECT_TIME,sleep directly
        } else {
        user_sensor_deep_sleep_init(SENSOR_CONNECT_TIME / 1000);//exceed SENSOR_CONNECT_TIME,sleep directly
        }
    }
}
#endif

