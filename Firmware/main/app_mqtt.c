/*
 * app_mqtt.c
 *
 *  Created on: Dec 23, 2021
 *      Author: ManhTH
 *      Transceiver data with MQTT Broker
 */
#include "mqtt_esp.h"
#include "cJSON.h"
#include "app_main.h"
#include "mbedtls/base64.h"
#include "iotc.h"
#include "iotc_jwt.h"
#include "ble_dwm1001.h"

#define DEVICE_PATH "projects/%s/locations/%s/registries/%s/devices/%s"
#define TOPIC_EVENT "/devices/%s/events"
#define TOPIC_STATE "/devices/%s/state"

#define MAX_CLIENT_ID_LENGTH   256

extern custom_partition_t user_partition;
extern rtls_app_param_t project_param;
//extern QueueHandle_t uwb_queue;
char device_topic_event[64] = {0};
char device_topic_state[64] = {0};
char gateway_topic_event[64] = {0};
char gateway_topic_state[64] = {0};

char *client_ID = NULL;

typedef struct{
    int16_t mbedTLS_err;
    int16_t espTLS_err;
    int16_t sock_err;
}tcp_connect_error_t;

tcp_connect_error_t tcp_err_code ={0};

mqtt_method_t mqtt_method;
esp_connection_t esp_connection;
char request_topic[64]={0};
char response_topic[64]={0};
extern syslog_t syslog_part;
esp_mqtt_client_handle_t client_mqtt = NULL;
static wifi_config_t esp_wifi_config;
static esp_err_t mqtt_client_event_handler(esp_mqtt_event_handle_t event);
//static void mqtt_client_process_event_data(void);
//static void enqueue_task(void *param);

void app_mqtt_client_start(void);
void update_status_server(void);
void update_extend_status_server(void);
extern void app_ota(void);

static void gateway_update_extend_state(void);
static void gateway_update_state(void);

static esp_err_t mqtt_client_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    static uint16_t error_code = 0;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            Logi("mqtt_client", "MQTT_EVENT_CONNECTED");
            gateway_update_state();
            led_control(MQTT_CONNECTED_STATUS);
            save_log(CAUSE_MQTT_CONNECT,VALUE_SUCCESS,0,device.time_stamp);
            esp_connection = CLOUD_CONNECTED;
            break;
        case MQTT_EVENT_DISCONNECTED:
            Loge("mqtt_client", "MQTT_EVENT_DISCONNECTED");
            save_log(CAUSE_MQTT_DISCONNECT,0,0,device.time_stamp);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            Logi("mqtt_client", "Subscribe topic \"%s\" successful, msg_id=%d", request_topic, event->msg_id);    
            break;
        case MQTT_EVENT_PUBLISHED:
            Logi("mqtt_client", "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            Logi("mqtt_client", "MQTT_EVENT_DATA");
            #ifdef DEBUG_MONITOR
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            #endif
            // RxJSON_root = cJSON_Parse(event->data);
            //Process Data here
            break;
        case MQTT_EVENT_ERROR:

            Loge("mqtt_client", "MQTT_EVENT_ERROR");

            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {

                Loge("mqtt_client","MQTT_ERROR_TYPE_TCP_TRANSPORT");

                tcp_err_code.espTLS_err = event->error_handle->esp_tls_last_esp_err;
                tcp_err_code.mbedTLS_err = event->error_handle->esp_tls_stack_err;
                tcp_err_code.sock_err =  event->error_handle->esp_transport_sock_errno;
                if (tcp_err_code.mbedTLS_err != 0) save_log(CAUSE_TCP_CONNECT,tcp_err_code.mbedTLS_err,0,device.time_stamp);
                if (tcp_err_code.espTLS_err != 0) save_log(CAUSE_TCP_CONNECT,tcp_err_code.espTLS_err,1,device.time_stamp);
                if (tcp_err_code.sock_err != 0) save_log(CAUSE_TCP_CONNECT,tcp_err_code.sock_err,2,device.time_stamp);

                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);

            }
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {

                Loge("mqtt_client","MQTT_ERROR_TYPE_CONNECTION_REFUSED, return code %d",event->error_handle->connect_return_code);

                if (event->error_handle->connect_return_code == 4 || event->error_handle->connect_return_code == 5) error_code = 1006;
                else if (event->error_handle->connect_return_code == 3) error_code = 1009;
                else if (event->error_handle->connect_return_code == 2) error_code = 1008;
                else if (event->error_handle->connect_return_code == 1)  error_code = 1007;   
                save_log(CAUSE_MQTT_CONNECT,VALUE_FAIL,error_code - 1001,device.time_stamp);  
            }
            break;
        default:
            // Logi("mqtt_client", "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}
