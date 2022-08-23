#include "smartconfig.h"
#include "ws2812b.h"
/* FreeRTOS event group to signal when we are connected & ready to make a request */
// static EventGroupHandle_t s_wifi_event_group;
smart_config_t smart_config;
static const char *TAG = "smartconfig";
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static esp_netif_t *esp_netif;
char wifi_hostname[32]={0};

static void smartconfig_task(void * parm);
static void wificonnect_task(void * parm);

static void event_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data);

static void run_smartconfig(void);
static void run_connect_to_current_wifi_config(void);

static uint32_t read_id(void);
void initialise_wifi(void);

static syslog_callback ptrFnc = NULL;
/*  Funtion for smart config on esp
-------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------
*/
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    static bool is_wifi_connected = false;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_netif_set_hostname(esp_netif, (const char*)wifi_hostname);
        esp_wifi_connect();
        led_control(180);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (is_wifi_connected == true){
            if(ptrFnc != NULL) ptrFnc(WIFI_EVENT_DISCONNECTED, NULL);
            is_wifi_connected = false;
        }
        esp_wifi_connect();
        if (smart_config.retry < WIFI_RETRY_CONNECT) {
            smart_config.retry++;
            Logi(TAG, "Retry to connect to  [\"%s\"] [\"%s\"]",smart_config.wifi_config.sta.ssid,
                                                                smart_config.wifi_config.sta.password);
        } 
        else {
            Logi(TAG,"Connect fail");
            smart_config.retry =  0;
            xEventGroupSetBits(smart_config.s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (is_wifi_connected == false){
            if(ptrFnc != NULL) ptrFnc(WIFI_EVENT_CONNECTED, NULL);
            is_wifi_connected = true;
        }
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        Logi(TAG, "IP:" IPSTR, IP2STR(&event->ip_info.ip));
        smart_config.retry = 0;
        xEventGroupSetBits(smart_config.s_wifi_event_group, CONNECTED_BIT);
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        Logi(TAG, "Scan done");
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        Logi(TAG, "Found channel");
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        Logi(TAG, "Got SSID and password");
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        bzero(&smart_config.wifi_config, sizeof(wifi_config_t));
        memcpy(smart_config.wifi_config.sta.ssid, evt->ssid, sizeof(smart_config.wifi_config.sta.ssid));
        memcpy(smart_config.wifi_config.sta.password, evt->password, sizeof(smart_config.wifi_config.sta.password));
        smart_config.wifi_config.sta.bssid_set = evt->bssid_set;
        if (smart_config.wifi_config.sta.bssid_set == true) {
            memcpy(smart_config.wifi_config.sta.bssid, evt->bssid, sizeof(smart_config.wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        Logi(TAG, "SSID:%s", ssid);
        Logi(TAG, "PASSWORD:%s", password);
        esp_wifi_disconnect();
        esp_wifi_set_config(WIFI_IF_STA, &smart_config.wifi_config);
        esp_wifi_connect();
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(smart_config.s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}
void wifi_register_syslog_callback(syslog_callback callback){
    ptrFnc = callback;
}
static uint32_t read_id(void){
    uint8_t tmp[6]={0};
    esp_err_t ret;
    ret = esp_read_mac(tmp,ESP_MAC_WIFI_STA);
    if (ret == ESP_FAIL) { 
        Loge("log","Read MAC address fail.");
    }     
    uint64_t mac_address64 = 0;
    uint64_t chipId = 0;
    for (int i = 0; i < 6; i++){
        mac_address64 |= (uint64_t)tmp[i] << (i*8);
    }
    for(int i = 0; i < 25; i = i + 8) {
        chipId |= ((mac_address64 >> (40 - i)) & 0xff) << i;
    }
    return (uint32_t)chipId; 
}

static void run_smartconfig(void){
    Logi(TAG,"Running Smart config.");
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL);
    esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 12, NULL);  
}
static void run_connect_to_current_wifi_config(void){
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL);
    esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    Logi(TAG,"Connecting to Wifi [\"%s\"] [\"%s\"]",smart_config.wifi_config.sta.ssid,
                                                                    smart_config.wifi_config.sta.password );
    esp_wifi_set_config(WIFI_IF_STA, &smart_config.wifi_config);
    xTaskCreate(wificonnect_task, "wifi_connec_task", 4096, NULL, 12, NULL);   
}
void initialise_wifi(void)
{
    uint8_t ssid_ret = nvs_read_str("SSID",(char*)smart_config.wifi_config.sta.ssid);
    uint8_t pass_ret = nvs_read_str("PASSWORD",(char*)smart_config.wifi_config.sta.password);
    // strcpy((char*)smart_config.wifi_config.sta.ssid, ssid);
    // strcpy((char*)smart_config.wifi_config.sta.password, password);
    smart_config.s_wifi_event_group = xEventGroupCreate();
    esp_event_loop_create_default();
    esp_netif_init();
    esp_netif = esp_netif_create_default_wifi_sta();
    sprintf(wifi_hostname,"RTLS_GW%u",read_id());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    if (ssid_ret == nvs_ok && pass_ret == nvs_ok){
        Logi("nvs_store","Found SSID and PASSWORD save on NVS partition.");
        run_connect_to_current_wifi_config();
    }
    else {
        Loge("nvs_store","No SSID and PASSWORD save on NVS partition.");
        run_smartconfig();
        led_control(60);
    }  
    while(!smart_config.connected && !smart_config.esp_touch_done) vTaskDelay(1);
}

static void smartconfig_task(void * parm)
{
    EventBits_t uxBits;
    esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    esp_smartconfig_start(&cfg);
    while (1){
        uxBits = xEventGroupWaitBits(smart_config.s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT | WIFI_FAIL_BIT, true, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
            nvs_erase_brokerCfg();
            Logi(TAG,"Connected to Wifi [\"%s\"] [\"%s\"]",smart_config.wifi_config.sta.ssid,
                                                                    smart_config.wifi_config.sta.password );
            save_log(CAUSE_APP_CONFIG,VALUE_SUCCESS,EXTRA_SMARTCF,0);
            smart_config.connected = 1;
            nvs_write_str("SSID",(char*)smart_config.wifi_config.sta.ssid);
            nvs_write_str("PASSWORD",(char*)smart_config.wifi_config.sta.password);
        }
        if(uxBits & WIFI_FAIL_BIT){
            save_log(CAUSE_APP_CONFIG,VALUE_FAIL,EXTRA_SMARTCF,0);
            save_log(CAUSE_REBOOT,VALUE_SUCCESS,3,0);
            Loge(TAG,"Can't connect to Wifi [\"%s\"] [\"%s\"]",smart_config.wifi_config.sta.ssid,
                                                                    smart_config.wifi_config.sta.password );
            Loge(TAG,"Wrong SSID and Password, restart ");
            vTaskDelay(1000);
            esp_restart();
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            smart_config.esp_touch_done = 1;
            Logi(TAG, "Smart Config over. Delete task");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}
static void wificonnect_task(void * parm){
    EventBits_t uxBits;
    while(1){
        uxBits = xEventGroupWaitBits(smart_config.s_wifi_event_group, CONNECTED_BIT , true, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
            Logi(TAG,"Connected to Wifi [\"%s\"] [\"%s\"]",smart_config.wifi_config.sta.ssid,
                                                                    smart_config.wifi_config.sta.password );
            smart_config.connected = 1;
            vTaskDelete(NULL);
        }
    }
}
