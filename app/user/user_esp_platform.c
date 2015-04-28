/******************************************************************************
 * Copyright 2013-2014 Espressif Systems 
 *
 * FileName: user_esp_platform.c
 *
 * Description: The client mode configration.
 *              Check your hardware connection with the host while use this mode.
 *
 * Modification history:
 *     2014/5/09, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "espconn.h"
#include "user_esp_platform.h"
#include "user_iot_version.h"
#include "upgrade.h"
#include "driver/sensor_lp.h"
#include "driver/uart.h"
#include "user_config.h"


#define ESP_DEBUG
#ifdef  ESP_DEBUG
#define ESP_DBG os_printf
#else
#define ESP_DBG
#endif

#define ACTIVE_FRAME    "{\"nonce\": %d,\"path\": \"/v1/device/activate/\", \"method\": \"POST\", \"body\": {\"encrypt_method\": \"PLAIN\", \"token\": \"%s\", \"bssid\": \""MACSTR"\",\"rom_version\":\"%s\"}, \"meta\": {\"Authorization\": \"token %s\"}}\n"

#if SENSOR_DEVICE
#include "user_sensor.h"

#ifdef LOW_POWER_MODE
#define MULTI_UPLOAD_FRAME    "{\"nonce\": %d, \"path\": \"/v1/datastreams/%s/datapoints/\", \"method\": \"POST\", \
 \"meta\": {\"Authorization\": \"token %s\"},\"body\": {\"datapoints\": ["
#define LP_DATA_SET "{\"x\":%d,\"y\":%d}"
#define LP_FRAME_END "]}}\n"
#endif

#if HUMITURE_SUB_DEVICE
#define UPLOAD_FRAME  "{\"nonce\": %d, \"path\": \"/v1/datastreams/tem_hum/datapoint/\", \"method\": \"POST\", \
\"body\": {\"datapoint\": {\"x\": %s%d.%02d,\"y\": %d.%02d}}, \"meta\": {\"Authorization\": \"token %s\"}}\n"
#endif

LOCAL uint32 count = 0;
#endif

#define UPGRADE_FRAME  "{\"path\": \"/v1/messages/\", \"method\": \"POST\", \"meta\": {\"Authorization\": \"token %s\"},\
\"get\":{\"action\":\"%s\"},\"body\":{\"pre_rom_version\":\"%s\",\"rom_version\":\"%s\"}}\n"


#ifdef USE_DNS
ip_addr_t esp_server_ip;
#endif

LOCAL struct espconn user_conn;
LOCAL struct _esp_tcp user_tcp;
LOCAL os_timer_t client_timer;
LOCAL struct esp_platform_saved_param esp_param;
LOCAL uint8 device_status;
LOCAL uint8 device_recon_count = 0;
LOCAL uint32 active_nonce = 0;
LOCAL uint8 iot_version[20] = {0};
struct rst_info rtc_info;
void user_esp_platform_check_ip(uint8 reset_flag);

/******************************************************************************
 * FunctionName : user_esp_platform_load_param
 * Description  : load parameter from flash, toggle use two sector by flag value.
 * Parameters   : param--the parame point which write the flash
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_esp_platform_load_param(struct esp_platform_saved_param *param) {
    struct esp_platform_sec_flag_param flag;

    spi_flash_read((ESP_PARAM_START_SEC + ESP_PARAM_FLAG) * SPI_FLASH_SEC_SIZE,
                   (uint32 *)&flag, sizeof(struct esp_platform_sec_flag_param));

    if (flag.flag==0) {
        spi_flash_read((ESP_PARAM_START_SEC + ESP_PARAM_SAVE_0) * SPI_FLASH_SEC_SIZE,
                       (uint32 *)param, sizeof(struct esp_platform_saved_param));
    } else {
        spi_flash_read((ESP_PARAM_START_SEC + ESP_PARAM_SAVE_1) * SPI_FLASH_SEC_SIZE,
                       (uint32 *)param, sizeof(struct esp_platform_saved_param));
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_save_param
 * Description  : toggle save param to two sector by flag value,
 *              : protect write and erase data while power off.
 * Parameters   : param -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_save_param(struct esp_platform_saved_param *param)
{
    struct esp_platform_sec_flag_param flag;

    spi_flash_read((ESP_PARAM_START_SEC + ESP_PARAM_FLAG) * SPI_FLASH_SEC_SIZE,
                   (uint32 *)&flag, sizeof(struct esp_platform_sec_flag_param));

    if (flag.flag==0) {
        spi_flash_erase_sector(ESP_PARAM_START_SEC + ESP_PARAM_SAVE_1);
        spi_flash_write((ESP_PARAM_START_SEC + ESP_PARAM_SAVE_1) * SPI_FLASH_SEC_SIZE,
                        (uint32 *)param, sizeof(struct esp_platform_saved_param));
        flag.flag = 1;
        spi_flash_erase_sector(ESP_PARAM_START_SEC + ESP_PARAM_FLAG);
        spi_flash_write((ESP_PARAM_START_SEC + ESP_PARAM_FLAG) * SPI_FLASH_SEC_SIZE,
                        (uint32 *)&flag, sizeof(struct esp_platform_sec_flag_param));
    } else {
        spi_flash_erase_sector(ESP_PARAM_START_SEC + ESP_PARAM_SAVE_0);
        spi_flash_write((ESP_PARAM_START_SEC + ESP_PARAM_SAVE_0) * SPI_FLASH_SEC_SIZE,
                        (uint32 *)param, sizeof(struct esp_platform_saved_param));
        flag.flag = 0;
        spi_flash_erase_sector(ESP_PARAM_START_SEC + ESP_PARAM_FLAG);
        spi_flash_write((ESP_PARAM_START_SEC + ESP_PARAM_FLAG) * SPI_FLASH_SEC_SIZE,
                        (uint32 *)&flag, sizeof(struct esp_platform_sec_flag_param));
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_get_token
 * Description  : get the espressif's device token
 * Parameters   : token -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_get_token(uint8_t *token)
{
    if (token==NULL) {
        return;
    }

    os_memcpy(token, esp_param.token, sizeof(esp_param.token));
}

/******************************************************************************
 * FunctionName : user_esp_platform_set_token
 * Description  : save the token for the espressif's device
 * Parameters   : token -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_esp_platform_set_token(uint8_t *token) {
    if (token==NULL) return; 
    esp_param.activeflag = 0;
    os_memcpy(esp_param.token, token, os_strlen(token));
    user_esp_platform_save_param(&esp_param);
}

/******************************************************************************
 * FunctionName : user_esp_platform_set_active
 * Description  : set active flag
 * Parameters   : activeflag -- 0 or 1
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_esp_platform_set_active(uint8 activeflag) {
    esp_param.activeflag = activeflag;
    user_esp_platform_save_param(&esp_param);
}

void ICACHE_FLASH_ATTR user_esp_platform_set_connect_status(uint8 status) {
    device_status = status;
}

/******************************************************************************
 * FunctionName : user_esp_platform_get_connect_status
 * Description  : get each connection step's status
 * Parameters   : none
 * Returns      : status
*******************************************************************************/
uint8 ICACHE_FLASH_ATTR user_esp_platform_get_connect_status(void) {
    uint8 status = wifi_station_get_connect_status();
    if (status==STATION_GOT_IP) {
        status = (device_status==0) ? DEVICE_CONNECTING : device_status;
    }

    ESP_DBG("status %d\n", status);
    return status;
}

