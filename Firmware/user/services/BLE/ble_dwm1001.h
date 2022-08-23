#ifndef _BLE_DWM1001_H_
#define _BLE_DWM1001_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "print_log.h"

#include "kalman_filter.h"


#define MAX_BLE_CHARACTERISTIC 20
#define MAX_BLE_CONN    CONFIG_BTDM_CTRL_BLE_MAX_CONN
typedef enum {
    TAG = 0,
    ANCHOR, 
    INITIATOR,
}dwm1001_node_type;

typedef struct{
    uint32_t x;     /*!< x cordinate */
    uint32_t y;     /*!< y cordinate*/
    uint32_t z;     /*!< z cordinate*/
    uint8_t qf;     /*!< quality factor (0, 100)*/
}dwm1001_location;

typedef struct {
    char ID[16];                /*!< ID of DW1000 */
//    uint8_t name[6];               /*!< name of DW1000 */
    esp_bd_addr_t blu_addr;     /*!< Bluetooth Address of DWM1001*/
    dwm1001_node_type node_type;/*!< Type of Node: Anchor (1), Tag (0)*/
    uint16_t net_ID;             /*!< Network ID*/
    uint8_t uwb_type;           /*!< WUB Type: off (0), passive (1), active (2)*/
    uint8_t acc_en;             /*!< Accelometer Enable (0, 1)*/
    uint8_t led_en;             /*!< LED Indicator Enable (0, 1)*/
    uint8_t init_en;            /*!< initiator enable, anchor specific (0, 1)*/
    uint8_t lpw_en;             /*!< low power mode enable, tag specific (0, 1)*/
    uint8_t loc_engine_en;      /*!< location engine enable, tag specific (0, 1)*/
    uint8_t operation_mode[2];
    struct tag_prop {
        uint16_t dynamic_upd_rate;/*!< dynamic update rate of tag*/
        uint16_t static_upd_rate;/*!< static update rate of tag*/
        uint16_t notify_handle;
    }tag;

    struct anchor_prop {
        dwm1001_location anchor_loc;/*!< location of anchor*/
    }anchor;

}dwm1001_node_status;

typedef struct {
    char ID[16];
    uint16_t notify_handle;
    dwm1001_location tag_loc;
    bool is_slot_occupied;
    kalmanFilter xFilter;
    kalmanFilter yFilter;
    kalmanFilter zFilter;
}dwm1001_connected_tag;

typedef struct {
    char ID[16];
    dwm1001_location anchor_loc;
    bool is_slot_occupied;
}dwm1001_connected_anchor;


typedef enum {
    APP_DWM1001_PUSH_LOC_EVT = 0,           /*!< When ble get location from DWM1001 Tag, the event comes */
    APP_DWM1001_NEW_DEV_EVT,                /*!< When ble find new device, the event comes */
    APP_DWM1001_LOST_DEV_EVT,               /*!< When ble lost connection with a device, the event comes */
    APP_DWM1001_ANCHOR_LIST_EVT,            /*!< When connect to initiator and get current network's anchor list, the event comes */
    APP_DWM1001_SCAN_ANCHOR_TIMEOUT,        /*!< When a node disconnect in scan anchor period, the event comes, user need to reset scan anchor period amd it's parameter  */
    APP_DWMW1001_RECONNECT_NODE,
}app_ble_dwm1001_cb_event_t;

typedef struct {
    app_ble_dwm1001_cb_event_t event;        
    struct app_ble_get_location {
        uint8_t slot;
        dwm1001_location loc;
        dwm1001_node_status tag;
    }get_loc;

    struct app_ble_new_device {
        uint8_t slot;
        dwm1001_node_status new_dev;
    }new_dev;

    struct app_ble_lost_device {
        char ID[16];
        dwm1001_node_type node_type;
        uint8_t slot;
    }lost_dev;

    struct app_ble_anchor_list {
        uint8_t anchor_list_len;
        char anchor_list[4][16]; /*!< Receivce ID is last 4 char of Node ID and received list is 16 anchor maximum*/
    }anchor_list;

    struct app_ble_reconnect_node {
        uint8_t slot;
        dwm1001_node_status reconnect_node;
    }reconnect_node;

}app_ble_dwm1001_cb_param_t;

typedef enum {
    SCAN_ANCHOR = 0,
    RTLS_RUNNING,
    RTLS_CONFIG,
}rtls_mode;


typedef void(*app_ble_dwm1001_cb_t)(app_ble_dwm1001_cb_param_t* param);
void ble_dwm1001_init(void);
void ble_dwm1001_register_callback(app_ble_dwm1001_cb_t cb);
esp_err_t ble_dwm1001_disconnect_node(char* ID);
void ble_dwm1001_rtls_set_mode(rtls_mode mode);
esp_err_t ble_dwm1001_start_scanning(uint32_t time_scan);
esp_err_t ble_dwm1001_stop_scanning();
bool ble_dwm1001_get_scan_status();
#endif