static void gateway_update_state(void){
    cJSON *root = cJSON_CreateObject();
    cJSON *values = cJSON_CreateObject();
    cJSON_AddNumberToObject(root,"ts",device.time_stamp + 946659600);
    cJSON_AddNumberToObject(root,"id",device.id);
    cJSON_AddStringToObject(root,"method","state");
    cJSON_AddTrueToObject(values,"state");
    cJSON_AddItemToObject(root,"values",values);
    char *payload = cJSON_Print(root);
    Logi("payload","%s",payload);
    int msg_id = esp_mqtt_client_publish(client_mqtt,gateway_topic_state,payload,0,0,0);
    if (msg_id != 0) Loge("mqtt_client","Publish message fail");
    cJSON_Delete(root);
}
static void gateway_update_extend_state(void){
    cJSON *root = cJSON_CreateObject();
    cJSON *values = cJSON_CreateObject();
    cJSON_AddNumberToObject(root,"ts",device.time_stamp + 946659600);
    cJSON_AddNumberToObject(root,"id",device.id);
    cJSON_AddStringToObject(root,"method","extend_state");
    cJSON_AddNumberToObject(values,"firmware",device.firmware);
    cJSON_AddNumberToObject(values,"hardware",device.hardware);
    cJSON_AddNumberToObject(values,"rst_reason",device.rst_reason);
    cJSON_AddNumberToObject(values,"networkID",device.rst_reason);
    cJSON_AddItemToObject(root,"values",values);
    char *payload = cJSON_Print(root);
    Logi("payload","%s",payload);
    int msg_id = esp_mqtt_client_publish(client_mqtt,gateway_topic_state,payload,0,0,0);
    if (msg_id != 0) Loge("mqtt_client","Publish message fail");
    cJSON_Delete(root);   
}
static void create_jwt_client(char *jwt){
    /* Format the key type descriptors so the client understands
     which type of key is being represented. In this case, a PEM encoded
     byte array of a ES256 key. */
    if (project_param.prv_key_len != 0){
        project_param.private_key = (char*) malloc(project_param.prv_key_len * sizeof(char) + 1);
        memset(project_param.private_key,0,project_param.prv_key_len + 1);
        custom_partition_read(user_partition.partition,4096,(uint8_t*)project_param.private_key,project_param.prv_key_len);
    }
    else {
        Loge("mqtt_client","No private key");
        return;
    }
    iotc_crypto_key_data_t iotc_connect_private_key_data;
    iotc_connect_private_key_data.crypto_key_signature_algorithm = IOTC_CRYPTO_KEY_SIGNATURE_ALGORITHM_ES256;
    iotc_connect_private_key_data.crypto_key_union_type = IOTC_CRYPTO_KEY_UNION_TYPE_PEM;
    iotc_connect_private_key_data.crypto_key_union.key_pem.key = project_param.private_key;

    char tmp[IOTC_JWT_SIZE] = {0};
    size_t bytes_written = 0;
    iotc_state_t state = iotc_create_iotcore_jwt(
                             project_param.project,
                             /*jwt_expiration_period_sec=*/3600, &iotc_connect_private_key_data, tmp,
                             IOTC_JWT_SIZE, &bytes_written);

    if (IOTC_STATE_OK != state) {
        Loge("jwt", "iotc_create_iotcore_jwt returned with error: %ul", state);
        return;
    }
    memcpy(jwt,tmp,bytes_written-2);
    return;
}
static void set_mqtt_parameter(void){
    sprintf(gateway_topic_event,TOPIC_EVENT,device.product_id);
    sprintf(gateway_topic_state,TOPIC_STATE,device.product_id);
    Logi("topic","%s\n%s",gateway_topic_event,gateway_topic_state);
    if (client_ID != NULL) free(client_ID);
    client_ID = malloc(MAX_CLIENT_ID_LENGTH*sizeof(char));
    memset(client_ID,0,MAX_CLIENT_ID_LENGTH);
    sprintf(client_ID, DEVICE_PATH,     project_param.project, 
                                        project_param.location, 
                                        project_param.registry, 
                                        device.product_id);
    Logi("mqtt_client","clientID:%s",client_ID);
}
void send_location_from_bound_device(char* ID, uint8_t slot, dwm1001_location tag_loc){
    char nodeID[10];
    char node_id[17];
    sprintf(nodeID, "%s%02d", "tag", slot + 1);
    nodeID[5] = '\0';
    memcpy(node_id, ID, 16);
    node_id[16] = '\0';
    sprintf(device_topic_event, TOPIC_EVENT, nodeID);
    cJSON *root = cJSON_CreateObject();
    cJSON *values = cJSON_CreateObject();
    cJSON_AddNumberToObject(root,"ts",device.time_stamp + 946659600);
    cJSON_AddStringToObject(root,"id", node_id);
    cJSON_AddStringToObject(root,"method","loc_event");
    cJSON_AddNumberToObject(values,"pos_x", tag_loc.x);
    cJSON_AddNumberToObject(values,"pos_y", tag_loc.y);
    cJSON_AddNumberToObject(values,"pos_z", tag_loc.z);
    cJSON_AddNumberToObject(values,"quality",tag_loc.qf);
    cJSON_AddItemToObject(root,"values",values);
    char *payload = cJSON_Print(root);
    //Logi("topic", "%u",  esp_get_free_heap_size());
    //Logi("payload","%s",payload);
    int msg_id = esp_mqtt_client_publish(client_mqtt,device_topic_event,payload,0,0,0);
    if (msg_id != 0) Loge("mqtt_client","Publish message fail");
    cJSON_Delete(root);       
    free(payload);
}

