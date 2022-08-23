#include "usart.h"
#include "command.h"
#include "string.h"
#include "stdbool.h"
#include "mqtt_esp.h"
#include "smartconfig.h"
#include "esp_sntp.h"
#include <time.h>
#include <sys/time.h>
#include "esp_spi_flash.h"
#include "print_log.h"
#include "esp_partition.h"
#include "ble_dwm1001.h"
#include "nvs_store.h"
#include "buzzer.h"
#include "ws2812b.h"
#define INFO_FM     "\x1B[0m===============================================================\r\n"\
                    "===============================================================\r\n"\
                    "\x1B[31mFw: %d Hw: %d Type: %s Author: DungDA  Build: %s, %s\r\n"\
                    "\x1B[0m===============================================================\r\n"\
                    "===============================================================\r\n"   
#define HARDWARE_TYPE   0
/*  HARDWARE_TYPE = 0   Device
    HARDWARE_TYPE = 1   ESP32_Devkit
    */ 
#if (HARDWARE_TYPE == 0)
    #define HARDWARE_INFO   "Device"
    #define FIRMWARE_VERSION        125
    #define HARDWARE_VERSION        101
#elif (HARDWARE_TYPE == 1)
    #define HARDWARE_INFO "ESP32_Devkit"
    #define FIRMWARE_VERSION        101
    #define HARDWARE_VERSION        201
#endif

// #define MQTT_CONNECT_DEV
#define MODEL   "ESP32-"

#define SMARTCONFIG_MODE_STATUS     60
#define CONFIG_STATUS               120
#define WIFI_CONNECTING_STATUS      180
#define MQTT_CONNECTING_STATUS      240
#define MQTT_CONNECTED_STATUS       300
// #define SCANNING_ANCHOR_STATUS      300
// #define RTLS_RUNNING_STATUS         360

typedef struct {
    char host[64];
    uint32_t port;
    char project[32];
    char registry[32];
    char location[32];
    char *root_ca;
    uint32_t ca_len;
    char *private_key;
    uint32_t prv_key_len;
}rtls_app_param_t;
typedef struct {
    char host[64];
    uint32_t port;
    char user[32];
    char pwd[256];
    char client_id[32];

}mqtt_info_t;

typedef struct{
    uint32_t id;
    uint32_t ts;
    char tc[10];
    char method[32];
    uint16_t signal;
    char product_id[32];
    uint32_t  firmware;
    uint32_t  hardware;
    char mac_addr_str[32];
    mqtt_info_t mqtt_config;
    uint8_t mqtt_connect_ok;
    EventGroupHandle_t mqtt_event_group;
    time_t time_stamp;
    struct tm time_info;
    int8_t rssi;
    esp_reset_reason_t rst_reason;
}device_info_t;

typedef enum{
    GET_STATUS,
    COMMAND,
    OTA_REQUEST,
    REBOOT,
    GET_EXTEND_STATUS,
    GET_LOG,
    FACTORY_RESET,
}mqtt_method_t;

typedef struct{
    dwm1001_node_status status;
    dwm1001_location loc;
    uint8_t slot;
    uint8_t is_online;
    //app_ble_dwm1001_cb_event_t event;
}rtls_node_t;

extern device_info_t device;
extern char request_topic[];
extern char response_topic[];
extern uart_init_t uart;