/******************************************************************************
 * FunctionName : user_esp_platform_parse_nonce
 * Description  : parse the device nonce
 * Parameters   : pbuffer -- the recivce data point
 * Returns      : the nonce
*******************************************************************************/
int ICACHE_FLASH_ATTR user_esp_platform_parse_nonce(char *pbuffer) {
    char *pstr   = NULL;
    char *pparse = NULL;
    char noncestr[11] = {0};
    int nonce = 0;
    pstr = (char *)os_strstr(pbuffer, "\"nonce\": ");

    if (pstr!=NULL) {
        pstr += 9;
        pparse = (char *)os_strstr(pstr, ",");

        if (pparse!=NULL) { os_memcpy(noncestr, pstr, pparse - pstr); } 
        else {
            pparse = (char *)os_strstr(pstr, "}");
            if (pparse!=NULL) { os_memcpy(noncestr, pstr, pparse - pstr); } 
            else {
                pparse = (char *)os_strstr(pstr, "]");
                if (pparse!=NULL) { os_memcpy(noncestr, pstr, pparse - pstr); }
                else { return 0; }
            }
        }
        nonce = atoi(noncestr);
    }
    return nonce;
}

/******************************************************************************
 * FunctionName : user_esp_platform_get_info
 * Description  : get and update the espressif's device status
 * Parameters   : pespconn -- the espconn used to connect with host
 *                pbuffer -- prossing the data point
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_esp_platform_get_info(struct espconn *pconn, uint8 *pbuffer) {
    char *pbuf = NULL;
    int nonce = 0;

    pbuf = (char *)os_zalloc(packet_size);
    nonce = user_esp_platform_parse_nonce(pbuffer);

    if (pbuf!=NULL) {
        ESP_DBG("%s\n", pbuf);
#ifdef CLIENT_SSL_ENABLE
        espconn_secure_sent(pconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pconn, pbuf, os_strlen(pbuf));
#endif
        os_free(pbuf);
        pbuf = NULL;
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_set_info
 * Description  : prossing the data and controling the espressif's device
 * Parameters   : pespconn -- the espconn used to connect with host
 *                pbuffer -- prossing the data point
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_set_info(struct espconn *pconn, uint8 *pbuffer)
{
    user_esp_platform_get_info(pconn, pbuffer);
}

/******************************************************************************
 * FunctionName : user_esp_platform_reconnect
 * Description  : reconnect with host after get ip
 * Parameters   : pespconn -- the espconn used to reconnect with host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_reconnect(struct espconn *pespconn)
{
    ESP_DBG("user_esp_platform_reconnect\n");

    user_esp_platform_check_ip(0);
}

/******************************************************************************
 * FunctionName : user_esp_platform_discon_cb
 * Description  : disconnect successfully with the host
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_discon_cb(void *arg)
{
    struct espconn *pespconn = arg;
    struct ip_info ipconfig;
    struct dhcp_client_info dhcp_info;
    ESP_DBG("user_esp_platform_discon_cb\n");


    if (pespconn==NULL) {
        return;
    }

    pespconn->proto.tcp->local_port = espconn_port();

    user_link_led_output(1);

#if SENSOR_DEVICE
#ifdef SENSOR_DEEP_SLEEP

    if (wifi_get_opmode()==STATION_MODE) {
        /***add by tzx for saving ip_info to avoid dhcp_client start****/
        wifi_get_ip_info(STATION_IF, &ipconfig);

        dhcp_info.ip_addr = ipconfig.ip;
        dhcp_info.netmask = ipconfig.netmask;
        dhcp_info.gw = ipconfig.gw ;
        dhcp_info.flag = 0x01;
        os_printf("dhcp_info.ip_addr = %d\n",dhcp_info.ip_addr);
        system_rtc_mem_write(64,&dhcp_info,sizeof(struct dhcp_client_info));

        user_sensor_deep_sleep_enter();
    } else {
        os_timer_disarm(&client_timer);
        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_reconnect, pespconn);
        os_timer_arm(&client_timer, SENSOR_DEEP_SLEEP_TIME / 1000, 0);
    }