void send_status_from_bound_device(char* ID, uint8_t slot, dwm1001_node_type node_type, uint8_t status)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *values = cJSON_CreateObject();
    char node_slot_name[10];
    char node_id[17];
    if ((node_type == ANCHOR) || (node_type == INITIATOR))
    {
        sprintf(node_slot_name, "%s%02d", "anchor", slot + 1);
        node_slot_name[8] = '\0';

    }
    else
    {
        sprintf(node_slot_name, "%s%02d", "tag", slot + 1);
        node_slot_name[5] = '\0';
    }
    memcpy(node_id, ID, 16);
    node_id[16] = '\0';
    cJSON_AddNumberToObject(root,"ts",device.time_stamp + 946659600);
    cJSON_AddStringToObject(root,"method","status");
    cJSON_AddStringToObject(values, "node_id", node_id);
    cJSON_AddStringToObject(values,"slot_name", node_slot_name);
    cJSON_AddStringToObject(values, "status", (status == 0)? "offline" : "online");
    cJSON_AddItemToObject(root,"values",values);
    char *payload = cJSON_Print(root);
    int msg_id = esp_mqtt_client_publish(client_mqtt,gateway_topic_event,payload,0,0,0);
    if (msg_id != 0) Loge("mqtt_client","Publish message fail");
    cJSON_Delete(root);
}

