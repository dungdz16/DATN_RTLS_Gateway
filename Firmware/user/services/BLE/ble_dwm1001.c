#include "ble_dwm1001.h"
#include "freeRTOS/timers.h"
#include "freeRTOS/queue.h"
#include "freeRTOS/event_groups.h"
#include "freeRTOS/semphr.h"
#include "user_button.h"
#include "ble_nvs.h"
#include "driver/timer.h"
#include "buzzer.h"
//#include "esp_bit_defs.h"

#define GATTC_TAG "GATTC"
#define MAX_GATT_CONNECTION      7
#define INVALID_HANDLE   0
#define IS_TAG 0
#define IS_ANCHOR 1
#define IS_INITIATOR 2
#define MAX_NODE 9
#define SCAN_ANCHOR_TIMEOUT 30000

#define DWM1001_CHAR_OPERATION_MODE_CHAR_HANDLE 0x000d
#define DWM1001_CHAR_LOC_DATA_CHAR_HANDLE       0x0010
#define DWM1001_CHAR_LOC_DATA_MODE_CHAR_HANDLE  0x0013
#define DWM1001_CHAR_PERSISTED_LOC_CHAR_HANDLE  0x0016
#define DWM1001_CHAR_UPDATE_RATE_CHAR_HANDLE    0x0019
#define DWM1001_CHAR_NETID_CHAR_HANDLE          0x001c
#define DWM1001_CHAR_DEV_INFO_CHAR_HANDLE       0x001f
#define DWM1001_CHAR_ANCHOR_LIST_CHAR_HANDLE    0x0028
#define DWM1001_CHAR_FW_UPDATE_POLL_CHAR_HANDLE 0x002b
#define DWM1001_CHAR_DISCONNECT_CHAR_HANDLE     0x0031
#define DWM1001_CHAR_STATISTICS_CHAR_HANDLE     0x0037

#define MAX_DWM1001_TAG 10
#define MAX_DWM1001_ANCHOR 4

#define MAX_DWM1001_EVENT_QUEUE_LEN 20

#define DWM1001_GATTC_OPEN_SUCCESS               BIT0
#define DWM1001_GATTC_OPEN_TIMEOUT               BIT1 

#define TIMER_DIVIDER         (8000)  //  Hardware timer clock divider

typedef struct {
    esp_bd_addr_t node_bda_addr;
    esp_ble_addr_type_t node_addr_type;
    dwm1001_node_type node_type;
}dwm1001_node_in_queue_t;

static bool m_is_scanning = false;
static bool m_is_initiator_online = false;
static uint8_t m_num_node_connecting = 0; /*!< The number of nodes is connecting */
static uint8_t m_num_tag_connected = 0;  /*!< The number of tag is connected */
static uint8_t m_num_anchor_connected = 0;  /*!< The number of anchor is connected */
static uint8_t m_num_anchor_disconnected = 0;
static uint8_t m_alloc_tag_list_size = 0;
static uint8_t m_alloc_anchor_list_size = 0;
static uint8_t m_disconnected_anchor_in_scan_anchor = 0;
// static esp_ble_addr_type_t m_test_node_addr_type = 0;
// static esp_bd_addr_t m_test_node_addr = {0xf6, 0x8d, 0x19, 0xbf, 0xc3, 0x34};
//static uint8_t temp_node_name[6]; /*!< Buffer store connecting node and that node is getting information but not enough*/
dwm1001_node_status m_node_connecting_list[MAX_BLE_CONN];
dwm1001_connected_anchor m_connected_anchor_list[MAX_DWM1001_ANCHOR];
dwm1001_connected_tag m_connected_tag_list[MAX_DWM1001_TAG];
dwm1001_node_status m_node_initiator;
rtls_mode m_network_mode = SCAN_ANCHOR;
xTimerHandle ble_scan_timer = NULL;
xTimerHandle ble_scheduler_timer = NULL;
QueueHandle_t dwm1001_event_queue = NULL; 
QueueHandle_t dwm1001_wait_connnect_queue = NULL;
QueueHandle_t dwm1001_connect_queue = NULL;
SemaphoreHandle_t sem_get_tags_pos = NULL;
//EventGroupHandle_t dwm1001_gattc_open_group_bit;

app_ble_dwm1001_cb_t callback = NULL;

//static uint8_t DWM1001_SERVICE_UUID[] = {0xe7, 0x29, 0x13, 0xc2, 0xa1, 0xba, 0x11, 0x9c, 0x1f, 0x4c, 0x46, 0xc9, 0xd9, 0x21, 0x0c, 0x68};
//static uint8_t DWM1001_CHAR_LOC_DATA_UUID[] = {0x37, 0x9a, 0xb8, 0x89, 0xc8, 0x7e, 0x56, 0xab, 0x3d, 0x4b, 0x34, 0xc6, 0xf2, 0xbd, 0x3b, 0x00};/*TAG - Read Only*/
//static uint8_t DWM1001_CHAR_LOC_DATA_MODE_UUID[] = {0xad, 0x0e, 0x1e, 0x52, 0x82, 0x18, 0x6a, 0x99, 0x16, 0x45, 0x97, 0xdf, 0x7e, 0x94, 0x2b, 0xa0};/*TAG - Read/Write*/
//static uint8_t DWM1001_CHAR_LABEL_UUID[] = {0x00, 0x2a};/*ALL - Read/Write*/
//static uint8_t DWM1001_CHAR_OPERATION_MODE_UUID[] = {0x64, 0x89, 0x59, 0x99, 0xc0, 0x9f, 0xe7, 0xb5, 0xb0, 0x46, 0x70, 0x77, 0x88, 0xfd, 0x0a, 0x3f};/*ALL - Read/Write*/
//static uint8_t DWM1001_CHAR_NETID_UUID[] = {0x08, 0x12, 0x99, 0x37, 0x6a, 0x2d, 0x81, 0xa1, 0xbb, 0x45, 0xff, 0x3b, 0xbc, 0xd8, 0xf9, 0x80};/*ALL - Read/Write*/
//static uint8_t DWM1001_CHAR_DEV_INFO_UUID[] = {0x01, 0x25, 0x19, 0x65, 0xe9, 0xc1, 0x54, 0xaf, 0x4e, 0x44, 0xed, 0xd4, 0xeb, 0xb1, 0x63, 0x1e};/*ALL - Read Only*/
//static uint8_t DWM1001_CHAR_STATISTICS_UUID[] = {0xe5, 0x9d, 0xc6, 0x04, 0x02, 0x8a, 0x35, 0x85, 0x1c, 0x4c, 0xf1, 0xba, 0x59, 0xbc, 0xb2, 0x0e};/*ALL - No Information*/
//static uint8_t DWM1001_CHAR_FW_UPDATE_PUSH_UUID[] = {0x2b, 0xc3, 0x9a, 0xc8, 0xfa, 0xbd, 0xa6, 0x8a, 0x30, 0x40, 0x85, 0xe0, 0x10, 0xaa, 0x55, 0x59};/*ALL - Write Only*/
//static uint8_t DWM1001_CHAR_FW_UPDATE_POLL_UUID[] = {0x50, 0xa8, 0xab, 0x1d, 0x44, 0x7c, 0x92, 0xbd, 0x1c, 0x4d, 0xc0, 0x09, 0x27, 0x0e, 0xed, 0x9e};/*ALL - Read Only*/
//static uint8_t DWM1001_CHAR_DISCONNECT_UUID[] = {0x73, 0xe4, 0x80, 0x10, 0x40, 0x8b, 0xdc, 0xa2, 0x0a, 0x4a, 0x03, 0xda, 0x48, 0xb8, 0x83, 0xed};/*ALL- Read Only*/
//static uint8_t DWM1001_CHAR_PERSISTED_LOC_UUID[] = {0x0c, 0xb4, 0xf1, 0xde, 0x03, 0xfe, 0x60, 0xab, 0xac, 0x49, 0x8c, 0x2c, 0x9b, 0x6c, 0xf2, 0xf0};/*Anchor - Write Only*/
//static uint8_t DWM1001_CHAR_ANCHOR_LIST_UUID[] = {0xcb, 0xbc, 0xb6, 0x79, 0xbd, 0x9d, 0xe1, 0xae, 0x6f, 0x48, 0x2f, 0xaf, 0x28, 0xc4, 0x10, 0x5b};/*Anchor - Read Only*/
//static uint8_t DWM1001_CHAR_UPDATE_RATE_UUID[] = {0xb6, 0x08, 0x13, 0x73, 0x05, 0x83, 0x69, 0xb0, 0x89, 0x43, 0x02, 0x56, 0x30, 0x7f, 0xd4, 0x7b};/*TAG - Read Only*/

