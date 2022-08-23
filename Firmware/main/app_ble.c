#include "app_main.h"
#include "ble_dwm1001.h"
#include "cJSON.h"

#define MAX_ANCHOR_SLOT 16
#define MAX_TAG_SLOT 16
#define BLE_TAG "app_ble"

rtls_node_t* anchor_list = NULL;
rtls_node_t* tag_list = NULL;
uint8_t anchor_num = 0;
uint8_t tag_num = 0;
QueueHandle_t uwb_queue = NULL;
rtls_node_t parameter;

static void dwm1001_evt_cb(app_ble_dwm1001_cb_event_t event, app_ble_dwm1001_cb_param_t* param);

static void dwm1001_evt_cb(app_ble_dwm1001_cb_event_t event, app_ble_dwm1001_cb_param_t* param){

    switch (event)
    {
    case (APP_DWM1001_PUSH_LOC_EVT): 
    {
        for (uint8_t idx = 0; idx < tag_num; idx++)
        {
            if (strcmp(tag_list[idx].status.ID, param->get_loc.tag.ID) == 0)
            {
                tag_list[idx].loc = param->get_loc.loc;
                tag_list[idx].event = event;
                parameter = tag_list[idx];
            }
        }
        if (uwb_queue != NULL) xQueueSend(uwb_queue,(void*)&parameter,portMAX_DELAY);
        break;
    }
    case (APP_DWM1001_NEW_DEV_EVT): 
    {
        if (param->new_dev.new_dev.node_type == TAG)
        {
            uint8_t is_node_exist = false;
            for (uint8_t idx = 0; idx < tag_num; idx++)
            {
                if (strcmp(tag_list[idx].status.ID, param->new_dev.new_dev.ID) == 0)
                {
                    is_node_exist = true;
                    
                }
            }
            if (is_node_exist == false)
            {
                tag_list[tag_num].status = param->new_dev.new_dev;
                tag_list[tag_num].slot = tag_num;
                tag_list[tag_num].event = event;
                parameter = tag_list[tag_num];
                tag_num++;
                Logi(BLE_TAG,"APP_DWM1001_NEW_DEV_EVT ");
            }
        }
        else
        if (param->new_dev.new_dev.node_type == ANCHOR)
        {
            uint8_t is_node_exist = false;
            for (uint8_t idx = 0; idx < anchor_num; idx++)
            {
                if (strcmp(anchor_list[idx].status.ID, param->new_dev.new_dev.ID) == 0)
                {
                    is_node_exist = true;
                }
            }
            if (is_node_exist == false)
            {
                anchor_list[anchor_num].status = param->new_dev.new_dev;
                anchor_list[anchor_num].slot = anchor_num;
                anchor_list[anchor_num].event = event;
                parameter = anchor_list[anchor_num];
                anchor_num++;
                Logi(BLE_TAG,"APP_DWM1001_NEW_DEV_EVT ");
                if (anchor_num == 4)
                {
                    ESP_LOGI(BLE_TAG, "enough anchor");
                    ble_dwm1001_disconnect_node(anchor_list[0].status.ID);
                    ble_dwm1001_disconnect_node(anchor_list[1].status.ID);
                    ble_dwm1001_disconnect_node(anchor_list[2].status.ID);
                    ble_dwm1001_disconnect_node(anchor_list[3].status.ID);
                    ble_dwm1001_rtls_set_mode(RTLS_RUNNING);
                    if (ble_dwm1001_get_scan_status() == false)
                    {
                        ble_dwm1001_start_scanning(30);
                    }
                }
            }
        }
        if (uwb_queue != NULL) xQueueSend(uwb_queue,(void*)&parameter,portMAX_DELAY);
        if (ble_dwm1001_get_scan_status() == false)
        {
            ble_dwm1001_start_scanning(30);
        }
        break;
    }
    case (APP_DWM1001_SCAN_ANCHOR_TIMEOUT):
        break;
    case (APP_DWM1001_LOST_DEV_EVT):
    {
        Logi(BLE_TAG, "APP_DWM1001_LOST_DEV_EVT");
        // if (ble_dwm1001_get_scan_status() == false)
        // {
        //     ble_dwm1001_start_scanning(30);
        // }
        break;
    }
    case (APP_DWM1001_ANCHOR_LIST_EVT):
    {
        for (uint8_t idx = 0; idx < param->anchor_list.anchor_list_len; idx++)
        {
            ESP_LOGI(BLE_TAG, "anchor %d: %s", idx, param->anchor_list.anchor_list[idx]);
        }
        for (uint8_t idx = 0; idx < anchor_num; idx++)
        {
            for (uint8_t innerIndex = 0; innerIndex < param->anchor_list.anchor_list_len; innerIndex++)
            {
                //if ()
            }
        }
        if (ble_dwm1001_get_scan_status() == false)
        {
            ble_dwm1001_start_scanning(30);
        }
        break;
    }
    }
}
void app_ble_start(void){
    anchor_list = (rtls_node_t*)malloc(sizeof(rtls_node_t) * MAX_ANCHOR_SLOT);
    if (anchor_list == NULL)
    {
        ESP_LOGE(BLE_TAG, "Fail to create anchor list");
    }
    tag_list = (rtls_node_t*)malloc(sizeof(rtls_node_t) * MAX_TAG_SLOT);
    if (tag_list == NULL)
    {
        ESP_LOGE(BLE_TAG, "Fail to create tag list");
    }
    BLEInit();
    uwb_queue = xQueueCreate(5,sizeof(rtls_node_t));
    ble_dwm1001_register_callback(dwm1001_evt_cb);
}