#else
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_reconnect, pespconn);
    os_timer_arm(&client_timer, 1000, 0);
#endif
#else
    user_esp_platform_reconnect(pespconn);
#endif
}

/******************************************************************************
 * FunctionName : user_esp_platform_discon
 * Description  : A new incoming connection has been disconnected.
 * Parameters   : espconn -- the espconn used to disconnect with host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_discon(struct espconn *pespconn)
{
    ESP_DBG("user_esp_platform_discon\n");

    user_link_led_output(1);
#ifdef CLIENT_SSL_ENABLE
    espconn_secure_disconnect(pespconn);
#else
    espconn_disconnect(pespconn);
#endif


}

/******************************************************************************
 * FunctionName : user_esp_platform_sent_cb
 * Description  : Data has been sent successfully and acknowledged by the remote host.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_sent_cb(void *arg)
{
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_sent_cb\n");
    #ifdef LOW_POWER_MODE
    if(esp_param.activeflag==1){
        
        //wifi_station_disconnect();
        //os_printf("wifi disconnect\r\n");
        //system_deep_sleep(2000000 );
        user_sensor_deep_sleep_enter();
    }
    #endif
}

/******************************************************************************
 * FunctionName : user_esp_platform_sent
 * Description  : Processing the application data and sending it to the host
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR user_esp_platform_sent(struct espconn *pespconn) {
    uint8 devkey[token_size] = {0};
    uint32 nonce;
    char *pbuf = (char *)os_zalloc(packet_size);
    os_memcpy(devkey, esp_param.devkey, 40);

    if (esp_param.activeflag==0xFF) { esp_param.activeflag = 0; }

    if (pbuf!=NULL) {
        if (esp_param.activeflag==0) {  // Has not registered the account
            uint8 token[token_size] = {0};
            uint8 bssid[6];
            active_nonce = rand();
            os_memcpy(token, esp_param.token, 40);
            wifi_get_macaddr(STATION_IF, bssid);
            os_sprintf(pbuf, ACTIVE_FRAME, active_nonce, token, MAC2STR(bssid),iot_version, devkey);
        } else {       // Has registered. We can upload.  
            SENSOR_DATA_RTC_MEM* s_data = get_sensor_data();
            int idx;
            os_memset(pbuf,0,sizeof(pbuf));
            os_sprintf(pbuf,MULTI_UPLOAD_FRAME,count,SENSOR_DATA_STREAM,devkey);
            uint8* data_set_tmp = (uint8*) os_zalloc(SENSOR_DATA_SET_LEN);
        
            if(data_set_tmp) {
                for(idx=0; idx<SENSOR_DATA_NUM; idx++){
                    os_memset(data_set_tmp, 0, SENSOR_DATA_SET_LEN);

                    if(idx==SENSOR_DATA_NUM-1) { os_sprintf(data_set_tmp, "{\"x\":%d,\"y\":%d}", idx,s_data->data[idx]); } 
                    else { os_sprintf(data_set_tmp, "{\"x\":%d,\"y\":%d},", idx,s_data->data[idx]); }
                    //os_printf("test :data_set_tmp: %s\r\n",data_set_tmp);

                    if(os_strlen(pbuf)+SENSOR_DATA_SET_LEN<packet_size) { os_strcat(pbuf,data_set_tmp); } 
                    else { break; }
                }
            }
            os_strcat(pbuf,LP_FRAME_END);
            os_free(data_set_tmp);
            //tp_t = (tp >> 2) * 165 * 100 / (16384 - 1);
            //rh_t = (rh & 0x3fff) * 100 * 100 / (16384 - 1);
            //if (tp_t >= 4000) {
            //    os_sprintf(pbuf, UPLOAD_FRAME, count, "", tp_t / 100 - 40, tp_t % 100, rh_t / 100, rh_t % 100, devkey);
            //} else {
            //    tp_t = 4000 - tp_t;
            //    os_sprintf(pbuf, UPLOAD_FRAME, count, "-", tp_t / 100, tp_t % 100, rh_t / 100, rh_t % 100, devkey);
            //}
        }
        ESP_DBG("%s\n", pbuf);

#ifdef CLIENT_SSL_ENABLE
        espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));
#else
        espconn_sent(pespconn, pbuf, os_strlen(pbuf));
#endif
        os_free(pbuf);
    }
}


/******************************************************************************
 * FunctionName : user_esp_platform_recv_cb
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pusrdata -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
    char *pstr = NULL;
    LOCAL char pbuffer[1024 * 2] = {0};
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_recv_cb %s\n", pusrdata);

    if (length==1460) {
        os_memcpy(pbuffer, pusrdata, length);
    } else {
        struct espconn *pespconn = (struct espconn *)arg;

        os_memcpy(pbuffer + os_strlen(pbuffer), pusrdata, length);

        if ((pstr = (char *)os_strstr(pbuffer, "\"activate_status\": "))!=NULL &&
                user_esp_platform_parse_nonce(pbuffer)==active_nonce) {
            if (os_strncmp(pstr + 19, "1", 1)==0) {
                ESP_DBG("device activates successful.\n");

                device_status = DEVICE_ACTIVE_DONE;
                esp_param.activeflag = 1;
                user_esp_platform_save_param(&esp_param);
        #ifdef LOW_POWER_MODE
            user_sensor_deep_sleep_enter();
        #else
                user_esp_platform_sent(pespconn);
        #endif
            } else {
                ESP_DBG("device activates failed.\n");
                device_status = DEVICE_ACTIVE_FAIL;
            }
        }

#if SENSOR_DEVICE
        else if ((pstr = (char *)os_strstr(pbuffer, "\"status\":"))!=NULL) {
            if (os_strncmp(pstr + 10, "200", 3)!=0) {
                ESP_DBG("message upload failed.\n");
            } else {
                count++;
                ESP_DBG("message upload sucessful.\n");
            }

            os_timer_disarm(&client_timer);
            os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_discon, pespconn);
            os_timer_arm(&client_timer, 10, 0);
        }

#endif
        else if ((pstr = (char *)os_strstr(pbuffer, "device"))!=NULL) {
        }

        os_memset(pbuffer, 0, sizeof(pbuffer));
    }

}

#if AP_CACHE
/******************************************************************************
 * FunctionName : user_esp_platform_ap_change
 * Description  : add the user interface for changing to next ap ID.
 * Parameters   :
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_ap_change(void)
{
    uint8 current_id;
    uint8 i = 0;
    ESP_DBG("user_esp_platform_ap_is_changing\n");

    current_id = wifi_station_get_current_ap_id();
    ESP_DBG("current ap id =%d\n", current_id);

    if (current_id==AP_CACHE_NUMBER - 1) {
       i = 0;
    } else {
       i = current_id + 1;
    }
    while (wifi_station_ap_change(i)!=true) {
       i++;
       if (i==AP_CACHE_NUMBER - 1) {
           i = 0;
       }
    }

    /* just need to re-check ip while change AP */
    device_recon_count = 0;
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
    os_timer_arm(&client_timer, 100, 0);
}
#endif