/* Declare static functions */
static void add_anchor_into_connected_list(dwm1001_node_status *node, uint8_t idx);
static void add_tag_into_connected_list(dwm1001_node_status *node, uint8_t idx);
static void add_node_into_connected_list(dwm1001_node_status *node);
static void delete_node_from_connected_list(dwm1001_node_status node);
static int8_t is_tag_node_exist(char* ID);
static int8_t is_anchor_node_exist(char* ID);
static int8_t is_tag_in_connecting_list(esp_bd_addr_t bda);
static uint8_t app_ble_dwm1001_get_slot_from_id(char* ID);
static void ble_scan_callback(xTimerHandle xTimer);
static void parseOperationMode(uint8_t* message, dwm1001_node_status *result);
static void parseLocation(uint8_t* message, int32_t* x, int32_t* y, int32_t* z);
// static esp_err_t convertUUID128(char* uuid128, uint8_t* result);
// static uint8_t toHex(char c);
// static esp_err_t bda_cmp(esp_bd_addr_t bda1, esp_bd_addr_t bda2);
static esp_err_t IDCmp(char* id1, char* id2);
static esp_err_t bda_cmp(esp_bd_addr_t bda1, esp_bd_addr_t bda2);
uint8_t getEmptySeat();
static esp_err_t parseID(uint8_t* ID, uint8_t ID_len, char* IDstr);
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
void gattc_profile_event_handler(uint8_t slot, esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void timer_init_ms(int group, int timer, bool auto_reload);
//static esp_err_t app_ble_dwm1001_get_node_status(esp_bd_addr_t remote_addr, dwm1001_node_status* result);


static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

struct gattc_profile_inst {
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    bool is_occupied;
    bool get_service;
    esp_bd_addr_t blu_addr;
};


/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[MAX_GATT_CONNECTION];


void gattc_profile_event_handler(uint8_t slot, esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *para)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)para;
    switch (event) {
    case ESP_GATTC_OPEN_EVT:
        if (p_data->open.status != ESP_GATT_OK){
            Loge(GATTC_TAG, "open failed, status %d", p_data->open.status);
            if (m_is_scanning == false)
            {
                ble_dwm1001_start_scanning(30);
            }
            break;
        }
        gl_profile_tab[slot].is_occupied = true;
        gl_profile_tab[slot].conn_id = p_data->open.conn_id;
        memcpy(gl_profile_tab[slot].blu_addr, p_data->open.remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->open.conn_id);
        if (mtu_ret){
            Loge(GATTC_TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    case ESP_GATTC_CFG_MTU_EVT:
    {
        if (p_data->cfg_mtu.status != ESP_GATT_OK){
            Loge(GATTC_TAG,"config mtu failed, error status = %x", p_data->cfg_mtu.status);
        }
        esp_ble_gattc_search_service(gattc_if, p_data->cfg_mtu.conn_id, NULL);
        break;
    }
    case ESP_GATTC_SEARCH_RES_EVT: 
    {
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_128) {
            //Logi(GATTC_TAG, "service found");
            gl_profile_tab[slot].get_service = true;
            gl_profile_tab[slot].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[slot].service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
    {
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            Loge(GATTC_TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if (gl_profile_tab[slot].get_service == true){
            //increase the number of connected nodes
            m_num_node_connecting++;
            //m_node_connecting_list[slot] = 0;
            memset(&m_node_connecting_list[slot], 0, sizeof(dwm1001_node_status));
            // kalman_init(&m_node_connecting_list[slot].xFilter, 2, 2, 0.01);
            // kalman_init(&m_node_connecting_list[slot].yFilter, 2, 2, 0.01);
            // kalman_init(&m_node_connecting_list[slot].zFilter, 2, 2, 0.01);
            esp_ble_gattc_read_char(gattc_if,
                                    p_data->search_cmpl.conn_id,
                                    DWM1001_CHAR_NETID_CHAR_HANDLE,
                                    ESP_GATT_AUTH_REQ_NONE);
        
        }
        break;
    }    
    case ESP_GATTC_READ_CHAR_EVT:
    {
        //Logi(GATTC_TAG, "ESP_GATTC_READ_CHAR_EVT");
        switch(p_data->read.handle)
        {
            case DWM1001_CHAR_LOC_DATA_MODE_CHAR_HANDLE:
            {
                //Logi(GATTC_TAG, "read location mode");
                if (p_data->read.value != 0)
                {
                    uint8_t value = 0;
                    esp_ble_gattc_write_char(gattc_if, 
                                            p_data->read.conn_id,
                                            p_data->read.handle,
                                            1,
                                            &value,
                                            ESP_GATT_WRITE_TYPE_NO_RSP,
                                            ESP_GATT_AUTH_REQ_NONE);
                }
                break;
            }
            case DWM1001_CHAR_OPERATION_MODE_CHAR_HANDLE:
            {
                //Logi(GATTC_TAG, "read operation mode");
                memcpy(m_node_connecting_list[slot].operation_mode, p_data->read.value, 2);
                parseOperationMode(p_data->read.value, &m_node_connecting_list[slot]);
                
                if (m_node_connecting_list[slot].node_type == TAG)
                {
                    //change location data mode of tag and register notify
                    esp_ble_gattc_read_char(gattc_if,
                                            p_data->read.conn_id,
                                            DWM1001_CHAR_LOC_DATA_MODE_CHAR_HANDLE,
                                            ESP_GATT_AUTH_REQ_NONE);
                    esp_ble_gattc_register_for_notify(  gl_profile_tab[slot].gattc_if,
                                                        gl_profile_tab[slot].blu_addr,
                                                        DWM1001_CHAR_LOC_DATA_CHAR_HANDLE );
                    memcpy(m_node_connecting_list[slot].blu_addr, gl_profile_tab[slot].blu_addr, 6);
                    // app_ble_dwm1001_cb_param_t param;
                    // add_node_into_connected_list(&m_node_connecting_list[slot]);
                    // param.new_dev.new_dev = m_node_connecting_list[slot];
                    // param.new_dev.slot = is_tag_node_exist(m_node_connecting_list[slot].ID);
                    // param.event = APP_DWM1001_NEW_DEV_EVT;
                    //xQueueSend(dwm1001_event_queue, &param, 10);
                    
                }
                else
                {
                    //read location data when it's anchor
                    esp_ble_gattc_read_char(gattc_if,
                                            p_data->read.conn_id,
                                            DWM1001_CHAR_LOC_DATA_CHAR_HANDLE,
                                            ESP_GATT_AUTH_REQ_NONE);
                }
                break;
            }
            case DWM1001_CHAR_NETID_CHAR_HANDLE:
            {
                //Logi(GATTC_TAG, "read netID");
                m_node_connecting_list[slot].net_ID = (p_data->read.value[1] * 256) + (p_data->read.value[0]);
                esp_ble_gattc_read_char(gattc_if,
                                        p_data->read.conn_id,
                                        DWM1001_CHAR_DEV_INFO_CHAR_HANDLE,
                                        ESP_GATT_AUTH_REQ_NONE);
                break;
            }
            case DWM1001_CHAR_LOC_DATA_CHAR_HANDLE:
            {
                int32_t x, y, z;
                //ESP_LOG_BUFFER_HEX(GATTC_TAG, p_data->read.value, p_data->read.value_len);
                
                if (m_node_connecting_list[slot].node_type == TAG)
                {
                    if (p_data->read.value_len == 14)
                    {
                        parseLocation(p_data->read.value, &x, &y, &z);
                        
                        if ((x >= 0) && (y >=0) && (z >= 0))
                        {
                            //Logi(GATTC_TAG, "Position: x = %d mm, y = %d mm, z = %d mm, QF: %d", x, y, z, p_data->read.value[13]);
                            int8_t connected_tag_index = is_tag_node_exist(m_node_connecting_list[slot].ID);
                            float x_filter = updateEstimate(&m_connected_tag_list[connected_tag_index].xFilter, x);
                            float y_filter = updateEstimate(&m_connected_tag_list[connected_tag_index].yFilter, y);
                            float z_filter = updateEstimate(&m_connected_tag_list[connected_tag_index].zFilter, z);
                            //Logi(GATTC_TAG, "Position: x = %f mm, y = %f mm, z = %f mm, QF: %d", x_filter, y_filter, z_filter, p_data->read.value[13]);
                            app_ble_dwm1001_cb_param_t param;
                            //app_ble_dwm1001_cb_event_t event;
                            param.get_loc.loc.x = (uint32_t)x_filter;
                            param.get_loc.loc.y = (uint32_t)y_filter;
                            param.get_loc.loc.z = (uint32_t)z_filter;
                            param.get_loc.loc.qf = p_data->read.value[13];
                            param.get_loc.slot = connected_tag_index;
                            param.get_loc.tag = m_node_connecting_list[slot];
                            param.event = APP_DWM1001_PUSH_LOC_EVT;
                            //event = APP_DWM1001_PUSH_LOC_EVT;
                            // if (callback != NULL)
                            // {
                            //     callback(event, &param);
                            // }
                            xQueueSend(dwm1001_event_queue, &param, 10);
                        }
                    }
                }
                else
                {
                    parseLocation(p_data->read.value, &x, &y, &z);
                    m_node_connecting_list[slot].anchor.anchor_loc.x = x;
                    m_node_connecting_list[slot].anchor.anchor_loc.y = y;
                    m_node_connecting_list[slot].anchor.anchor_loc.z = z;
                    m_node_connecting_list[slot].anchor.anchor_loc.qf = p_data->read.value[13];
                    memcpy(m_node_connecting_list[slot].blu_addr, gl_profile_tab[slot].blu_addr, 6);
                    add_node_into_connected_list(&m_node_connecting_list[slot]);
                    app_ble_dwm1001_cb_param_t param;
                    //app_ble_dwm1001_cb_event_t event;
                    param.new_dev.new_dev = m_node_connecting_list[slot];
                    param.new_dev.slot = is_anchor_node_exist(m_node_connecting_list[slot].ID);
                    param.event = APP_DWM1001_NEW_DEV_EVT;
                    buzzer_beep();
                    if (param.new_dev.new_dev.init_en == true)
                    {
                        m_node_initiator = m_node_connecting_list[slot];
                    }
                    //event = APP_DWM1001_NEW_DEV_EVT;
                    xQueueSend(dwm1001_event_queue, &param, 10);
                    //callback(event, &param);
                }
                break;
            }
            case DWM1001_CHAR_DEV_INFO_CHAR_HANDLE:
            {
                //Logi(GATTC_TAG, "read device info");
                //memcpy(m_node_connecting_list[slot].ID, p_data->read.value, 8);
                parseID(p_data->read.value, 8, m_node_connecting_list[slot].ID);
                //ESP_LOG_BUFFER_HEX(GATTC_TAG, m_node_connecting_list[slot].ID, 8);
                if (m_network_mode == SCAN_ANCHOR)
                {
                    if (is_anchor_node_exist(m_node_connecting_list[slot].ID) != -1)
                    {
                        Logi(GATTC_TAG, "Anchor exist");
                        app_ble_dwm1001_cb_param_t param;
                        param.event = APP_DWMW1001_RECONNECT_NODE;
                        param.reconnect_node.reconnect_node = m_node_connecting_list[slot];
                        param.reconnect_node.slot = is_anchor_node_exist(m_node_connecting_list[slot].ID);
                        xQueueSend(dwm1001_event_queue, &param, 10);
                    }
                    else
                    {
                        esp_ble_gattc_read_char(gattc_if,
                                                p_data->read.conn_id,
                                                DWM1001_CHAR_OPERATION_MODE_CHAR_HANDLE,
                                                ESP_GATT_AUTH_REQ_NONE);
                    }
                }
                else
                if (m_network_mode == RTLS_RUNNING)
                {
                    if (m_node_connecting_list[slot].node_type == TAG)
                    {
                        int8_t tag_slot_in_connected_list = is_tag_node_exist(m_node_connecting_list[slot].ID);
                        if (tag_slot_in_connected_list != -1)
                        {
                            uint8_t notify_en = 1;
                            esp_ble_gattc_read_char(gattc_if,
                                                    gl_profile_tab[slot].conn_id,
                                                    DWM1001_CHAR_LOC_DATA_MODE_CHAR_HANDLE,
                                                    ESP_GATT_AUTH_REQ_NONE);
                            // esp_ble_gattc_write_char_descr( gattc_if,
                            //                                 gl_profile_tab[slot].conn_id,
                            //                                 m_connected_tag_list[tag_slot_in_connected_list].notify_handle,
                            //                                 sizeof(notify_en),
                            //                                 (uint8_t *)&notify_en,
                            //                                 ESP_GATT_WRITE_TYPE_NO_RSP,
                            //                                 ESP_GATT_AUTH_REQ_NONE);
                            esp_ble_gattc_register_for_notify(  gl_profile_tab[slot].gattc_if,
                                                                gl_profile_tab[slot].blu_addr,
                                                                DWM1001_CHAR_LOC_DATA_CHAR_HANDLE );
                            ESP_LOGI(GATTC_TAG, "Tag exist");
                            app_ble_dwm1001_cb_param_t param;
                            param.event = APP_DWMW1001_RECONNECT_NODE;
                            param.reconnect_node.reconnect_node = m_node_connecting_list[slot];
                            param.reconnect_node.slot = is_tag_node_exist(m_node_connecting_list[slot].ID);
                            xQueueSend(dwm1001_event_queue, &param, 10);
                        }
                        else
                        {
                            esp_ble_gattc_read_char(gattc_if,
                                                    p_data->read.conn_id,
                                                    DWM1001_CHAR_OPERATION_MODE_CHAR_HANDLE,
                                                    ESP_GATT_AUTH_REQ_NONE);
                        }
                    }
                    else
                    {
                        if (is_anchor_node_exist(m_node_connecting_list[slot].ID))
                        {
                            esp_ble_gattc_read_char(gattc_if,
                                                    p_data->read.conn_id,
                                                    DWM1001_CHAR_ANCHOR_LIST_CHAR_HANDLE,
                                                    ESP_GATT_AUTH_REQ_NONE);
                        }
                        else
                        {
                            
                        }
                    }
                }
                break;
            }
            case DWM1001_CHAR_ANCHOR_LIST_CHAR_HANDLE:
            {
                //Logi(GATTC_TAG, "read anchor list: %d", p_data->read.value[0]);
                m_is_initiator_online = true;
                app_ble_dwm1001_cb_param_t param;
                //app_ble_dwm1001_cb_event_t event;
                //event = APP_DWM1001_ANCHOR_LIST_EVT;
                param.anchor_list.anchor_list_len = p_data->read.value[0];
                param.event = APP_DWM1001_ANCHOR_LIST_EVT;
                for (uint8_t idx = 0; idx < param.anchor_list.anchor_list_len; idx++)
                {
                    uint8_t ID[2];
                    char parsed_ID[4];
                    ID[0] = p_data->read.value[1 + idx * 2];
                    ID[1] = p_data->read.value[2 + idx * 2];
                    parseID(ID, 2, parsed_ID);
                    memcpy(param.anchor_list.anchor_list[idx], parsed_ID, sizeof(parsed_ID));
                    param.anchor_list.anchor_list[idx][4] = '\0';
                }
                //callback(event, &param);
                xQueueSend(dwm1001_event_queue, &param, 10);
                ble_dwm1001_disconnect_node(m_node_initiator.ID);
                break;
            }
            default:
                break;
        }
        break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: 
    {
        //Logi(GATTC_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            Loge(GATTC_TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
        }else{
            uint16_t count = 1;
            uint16_t notify_en = 1;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                         gl_profile_tab[slot].conn_id,
                                                                         ESP_GATT_DB_DESCRIPTOR,
                                                                         gl_profile_tab[slot].service_start_handle,
                                                                         gl_profile_tab[slot].service_end_handle,
                                                                         DWM1001_CHAR_LOC_DATA_CHAR_HANDLE,
                                                                         &count);
            if (ret_status != ESP_GATT_OK){
                Loge(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
            }
            if (count > 0){
                //Logi(GATTC_TAG, "count_desc: %d", count);
                esp_gattc_descr_elem_t *descr_elem_result = NULL;
                descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result){
                    Loge(GATTC_TAG, "malloc error, gattc no mem");
                }else{
                    ret_status = esp_ble_gattc_get_all_descr( gattc_if,
                                                            gl_profile_tab[slot].conn_id,
                                                            p_data->reg_for_notify.handle,
                                                            descr_elem_result,
                                                            &count, 
                                                            0);                                                  
                    if (ret_status != ESP_GATT_OK){
                        Loge(GATTC_TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                    }
                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                    if (count > 0){
                        ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                     gl_profile_tab[slot].conn_id,
                                                                     descr_elem_result[0].handle,
                                                                     sizeof(notify_en),
                                                                     (uint8_t *)&notify_en,
                                                                     ESP_GATT_WRITE_TYPE_NO_RSP,
                                                                     ESP_GATT_AUTH_REQ_NONE);
                        if (is_tag_node_exist(m_node_connecting_list[slot].ID) == (- 1))
                        {
                            m_node_connecting_list[slot].tag.notify_handle = descr_elem_result[0].handle;
                            app_ble_dwm1001_cb_param_t param;
                            add_node_into_connected_list(&m_node_connecting_list[slot]);
                            param.new_dev.new_dev = m_node_connecting_list[slot];
                            param.new_dev.slot = is_tag_node_exist(m_node_connecting_list[slot].ID);
                            param.event = APP_DWM1001_NEW_DEV_EVT;
                            xQueueSend(dwm1001_event_queue, &param, 10);
                        }  
                    }
                    if (ret_status != ESP_GATT_OK){
                        Loge(GATTC_TAG, "esp_ble_gattc_write_char_descr error");
                    }

                    /* free descr_elem_result */
                    free(descr_elem_result);
                }
            }
            else{
                Loge(GATTC_TAG, "decsr not found: %s", __func__);
            }

        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
    {
        if ((p_data->notify.value[13] > 50) && (p_data->notify.value[13] < 100))
        {
            int32_t x, y, z;
            //ESP_LOG_BUFFER_HEX(GATTC_TAG, p_data->notify.value, p_data->notify.value_len);
            parseLocation(p_data->notify.value, &x, &y, &z);
            // x = abs(x);
            // y = abs(y);
            // z = abs(z);
            if ((x >= 0) && (y >= 0))
            {
                if (z < 0) z = -z;
                int8_t connected_tag_index = is_tag_node_exist(m_node_connecting_list[slot].ID);
                float x_filter = updateEstimate(&m_connected_tag_list[slot].xFilter, x);
                float y_filter = updateEstimate(&m_connected_tag_list[slot].yFilter, y);
                float z_filter = updateEstimate(&m_connected_tag_list[slot].zFilter, z);
                //Logi(GATTC_TAG, "Position: x = %f mm, y = %f mm, z = %f mm, QF: %d", x_filter, y_filter, z_filter, p_data->notify.value[13]);
                app_ble_dwm1001_cb_param_t param;
                //app_ble_dwm1001_cb_event_t event;
                param.event = APP_DWM1001_PUSH_LOC_EVT;
                param.get_loc.loc.x = (uint32_t)x_filter;
                param.get_loc.loc.y = (uint32_t)y_filter;
                param.get_loc.loc.z = (uint32_t)z_filter;
                param.get_loc.loc.qf = p_data->notify.value[13];
                param.get_loc.tag = m_node_connecting_list[slot];
                param.get_loc.slot = is_tag_node_exist(m_node_connecting_list[slot].ID);
                xQueueSend(dwm1001_event_queue, &param, 10);
            }
            //event = APP_DWM1001_PUSH_LOC_EVT;
            // if (callback != NULL)
            // {
            //     callback(event, &param);
            // }

        }
        break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
    {
        if (p_data->write.status != ESP_GATT_OK){
            Loge(GATTC_TAG, "write descr failed, error status = %s", __func__);
            break;
        }
        //Logi(GATTC_TAG, "write descr success ");
        break;
    }
    case ESP_GATTC_SRVC_CHG_EVT: 
    {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        Logi(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(GATTC_TAG, bda, sizeof(esp_bd_addr_t));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
    {
        if (p_data->write.status != ESP_GATT_OK){
            Loge(GATTC_TAG, "write char failed, error status = %x", p_data->write.status);
            break;
        }
        //Logi(GATTC_TAG, "write char success ");
        break;
    }
    case ESP_GATTC_CLOSE_EVT:
    {
        //decrease the number of connected nodes
        if (m_num_node_connecting > 0)
        {
            m_num_node_connecting--;
        }
        gl_profile_tab[slot].get_service = false;
        gl_profile_tab[slot].is_occupied = false;
        app_ble_dwm1001_cb_param_t param;
        //app_ble_dwm1001_cb_event_t event;
        memcpy(param.lost_dev.ID, m_node_connecting_list[slot].ID, sizeof(m_node_connecting_list[slot].ID));
        param.lost_dev.node_type = m_node_connecting_list[slot].node_type;
        if (m_node_connecting_list[slot].node_type == ANCHOR)
        {
            param.lost_dev.slot = is_anchor_node_exist(m_node_connecting_list[slot].ID);
        }
        else
        if (m_node_connecting_list[slot].node_type == TAG)
        {
            param.lost_dev.slot = is_tag_node_exist(m_node_connecting_list[slot].ID);
        }
        param.event = APP_DWM1001_LOST_DEV_EVT;
        xQueueSend(dwm1001_event_queue, &param, 10);
        //callback(event, &param);
        if ((m_node_connecting_list[slot].node_type == ANCHOR) && (m_network_mode == RTLS_RUNNING))
        {
            m_is_initiator_online = false;
        }
        // if (m_network_mode == RTLS_CONFIG)
        // {
        //     app_ble_dwm1001_cb_event_t event;
        //     app_ble_dwm1001_cb_param_t param;
        //     event = APP_DWM1001_SCAN_ANCHOR_TIMEOUT;
        //     callback(event, &param);
        // }
        break;
    }
    default:
        break;
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_service_data = NULL;
    uint8_t adv_service_data_len = 0;
    uint8_t *node_name = NULL;
    uint8_t node_name_len = 0;
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        //the unit of the duration is second
        uint32_t duration = 30;
        esp_err_t err;
        err = ble_dwm1001_start_scanning(duration);
        if (err != ESP_OK)
        {
            Loge(GATTC_TAG, "Fail to start scanning");
        }
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            Loge(GATTC_TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
            break;
        }
        Logi(GATTC_TAG, "scan start success");
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            adv_service_data = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_128SERVICE_DATA, &adv_service_data_len);

            // if (adv_service_data_len == 0)
            // {
            //     Loge(GATTC_TAG, "None of 128-bit service data");
            //     return;
            // }
            // node_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
            //                                     ESP_BLE_AD_TYPE_NAME_SHORT, &node_name_len);
            // if (node_name_len == 0)
            // {
            //     Loge(GATTC_TAG, "Node doesn't have name");
            //     return;
            // }
            if (adv_service_data_len == 18)
            {
                switch (m_network_mode)
                {
                    case SCAN_ANCHOR:
                    {
                        if ((((adv_service_data[16]) & 0x80) >> 7) == IS_ANCHOR)
                        {
                            esp_err_t err = ble_dwm1001_stop_scanning();
                            if (err != ESP_OK)
                            {
                                Loge(GATTC_TAG, "Fail to stop scanning");
                            }
                                
                            uint8_t gattEmptySeat = getEmptySeat();
                            if (gattEmptySeat == MAX_GATT_CONNECTION + 1)
                            {
                                Loge(GATTC_TAG, "Out of BLE slot");
                            }
                            else
                            {
                                //memcpy(temp_node_name, node_name, node_name_len);
                                //Logi(GATTC_TAG, "Node name: %s", node_name);
                                esp_ble_gattc_open( gl_profile_tab[gattEmptySeat].gattc_if, 
                                                    scan_result->scan_rst.bda, 
                                                    scan_result->scan_rst.ble_addr_type, 
                                                    true);
                                // memcpy(m_test_node_addr, scan_result->scan_rst.bda, sizeof(m_test_node_addr));
                                // m_test_node_addr_type = scan_result->scan_rst.ble_addr_type;
                                // ESP_LOG_BUFFER_HEX(GATTC_TAG, m_test_node_addr, 6);
                                //Logi(GATTC_TAG, "%s %d", m_test_node_addr, m_test_node_addr_type);
                            }
                        }
                        break;
                    }
                    case RTLS_RUNNING:
                    {
                        if (((((adv_service_data[16]) & 0x80) >> 7) == IS_TAG) || (((adv_service_data[16] & 0x0C) >> 2) == IS_INITIATOR))
                        //if (((((adv_service_data[16]) & 0x80) >> 7) == IS_TAG))
                        {
                            dwm1001_node_in_queue_t wait_connect_node; 
                            dwm1001_node_in_queue_t connect_node;
                            // if (xQueueReceive(dwm1001_connect_queue, &connect_node, 10) == pdTRUE)
                            // {
                            //     if (bda_cmp(scan_result->scan_rst.bda, connect_node.node_bda_addr) == ESP_OK)
                            //     {
                            //         ESP_LOG_BUFFER_HEX(GATTC_TAG, scan_result->scan_rst.bda, sizeof(esp_bd_addr_t));
                            //         uint8_t empty_seat = getEmptySeat();
                            //         m_node_connecting_list[empty_seat].node_type = connect_node.node_type;
                            //         ble_dwm1001_stop_scanning();
                            //         esp_ble_gattc_open( gl_profile_tab[empty_seat].gattc_if,
                            //                             scan_result->scan_rst.bda,
                            //                             scan_result->scan_rst.ble_addr_type,
                            //                             true );
                            //     }
                            // }
                            // else
                            // {
                                // if ((((adv_service_data[16]) & 0x80) >> 7) == IS_TAG)
                                // {
                                //     wait_connect_node.node_type = TAG;
                                // }
                                // else
                                // {
                                //     wait_connect_node.node_type = INITIATOR;
                                // }
                                // memcpy(wait_connect_node.node_bda_addr, scan_result->scan_rst.bda, sizeof(esp_bd_addr_t));
                                // wait_connect_node.node_addr_type = scan_result->scan_rst.ble_addr_type;
                                // xQueueSend(dwm1001_wait_connnect_queue, &wait_connect_node, 0);
                            //}
                            uint8_t gattEmptySeat = getEmptySeat();
                            if (gattEmptySeat == MAX_GATT_CONNECTION + 1)
                            {
                                Loge(GATTC_TAG, "Out of BLE slot");
                            }
                            else
                            {   
                                if (m_num_node_connecting < 7)
                                //if (m_num_node_connecting < (6 + m_is_initiator_online))
                                {
                                    esp_err_t err = ble_dwm1001_stop_scanning();
                                    
                                    if (err != ESP_OK)
                                    {
                                        Loge(GATTC_TAG, "Fail to stop scanning");
                                    }
                                    
                                    esp_ble_gattc_open( gl_profile_tab[gattEmptySeat].gattc_if, 
                                                        scan_result->scan_rst.bda, 
                                                        scan_result->scan_rst.ble_addr_type, 
                                                        true);

                                }
                                else
                                {
                                    Logi(GATTC_TAG, "full slot of tag: %d", m_num_node_connecting);
                                }
                            }
                        }
                        break;
                    }
                    case RTLS_CONFIG:
                        break;
                }
                
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            Loge(GATTC_TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
            break;
        }
        Logi(GATTC_TAG, "stop scan successfully");
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            Loge(GATTC_TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
            break;
        }
        Logi(GATTC_TAG, "stop adv successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         Logi(GATTC_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
            Logi(GATTC_TAG, "reg for %d: %d", param->reg.app_id, gattc_if);
        } else {
            Logi(GATTC_TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }
    
    
    int idx;
    for (idx = 0; idx < MAX_GATT_CONNECTION; idx++) {
        if (gattc_if == gl_profile_tab[idx].gattc_if) {
            //Logi(GATTC_TAG, "handler %d", idx);
            gattc_profile_event_handler(idx, event, gattc_if, param);
        }
    }

}

void dwm1001_evt_handler_task(void* pvParameter)
{
    while(true)
    {
        app_ble_dwm1001_cb_param_t param;
        if (xQueueReceive(dwm1001_event_queue, &param, 10) == pdTRUE)
        {
            switch (param.event)
            {
                case (APP_DWM1001_PUSH_LOC_EVT):
                {
                    Logi(GATTC_TAG, "tag %d pos: %d %d %d", param.get_loc.slot,
                                                            param.get_loc.loc.x, 
                                                            param.get_loc.loc.y,
                                                            param.get_loc.loc.z);
                    Logi(GATTC_TAG, "APP_DWM1001_PUSH_LOC_EVT");
                    break;
                }
                case (APP_DWM1001_NEW_DEV_EVT):
                {
                    if (m_is_scanning == false)
                    {
                        ble_dwm1001_start_scanning(0xFFFF);
                    }
                    if (m_network_mode == SCAN_ANCHOR)
                    {
                        // Logi(GATTC_TAG, "anchor pos: %d %d %d", param.new_dev.new_dev.anchor.anchor_loc.x, 
                        //                                             param.new_dev.new_dev.anchor.anchor_loc.y,
                        //                                             param.new_dev.new_dev.anchor.anchor_loc.z);
                        if (m_alloc_anchor_list_size == 4)
                        {
                            ble_dwm1001_disconnect_node(m_connected_anchor_list[0].ID);
                            ble_dwm1001_disconnect_node(m_connected_anchor_list[1].ID);
                            ble_dwm1001_disconnect_node(m_connected_anchor_list[2].ID);
                            ble_dwm1001_disconnect_node(m_connected_anchor_list[3].ID);
                            if (m_is_scanning == true)
                            {
                                ble_dwm1001_stop_scanning();
                            }
                        }
                    }
                    Logi(GATTC_TAG, "APP_DWM1001_NEW_DEV_EVT");
                    break;
                }
                case (APP_DWM1001_LOST_DEV_EVT):
                {
                    // if (m_is_scanning == false)
                    // {
                    //     ble_dwm1001_start_scanning(30);
                    // }
                    Logi(GATTC_TAG, "APP_DWM1001_LOST_DEV_EVT: %d %02d", param.lost_dev.node_type, param.lost_dev.slot);
                    if (m_network_mode == SCAN_ANCHOR)
                    {
                        m_disconnected_anchor_in_scan_anchor++;
                        if (m_disconnected_anchor_in_scan_anchor == 4)
                        {
                            m_network_mode = RTLS_RUNNING;
                            Logi(GATTC_TAG, "RTLS RUNNING");
                            vTaskDelay(pdMS_TO_TICKS(1000));
                            ble_dwm1001_start_scanning(0xFFFF);
                            buzzer_long_beep();
                            //timer_start(TIMER_GROUP_0, TIMER_0);
                        }
                    }
                    if (m_is_scanning == false)
                    {
                        ble_dwm1001_start_scanning(30);
                    }
                    
                    break;
                }
                case (APP_DWM1001_ANCHOR_LIST_EVT):
                {
                    Logi(GATTC_TAG, "APP_DWM1001_ANCHOR_LIST_EVT");
                    Logi(GATTC_TAG, "%s %s %s %s", param.anchor_list.anchor_list[0], param.anchor_list.anchor_list[1], param.anchor_list.anchor_list[2], param.anchor_list.anchor_list[3]);
                    
                    break;
                }
                case (APP_DWM1001_SCAN_ANCHOR_TIMEOUT):
                {
                    Logi(GATTC_TAG, "APP_DWM1001_SCAN_ANCHOR_TIMEOUT");
                    break;
                }
                case (APP_DWMW1001_RECONNECT_NODE):
                {
                    Logi(GATTC_TAG, "APP_DWMW1001_RECONNECT_NODE");
                    if (m_is_scanning == false)
                    {
                        ble_dwm1001_start_scanning(0xFFFF);
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }   
            callback(&param);
        }
    }
}

void rtls_manage_task(void *pvParameter)
{
    dwm1001_node_in_queue_t wait_connect_node;
    EventBits_t gattc_open_bits;
    while(1)
    {
        // if (xQueueReceive(dwm1001_wait_connnect_queue, &wait_connect_node, portMAX_DELAY) == pdTRUE)
        // {
        //     if (m_num_node_connecting < MAX_BLE_CONN)
        //     {
        //         xQueueSend(dwm1001_connect_queue, &wait_connect_node, 10);
        //         //ESP_LOGI(GATTC_TAG, "new node: %d", wait_connect_node.node_type);
        //         uint8_t empty_seat = getEmptySeat();
        //         m_node_connecting_list[empty_seat].node_type = wait_connect_node.node_type;
        //         // ESP_LOGI(GATTC_TAG, "%d", empty_seat);
        //         ESP_LOG_BUFFER_HEX(GATTC_TAG, wait_connect_node.node_bda_addr, sizeof(esp_bd_addr_t));
        //         ble_dwm1001_stop_scanning();
        //         esp_ble_gattc_open(gl_profile_tab[empty_seat].gattc_if,
        //                             wait_connect_node.node_bda_addr,
        //                             wait_connect_node.node_addr_type,
        //                             true);
        //         // gattc_open_bits = xEventGroupWaitBits(dwm1001_gattc_open_group_bit, 
        //         //                                       DWM1001_GATTC_OPEN_SUCCESS | DWM1001_GATTC_OPEN_TIMEOUT,
        //         //                                       true, 
        //         //                                       false,
        //         //                                       portMAX_DELAY);
        //         // if (gattc_open_bits & DWM1001_GATTC_OPEN_SUCCESS)
        //         // {
        //         //     ESP_LOGI(GATTC_TAG, "open success");
        //         // }
        //         // if (gattc_open_bits & DWM1001_GATTC_OPEN_TIMEOUT)
        //         // {
        //         //     ESP_LOGI(GATTC_TAG, "open timeout");
        //         // }
        //     }
        //     else
        //     {
                
        //     }
        // }
        // if (xSemaphoreTake(sem_get_tags_pos, portMAX_DELAY) == pdTRUE)
        // {
        //     for (uint8_t idx = 0; idx < MAX_GATT_CONNECTION; idx++)
        //     {
        //         if (gl_profile_tab[idx].is_occupied == true)
        //         {
        //             //ESP_LOGI(GATTC_TAG, "hello %d", idx);
        //             esp_ble_gattc_read_char(gl_profile_tab[idx].gattc_if,
        //                                     gl_profile_tab[idx].conn_id,
        //                                     DWM1001_CHAR_LOC_DATA_CHAR_HANDLE,
        //                                     ESP_GATT_AUTH_REQ_NONE);
        //         }
        //     }
        // }
    }
}

esp_err_t restore_uwb_info()
{
    uint8_t num_tag = 0;
    uint8_t num_anchor = 0;
    esp_err_t err = read_network_info(&num_tag, & num_anchor);
    ESP_LOGI(GATTC_TAG, "there are %d tags, %d anchors", num_tag, num_anchor);
    if ((num_tag == 0) && (num_anchor == 0))
    {
        return ESP_FAIL;
    }
    if (num_tag)
    {
        for (uint8_t idx = 0; idx < num_tag; idx++)
        {
            dwm1001_node_status node;
            uint8_t operation_data[2];
            esp_bd_addr_t bda;
            uint8_t err = 0;
            char node_id[16];

            if (read_node_operation(operation_data, idx + 1, TAG) == ESP_FAIL)
            {
                ESP_LOGE("restore uwb", "fail to get tag %d operation data", idx + 1);
                err++;
            }
            else
            {
                parseOperationMode(operation_data, &node);
            }
            if (read_node_bda(bda, idx + 1, TAG) == ESP_FAIL)
            {
                ESP_LOGE("restore uwb", "fail to get tag %d bda", idx + 1);
                err++;
            }
            else
            {
                memcpy(node.blu_addr, bda, sizeof(esp_bd_addr_t));
            }
            if (read_node_id(node_id, idx + 1, TAG))
            {
                ESP_LOGE("restore uwb", "fail to get tag %d id", idx + 1);
                err++;
            }
            else
            {
                memcpy(node.ID, node_id, 16);
            }
            if (err == 0)
            {
                add_tag_into_connected_list(&node, idx);
            }
        }
    }
    if (num_anchor == 4)
    {
        for (uint8_t idx = 0; idx < num_anchor; idx++)
        {
            dwm1001_node_status node;
            uint8_t operation_data[2];
            esp_bd_addr_t bda;
            uint8_t err = 0;
            char node_id[16];
            if (read_node_operation(operation_data, idx + 1, ANCHOR) == ESP_FAIL)
            {
                ESP_LOGE("restore uwb", "fail to get anchor %d operation data", idx + 1);
                err++;
                break;
            }
            else
            {
                parseOperationMode(operation_data, &node);
            }
            if (read_node_bda(bda, idx + 1, ANCHOR) == ESP_FAIL)
            {
                ESP_LOGE("restore uwb", "fail to get anchor %d bda", idx + 1);
                err++;
            }
            else
            {
                memcpy(node.blu_addr, bda, sizeof(esp_bd_addr_t));
            }
            if (read_node_id(node_id, idx + 1, ANCHOR))
            {
                ESP_LOGE("restore uwb", "fail to get anchor %d id", idx + 1);
                err++;
            }
            else
            {
                memcpy(node.ID, node_id, 16);
            }
            if (err == 0)
            {
                add_anchor_into_connected_list(&node, idx);
            }
        }
        if (m_num_anchor_connected == 4)
        {
            ESP_LOGI(GATTC_TAG, "RTLS_RUNNING");
            ble_dwm1001_rtls_set_mode(RTLS_RUNNING);
            buzzer_long_beep();
            //timer_start(TIMER_GROUP_0, TIMER_0);
        }
        else
        {
            ESP_LOGE(GATTC_TAG, "Not enough 4 anchor");
            memset(m_connected_anchor_list, 0, sizeof(m_connected_anchor_list));
            m_num_anchor_connected = 0;
            m_alloc_anchor_list_size = 0;
            ble_dwm1001_rtls_set_mode(SCAN_ANCHOR);
        }
    }
    return ESP_OK;
}

void ble_dwm1001_init(void)
{
    //create ble node list
    //m_node_connecting_list = (dwm1001_node_status*)malloc(sizeof(dwm1001_node_status) * MAX_NODE);
    timer_init_ms(TIMER_GROUP_0, TIMER_0, true);
        sem_get_tags_pos = xSemaphoreCreateBinary();
    memset(m_connected_anchor_list, 0, sizeof(m_connected_anchor_list));
    memset(m_connected_tag_list, 0, sizeof(m_connected_tag_list));
    for (uint8_t idx = 0; idx < MAX_DWM1001_TAG; idx++)
    {
        m_connected_tag_list[idx].is_slot_occupied = false;
        kalman_init(&m_connected_tag_list[idx].xFilter, 0.5, 0.1, 0.2);
        kalman_init(&m_connected_tag_list[idx].yFilter, 0.5, 0.1, 0.2);
        kalman_init(&m_connected_tag_list[idx].zFilter, 0.5, 0.1, 0.2);
    }
    for (uint8_t idx = 0; idx < MAX_DWM1001_ANCHOR; idx++)
    {
        m_connected_anchor_list[idx].is_slot_occupied = false;
    }
    restore_uwb_info();
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        Loge(GATTC_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        Loge(GATTC_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        Loge(GATTC_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        Loge(GATTC_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    //register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret){
        Loge(GATTC_TAG, "%s gap register failed, error code = %x\n", __func__, ret);
        return;
    }

    //register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if(ret){
        Loge(GATTC_TAG, "%s gattc register failed, error code = %x\n", __func__, ret);
        return;
    }
    for (uint8_t index = 0; index < MAX_GATT_CONNECTION; index++)
    {
        gl_profile_tab[index].is_occupied = false;
        gl_profile_tab[index].get_service = false;
        ret = esp_ble_gattc_app_register(index);
        if (ret){
            Loge(GATTC_TAG, "%s gattc app register failed, error code = %x\n", __func__, ret);
        }
    }
    esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (scan_ret){
        Loge(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
    }
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(200);
    if (local_mtu_ret){
        Loge(GATTC_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    dwm1001_event_queue = xQueueCreate(MAX_DWM1001_EVENT_QUEUE_LEN, sizeof(app_ble_dwm1001_cb_param_t));
    dwm1001_wait_connnect_queue = xQueueCreate(MAX_DWM1001_TAG - 7, sizeof(dwm1001_node_in_queue_t));
    dwm1001_connect_queue = xQueueCreate(MAX_DWM1001_TAG - 7, sizeof(dwm1001_node_in_queue_t));
    if (dwm1001_event_queue == NULL)
    {
        ESP_LOGE(GATTC_TAG, "Create event queue fail");
    }
    if (dwm1001_wait_connnect_queue == NULL)
    {
        ESP_LOGE(GATTC_TAG, "Create wait connect node queue fail");
    }
    //dwm1001_gattc_open_group_bit = xEventGroupCreate();
    xTaskCreate(dwm1001_evt_handler_task, 
                "dwm1001 event handler",
                1024 * 4,
                NULL,
                12,
                NULL);
    // xTaskCreate(rtls_manage_task, 
    //             "rtls manage task",
    //             1024 * 3,
    //             NULL,
    //             12,
    //             NULL);
    // button_handle_t button_handle = iot_button_create(GPIO_NUM_0, BUTTON_ACTIVE_LOW);
    // iot_button_add_custom_cb(button_handle, 1, button_cb_1s, NULL);
}

void ble_dwm1001_rtls_set_mode(rtls_mode mode)
{
    m_network_mode = mode;
}

esp_err_t ble_dwm1001_start_scanning(uint32_t time_scan)
{
    ble_scan_timer = xTimerCreate("BLE Scan Timer",
                                  pdMS_TO_TICKS((time_scan + 1) * 1000),
                                  false,
                                  NULL,
                                  ble_scan_callback);
    if (ble_scan_timer == NULL)
    {
        Loge(GATTC_TAG, "Fail to create ble_scan_timer");
    }
    else
    {
        esp_err_t err = esp_ble_gap_start_scanning(time_scan);
        if (err != ESP_OK)
        {
            return err;
        }
        m_is_scanning = true;
        xTimerStart(ble_scan_timer, 0);
    }
    return ESP_OK;
}

esp_err_t ble_dwm1001_stop_scanning()
{
    if (m_is_scanning == true)
    {
        esp_err_t err = esp_ble_gap_stop_scanning();
        if (err != ESP_OK)
        {
            return err;
        }
        m_is_scanning = false;
        xTimerStop(ble_scan_timer, 0);
        return ESP_OK;
    }
    return ESP_FAIL;
}

bool ble_dwm1001_get_scan_status()
{
    return m_is_scanning;
}
static void ble_scan_callback(xTimerHandle xTimer)
{
    Logi(GATTC_TAG, "Scan Timeout");
    m_is_scanning = false;
    ble_dwm1001_start_scanning(30);
}

esp_err_t ble_dwm1001_disconnect_node(char* ID)
{
    uint8_t slot = app_ble_dwm1001_get_slot_from_id(ID);
    if (slot == MAX_GATT_CONNECTION)
    {
        Loge(GATTC_TAG, "Fail to find node at ID: %s", ID);
        return ESP_FAIL;
    }
    uint8_t disconnect = 1;
    esp_err_t err =    esp_ble_gattc_write_char(gl_profile_tab[slot].gattc_if, 
                                                gl_profile_tab[slot].conn_id,
                                                DWM1001_CHAR_DISCONNECT_CHAR_HANDLE,
                                                sizeof(disconnect),
                                                (uint8_t*)&disconnect,
                                                ESP_GATT_WRITE_TYPE_NO_RSP,
                                                ESP_GATT_AUTH_REQ_NONE);
    return err;
}

void ble_dwm1001_register_callback(app_ble_dwm1001_cb_t cb)
{
    callback = cb;
}
static void add_anchor_into_connected_list(dwm1001_node_status *node, uint8_t idx)
{
    Logi(GATTC_TAG, "Add anchor node into connected list");
    memcpy(m_connected_anchor_list[idx].ID, node->ID, sizeof(node->ID));
    m_connected_anchor_list[idx].is_slot_occupied = true;
    m_num_anchor_connected++;
    m_alloc_anchor_list_size++;
}

static void add_tag_into_connected_list(dwm1001_node_status *node, uint8_t idx)
{
    Logi(GATTC_TAG, "Add tag node into connected list");
    memcpy(m_connected_tag_list[idx].ID, node->ID, sizeof(node->ID));
    m_connected_tag_list[idx].is_slot_occupied = true;
    m_connected_tag_list[idx].notify_handle = node->tag.notify_handle;
    m_num_tag_connected++;
    m_alloc_tag_list_size++;
}

static void add_node_into_connected_list(dwm1001_node_status *node)
{
    if (node->node_type == ANCHOR)
    {
        if (m_num_anchor_connected == MAX_DWM1001_ANCHOR) 
        {
            Loge(GATTC_TAG, "Error allocate new anchor");
            return;
        }
        for (uint8_t idx = 0; idx < MAX_DWM1001_ANCHOR; idx++)
        {
            if (m_connected_anchor_list[idx].is_slot_occupied == false)
            {
                add_anchor_into_connected_list(node, idx);
                save_node_bda((char*)node->blu_addr, idx + 1, node->node_type);
                save_node_id(node->ID, idx + 1, node->node_type);
                save_node_operation(node->operation_mode, idx + 1, node->node_type);
                // m_connected_anchor_list[m_num_anchor_connected].anchor_loc.x = node.anchor.anchor_loc.x;
                // m_connected_anchor_list[m_num_anchor_connected].anchor_loc.y = node.anchor.anchor_loc.y;
                // m_connected_anchor_list[m_num_anchor_connected].anchor_loc.z = node.anchor.anchor_loc.z;
                break;
            }
        }
    }
    else
    {
        if (m_num_tag_connected == MAX_DWM1001_TAG)
        {
            Loge(GATTC_TAG, "Error allocate new tag");
            return;
        }
        for (uint8_t idx = 0; idx < MAX_DWM1001_TAG; idx++)
        {
            if (m_connected_tag_list[idx].is_slot_occupied == false)
            {
                add_tag_into_connected_list(node, idx);
                // save_node_bda((char*)node->blu_addr, idx + 1, node->node_type);
                // save_node_id(node->ID, idx + 1, node->node_type);
                // save_node_operation(node->operation_mode, idx + 1, node->node_type);
                break;
            }
        }
    }
    save_network_info(m_num_tag_connected, m_num_anchor_connected);
}

static void delete_node_from_connected_list(dwm1001_node_status node)
{
    if (node.node_type == ANCHOR)
    {
        for (uint8_t idx = 0; idx < m_alloc_anchor_list_size; idx++)
        {
            if (IDCmp(m_connected_anchor_list[idx].ID, node.ID) == true)
            {
                Logi(GATTC_TAG, "Delete anchor node from connected list");
                memset(m_connected_anchor_list[idx].ID, 0, sizeof(m_connected_anchor_list[idx].ID));
                m_connected_anchor_list[idx].is_slot_occupied = false;
                m_num_anchor_connected--;
            }
        }
    }
    else
    {
        for (uint8_t idx = 0; idx < m_alloc_tag_list_size; idx++)
        {
            if (IDCmp(m_connected_anchor_list[idx].ID, node.ID) == true)
            {
                Logi(GATTC_TAG, "Delete tag node from connected list");
                memset(m_connected_tag_list[idx].ID, 0, sizeof(m_connected_tag_list[idx].ID));
                m_connected_tag_list[idx].is_slot_occupied = false;
                m_num_tag_connected--;
            }
        }
    }
}

static int8_t is_anchor_node_exist(char* ID)
{
    for (uint8_t idx = 0; idx < m_alloc_anchor_list_size; idx++)
    {
        if (IDCmp(ID, m_connected_anchor_list[idx].ID) == ESP_OK)
        {
            return idx;
        }
    }
    return -1;
}

static int8_t is_tag_node_exist(char* ID)
{
    for (uint8_t idx = 0; idx < m_alloc_tag_list_size; idx++)
    {
        if (IDCmp(ID, m_connected_tag_list[idx].ID) == ESP_OK)
        {
            return idx;
        }
    }
    return -1;
}

static int8_t is_tag_in_connecting_list(esp_bd_addr_t bda)
{
    for (uint8_t idx = 0; idx < m_num_node_connecting; idx++)
    {
        if (bda_cmp(bda, m_node_connecting_list[idx].blu_addr) == ESP_OK)
        {
            return idx;
        }
    }
    return -1;
}

static uint8_t app_ble_dwm1001_get_slot_from_id(char* ID){
    for (uint8_t index = 0; index < MAX_GATT_CONNECTION; index++)
    {
        if (IDCmp(ID, m_node_connecting_list[index].ID) == ESP_OK)
        {
            return index;
        }
    }
    return MAX_GATT_CONNECTION;
}

uint8_t getEmptySeat()
{
    for (uint8_t index = 0; index < MAX_GATT_CONNECTION; index++)
    {
        if (gl_profile_tab[index].is_occupied == false)
        {
            return index;
        }
    }
    return MAX_GATT_CONNECTION + 1;
}
static esp_err_t IDCmp(char* id1, char* id2)
{
    for (uint8_t idx = 0; idx < 16; idx++)
    {
        if (id1[idx] != id2[idx])
        {
            //ESP_LOGI(GATTC_TAG, "%c%c", id1[index], id2[index]);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t bda_cmp(esp_bd_addr_t bda1, esp_bd_addr_t bda2)
{
    for (uint8_t idx = 0; idx < sizeof(esp_bd_addr_t); idx++)
    {
        if (bda1[idx] != bda2[idx])
        {
            //ESP_LOGI(GATTC_TAG, "%c%c", id1[index], id2[index]);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

// static esp_err_t bda_cmp(esp_bd_addr_t bda1, esp_bd_addr_t bda2)
// {
//     for (uint8_t index = 0; index < 6; index++)
//     {
//         if (bda1[index] != bda2[index])
//         {
//             return ESP_FAIL;
//         }
//     }
//     return ESP_OK;
// }

// static uint8_t toHex(char c)
// {
//     if ((c >= '0') && (c <= '9'))
//     {
//         return c - '0';
//     }
//     if ((c >= 'a') && (c <= 'f'))
//     {
//         return c - 87;
//     }
//     return 0;
// }

// static esp_err_t convertUUID128(char* uuid128, uint8_t* result)
// {   
//     if ((strlen(uuid128) != 32))
//     {
//         return ESP_FAIL;
//     }
//     for (uint8_t index = 0; index < 32; index += 2)
//     {
//         result[index / 2] = toHex(uuid128[30 - index]) * 16 + toHex(uuid128[31 - index]);
        
//     }
//     Logi(GATTC_TAG, "result");
//     for (uint8_t index = 0; index < 16; index++)
//     {
//         printf("0x%02x, ", result[index]);
//     }
//     return ESP_OK;
// }

static void parseLocation(uint8_t* message, int32_t* x, int32_t* y, int32_t* z)
{
    
    int32_t lx = 0, ly = 0, lz = 0;
    lx = ((message[4] & 0x7F) * 256 * 256 * 256 + message[3] * 256 * 256 + message[2] * 256 + message[1]) * (message[4] >> 7 == 1 ? (-1) : 1);
    ly = ((message[8] & 0x7F) * 256 * 256 * 256 + message[7] * 256 * 256 + message[6] * 256 + message[5]) * (message[8] >> 7 == 1 ? (-1) : 1);
    lz = ((message[12] & 0x7F) * 256 * 256 * 256 + message[11] * 256 * 256 + message[10] * 256 + message[9]) * (message[12] >> 7 == 1 ? (-1) : 1);
    *x = lx;
    *y = ly;
    *z = lz;
}
static void parseOperationMode(uint8_t* message, dwm1001_node_status *result)
{
    result->node_type = message[0] >> 7;
    result->uwb_type = (message[0] & 0x60) >> 5 ;
    result->acc_en = (message[0] & (1 << 3)) >> 3;
    result->led_en = (message[0] & (1 << 2)) >> 2; 
    if (result->node_type == TAG)
    {
        result->lpw_en = message[1] >> 7;
        result->loc_engine_en = (message[1] >> 6) & (0x01);  
    }
    else
    {
        result->init_en = message[1] >> 7;
    }
}

static esp_err_t parseID(uint8_t* ID, uint8_t ID_len, char* IDstr)
{
    for (uint8_t idx = 0; idx < ID_len; idx++)
    {
        IDstr[ID_len * 2 - idx * 2 - 1] = ((ID[idx] % 16) > 9) ? (ID[idx] % 16 + 55) : (ID[idx] % 16 + 48);
        IDstr[ID_len * 2 - idx * 2 - 2] = ((ID[idx] / 16) > 9) ? (ID[idx] / 16 + 55) : (ID[idx] / 16 + 48);
    }
    IDstr[ID_len * 2] = '\0';
    return ESP_OK;
}

static bool IRAM_ATTR timer_group_isr_callback(void *args)
{
    BaseType_t high_task_awoken = pdFALSE;
    xSemaphoreGiveFromISR(sem_get_tags_pos, &high_task_awoken);
    return high_task_awoken == pdTRUE;
}
static void timer_init_ms(int group, int timer, bool auto_reload)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    }; // default clock source is APB
    timer_init(group, timer, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(group, timer, 0);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(group, timer, 4000);
    timer_enable_intr(group, timer);
    timer_isr_callback_add(group, timer, timer_group_isr_callback, NULL, 0);
}