void send_new_device(char* ID, uint8_t slot, dwm1001_node_type node_type, dwm1001_location anchor_loc)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *values = cJSON_CreateObject();
    char nodeID[10];
    char node_id[17];
    if ((node_type == ANCHOR)  || (node_type == INITIATOR))
    {
        sprintf(nodeID, "%s%02d", "anchor", slot + 1);
        nodeID[8] = '\0';

    }
    else
    {
        sprintf(nodeID, "%s%02d", "tag", slot + 1);
        nodeID[5] = '\0';
    }
    memcpy(node_id, ID, 16);
    node_id[16] = '\0';
    cJSON_AddNumberToObject(root,"ts",device.time_stamp + 946659600);
    cJSON_AddStringToObject(root,"method","new_node");
    cJSON_AddStringToObject(values, "node_id", node_id);
    cJSON_AddStringToObject(values, "node_type", (node_type == ANCHOR) ? "anchor" : "tag");
    cJSON_AddStringToObject(values,"slot_name", nodeID);
    cJSON_AddItemToObject(root,"values",values);
    char *payload = cJSON_Print(root);
    //Logi("payload","%s",payload);
    int msg_id = esp_mqtt_client_publish(client_mqtt,gateway_topic_event,payload,0,0,0);
    if (msg_id != 0) Loge("mqtt_client","Publish message fail");
    cJSON_Delete(root);
    if (node_type == ANCHOR)
    {
        cJSON *root1 = cJSON_CreateObject();
        cJSON *values1 = cJSON_CreateObject();
        sprintf(device_topic_event, TOPIC_EVENT, nodeID);
        cJSON_AddNumberToObject(root1,"ts",device.time_stamp + 946659600);
        cJSON_AddStringToObject(root1,"id", ID);
        cJSON_AddStringToObject(root1,"method","loc_event");
        cJSON_AddNumberToObject(values1,"pos_x", anchor_loc.x);
        cJSON_AddNumberToObject(values1,"pos_y", anchor_loc.y);
        cJSON_AddNumberToObject(values1,"pos_z", anchor_loc.z);
        cJSON_AddNumberToObject(values1,"quality", anchor_loc.qf);
        cJSON_AddItemToObject(root1,"values",values1);
        char *payload = cJSON_Print(root1);
        //Logi("payload","%s",payload);
        if (esp_mqtt_client_publish(client_mqtt,device_topic_event,payload,0 ,0 ,0) != 0) 
            Loge("mqtt_client","Publish message fail");
        cJSON_Delete(root1);
    }       
}

static void ble_dwm1001_callback(app_ble_dwm1001_cb_param_t* param)
{
    
    switch (param->event)
    {
        case (APP_DWM1001_PUSH_LOC_EVT):
        {
            send_location_from_bound_device(param->get_loc.tag.ID, 
                                            param->get_loc.slot,
                                            param->get_loc.loc);
            break;
        }
        case (APP_DWM1001_NEW_DEV_EVT):
        {
            send_new_device(param->new_dev.new_dev.ID,
                            param->new_dev.slot,
                            param->new_dev.new_dev.node_type,
                            param->new_dev.new_dev.anchor.anchor_loc);
            break;
        }
        case (APP_DWM1001_LOST_DEV_EVT):
        {
            send_status_from_bound_device(param->lost_dev.ID,
                                         param->lost_dev.slot,
                                         param->lost_dev.node_type,
                                         0);
            break;
        }
        case (APP_DWM1001_ANCHOR_LIST_EVT):
        {
            break;
        }
        case (APP_DWM1001_SCAN_ANCHOR_TIMEOUT):
        {
            break;
        }
        case (APP_DWMW1001_RECONNECT_NODE):
        {
            send_status_from_bound_device(param->reconnect_node.reconnect_node.ID,
                                         param->reconnect_node.slot,
                                         param->reconnect_node.reconnect_node.node_type,
                                         1);
            break;
        }
        default:
        {
            break;
        }
    }   

}

void app_mqtt_client_start(void){
    set_mqtt_parameter();
    create_jwt_client(device.mqtt_config.pwd);
    Logi("jwt","%s",device.mqtt_config.pwd);
    esp_wifi_get_config(ESP_IF_WIFI_STA, &esp_wifi_config);
    mqtt_register_callback(mqtt_client_event_handler);
    if (project_param.ca_len != 0){
        if (project_param.root_ca != NULL) free(project_param.root_ca);
        project_param.root_ca = (char*) malloc(project_param.ca_len * sizeof(char) + 1);
        memset(project_param.root_ca,0,project_param.ca_len + 1);
        custom_partition_read(user_partition.partition,0,(uint8_t*)project_param.root_ca,project_param.ca_len);
        client_mqtt = mqtt_app_start(project_param.host, project_param.port, 
                "unused", device.mqtt_config.pwd, client_ID, (char*)project_param.root_ca);
        led_control(MQTT_CONNECTING_STATUS);
    }
    else {
        Loge("mqtt_client","no Root CA");
    }
    ble_dwm1001_register_callback(ble_dwm1001_callback);
    //xTaskCreate(enqueue_task,"test_task",2048 * 2,NULL,12,NULL);
}