LOCAL bool ICACHE_FLASH_ATTR
user_esp_platform_reset_mode(void)
{
    if (wifi_get_opmode()==STATION_MODE) {
        wifi_set_opmode(STATIONAP_MODE);
    }

#if AP_CACHE
    /* delay 5s to change AP */
    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_ap_change, NULL);
    os_timer_arm(&client_timer, 5000, 0);

    return true;
#endif

    return false;
}

/******************************************************************************
 * FunctionName : user_esp_platform_recon_cb
 * Description  : The connection had an error and is already deallocated.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_recon_cb(void *arg, sint8 err)
{
    struct espconn *pespconn = (struct espconn *)arg;

    ESP_DBG("user_esp_platform_recon_cb\n");


    user_link_led_output(1);

// !!!Check logic over here


    if (++device_recon_count==5) {
        device_status = DEVICE_CONNECT_SERVER_FAIL;
        if (user_esp_platform_reset_mode()) return; 
    }

    if (wifi_get_opmode()==STATION_MODE) { user_esp_platform_reset_mode(); }
    else {
        os_timer_disarm(&client_timer);
        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_reconnect, pespconn);
        os_timer_arm(&client_timer, 1000, 0);
    }

}

/******************************************************************************
 * FunctionName : user_esp_platform_connect_cb
 * Description  : A new incoming connection has been connected.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_connect_cb(void *arg)
{
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_connect_cb\n");
    if (wifi_get_opmode()== STATIONAP_MODE ) {
        wifi_set_opmode(STATION_MODE);
    }

    user_link_led_timer_done();
    device_recon_count = 0;
    espconn_regist_recvcb(pespconn, user_esp_platform_recv_cb);
    espconn_regist_sentcb(pespconn, user_esp_platform_sent_cb);
    user_esp_platform_sent(pespconn);
}

/******************************************************************************
 * FunctionName : user_esp_platform_connect
 * Description  : The function given as the connect with the host
 * Parameters   : espconn -- the espconn used to connect the connection
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_connect(struct espconn *pespconn)
{
    ESP_DBG("user_esp_platform_connect\n");

#ifdef CLIENT_SSL_ENABLE
    espconn_secure_connect(pespconn);
#else
    espconn_connect(pespconn);
#endif
}

#ifdef USE_DNS
/******************************************************************************
 * FunctionName : user_esp_platform_dns_found
 * Description  : dns found callback
 * Parameters   : name -- pointer to the name that was looked up.
 *                ipaddr -- pointer to an ip_addr_t containing the IP address of
 *                the hostname, or NULL if the name could not be found (or on any
 *                other error).
 *                callback_arg -- a user-specified callback argument passed to
 *                dns_gethostbyname
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;

    if (ipaddr==NULL) {
        ESP_DBG("user_esp_platform_dns_found NULL\n");

        if (++device_recon_count==5) {
            device_status = DEVICE_CONNECT_SERVER_FAIL;

            user_esp_platform_reset_mode();
        }

        return;
    }

    ESP_DBG("user_esp_platform_dns_found %d.%d.%d.%d\n",
            *((uint8 *)&ipaddr->addr), *((uint8 *)&ipaddr->addr + 1),
            *((uint8 *)&ipaddr->addr + 2), *((uint8 *)&ipaddr->addr + 3));

    if (esp_server_ip.addr==0 && ipaddr->addr!=0) {
        os_timer_disarm(&client_timer);
        esp_server_ip.addr = ipaddr->addr;
        os_memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4);

        pespconn->proto.tcp->local_port = espconn_port();

#ifdef CLIENT_SSL_ENABLE
        pespconn->proto.tcp->remote_port = 8443;
#else
        pespconn->proto.tcp->remote_port = 8000;
#endif

        espconn_regist_connectcb(pespconn, user_esp_platform_connect_cb);
        espconn_regist_disconcb(pespconn, user_esp_platform_discon_cb);
        espconn_regist_reconcb(pespconn, user_esp_platform_recon_cb);
        user_esp_platform_connect(pespconn);
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_dns_check_cb
 * Description  : 1s time callback to check dns found
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_dns_check_cb(void *arg)
{
    struct espconn *pespconn = arg;

    ESP_DBG("user_esp_platform_dns_check_cb\n");

    espconn_gethostbyname(pespconn, ESP_DOMAIN, &esp_server_ip, user_esp_platform_dns_found);

    os_timer_arm(&client_timer, 1000, 0);
}

LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_start_dns(struct espconn *pespconn)
{
    esp_server_ip.addr = 0;
    espconn_gethostbyname(pespconn, ESP_DOMAIN, &esp_server_ip, user_esp_platform_dns_found);

    os_timer_disarm(&client_timer);
    os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_dns_check_cb, pespconn);
    os_timer_arm(&client_timer, 1000, 0);
}
#endif

/******************************************************************************
 * FunctionName : user_esp_platform_check_ip
 * Description  : espconn struct parame init when get ip addr
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_check_ip(uint8 reset_flag)
{
    struct ip_info ipconfig;

    os_timer_disarm(&client_timer);

    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (wifi_station_get_connect_status()==STATION_GOT_IP && ipconfig.ip.addr!=0) {
        user_link_led_timer_init();
        user_conn.proto.tcp = &user_tcp;
        user_conn.type = ESPCONN_TCP;
        user_conn.state = ESPCONN_NONE;

        device_status = DEVICE_CONNECTING;

        if (reset_flag) {
            device_recon_count = 0;
        }

#ifdef USE_DNS
        user_esp_platform_start_dns(&user_conn);
#else
        const char esp_server_ip[4] = {114, 215, 177, 97};

        os_memcpy(user_conn.proto.tcp->remote_ip, esp_server_ip, 4);
        user_conn.proto.tcp->local_port = espconn_port();

#ifdef CLIENT_SSL_ENABLE
        user_conn.proto.tcp->remote_port = 8443;
#else
        user_conn.proto.tcp->remote_port = 8000;
#endif

        espconn_regist_connectcb(&user_conn, user_esp_platform_connect_cb);
        espconn_regist_reconcb(&user_conn, user_esp_platform_recon_cb);
        user_esp_platform_connect(&user_conn);
#endif
    } else {
        /* if there are wrong while connecting to some AP, then reset mode */
        if ((wifi_station_get_connect_status()==STATION_WRONG_PASSWORD ||
                wifi_station_get_connect_status()==STATION_NO_AP_FOUND ||
                wifi_station_get_connect_status()==STATION_CONNECT_FAIL)) {
            user_esp_platform_reset_mode();
        } else {
            os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
            os_timer_arm(&client_timer, 100, 0);
        }
    }
}

