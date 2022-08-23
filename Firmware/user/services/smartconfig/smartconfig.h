#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"

#include "esp_netif.h"
#include "esp_smartconfig.h"

#include "esp_http_server.h"
#include <sys/param.h>
#include "nvs_store.h"
#include "syslog.h"
#include "print_log.h"

#define CONNECTED_BIT   BIT0
#define ESPTOUCH_DONE_BIT BIT1
#define WIFI_FAIL_BIT   BIT2

#define WIFI_RETRY_CONNECT  20
// #define _USE_SMART_CONFIG 
typedef struct {
    EventGroupHandle_t s_wifi_event_group;
    wifi_config_t wifi_config;
    uint8_t connected;
    uint8_t esp_touch_done;
    uint8_t retry;
}smart_config_t;
typedef void(*syslog_callback)(syslog_id_e, void*);
typedef esp_err_t (*http_handler)(httpd_req_t *r);
void initialise_wifi(void);
void http_server_register_callback(http_handler cbFunc_get, http_handler cbFunc_post);
httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);
void restart_smartconfig();
void wifi_register_syslog_callback(syslog_callback callback);