/******************************************************************************
 * FunctionName : user_esp_platform_init
 * Description  : device parame init based on espressif platform
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_esp_platform_init(void) {

    os_sprintf(iot_version,"%s%d.%d.%dt%d(%s)",VERSION_TYPE,IOT_VERSION_MAJOR,\
    IOT_VERSION_MINOR,IOT_VERSION_REVISION,device_type,UPGRADE_FALG);
    os_printf("IOT VERSION = %s\n",iot_version);

    user_esp_platform_load_param(&esp_param);
    /***add by tzx for saving ip_info to avoid dhcp_client start****/
    struct dhcp_client_info dhcp_info;
    struct ip_info sta_info;
    system_rtc_mem_read(64,&dhcp_info,sizeof(struct dhcp_client_info));
    if(dhcp_info.flag==0x01 ) {
        if (true==wifi_station_dhcpc_status())
        {
            wifi_station_dhcpc_stop();
        }
        sta_info.ip = dhcp_info.ip_addr;
        sta_info.gw = dhcp_info.gw;
        sta_info.netmask = dhcp_info.netmask;
        if ( true!=wifi_set_ip_info(STATION_IF,&sta_info)) {
            os_printf("set default ip wrong\n");
        }
    }
    os_memset(&dhcp_info,0,sizeof(struct dhcp_client_info));
    system_rtc_mem_write(64,&dhcp_info,sizeof(struct rst_info));


#if AP_CACHE
    wifi_station_ap_number_set(AP_CACHE_NUMBER);
#endif

    //if not activated,open softap interface
    if (esp_param.activeflag!=1) {
#ifdef LOW_POWER_MODE
        struct softap_config config_softap;
        char ssid[33]={0};
        wifi_softap_get_config(&config_softap);
        os_memset(config_softap.password, 0, sizeof(config_softap.password));
        os_memset(config_softap.ssid, 0, sizeof(config_softap.ssid));
        os_sprintf(ssid,"MADCOWMOOMOO_SENSOR_%06X",system_get_chip_id());
        os_memcpy(config_softap.ssid, ssid, os_strlen(ssid));
        config_softap.ssid_len = os_strlen(ssid);
        config_softap.authmode = AUTH_OPEN;
        wifi_softap_set_config(&config_softap);
#endif
#ifdef SOFTAP_ENCRYPT
        struct softap_config config;
        char password[33];
        char macaddr[6];
        
        wifi_softap_get_config(&config);
        wifi_get_macaddr(SOFTAP_IF, macaddr);
        os_memset(config.password, 0, sizeof(config.password));
        os_sprintf(password, MACSTR "_%s", MAC2STR(macaddr), PASSWORD);
        os_memcpy(config.password, password, os_strlen(password));
        config.authmode = AUTH_WPA_WPA2_PSK;
        wifi_softap_set_config(&config);
#endif
        wifi_set_opmode(STATIONAP_MODE);
    }
    user_sensor_init(esp_param.activeflag);

    if (wifi_get_opmode()!=SOFTAP_MODE) {
        os_timer_disarm(&client_timer);
        os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_check_ip, 1);
        os_timer_arm(&client_timer, 100, 0);
    }
}

//
//    struct softap_config config_softap;
//    char ssid[33]={0};
//    os_sprintf(ssid,"MADCOWMOOMOO_SENSOR_%06X",system_get_chip_id());
//    char ssid[33]={0};
//    setup_softAP(config_softap, ssid, auth_mode, password);
//
//void setup_softAP(softap_config &config_softap, char* ssid, int auth_mode, char* password) {
//    wifi_softap_get_config(config_softap);
//    // TODO: Add range check to ensure that passwords are not the wrong len and no overflow.
//    // Set password
//    os_memset(config_softap.password, 0, sizeof(config_softap.password));
//    os_memcpy(config_softap.password, password, os_strlen(password));
//    config_softap->password_len = os_strlen(password);
//    // Set SSID
//    os_memset(config_softap.ssid, 0, sizeof(config_softap.ssid));
//    os_memcpy(config_softap.ssid, ssid, os_strlen(ssid));
//    config_softap.ssid_len = os_strlen(ssid);
//    // Set authentication mode
//    config_softap->authmode = auth_mode;
//    wifi_softap_set_config(config_softap);
//}
