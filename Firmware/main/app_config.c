/*
 * app_config.c
 *
 *  Created on: Dec 23, 2021
 *      Author: ManhTH
 *      Config Wifi and MQTT for Gateway RTLS product using Smart config
 */
#include "app_main.h"
#include "cJSON.h"
#include "nvs_store.h"
#include "mbedtls/base64.h"
/* An HTTP GET handler */
#define MQTT_CONFIG_BIT BIT0
#define MQTT_CONNECT_OK_BIT BIT1
#define MQTT_CONNECT_FAIL_BIT   BIT2
#define HTTP_SERVER_TIMEOUT_BIT BIT3

rtls_app_param_t project_param;
static uint8_t is_config = 0;
static httpd_handle_t server = NULL;
static void parse_API_setup_project(char *buffer);
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);
static esp_err_t esp_server_get_handler(httpd_req_t *req);
static esp_err_t esp_server_post_handler(httpd_req_t *req);

//Function for http server
httpd_handle_t start_webserver(void);
//API config for http server
static  httpd_uri_t api_setup_cert = {
    .uri       = "/api/setup/cert",
    .method    = HTTP_POST,
    .handler   = esp_server_post_handler,
    .user_ctx  = NULL
};
static  httpd_uri_t api_setup_project = {
    .uri       = "/api/setup/project",
    .method    = HTTP_POST,
    .handler   = esp_server_post_handler,
    .user_ctx  = NULL
};
static  httpd_uri_t api_info = {
    .uri       = "/api/info",
    .method    = HTTP_GET,
    .handler   = esp_server_get_handler,
    .user_ctx  = NULL
};

custom_partition_t user_partition;
void app_config_start();
extern void app_sync_time();

httpd_handle_t start_webserver(void){
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    // Start the httpd server
    Logi("esp_server", "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        Logi("esp_server", "Registering URI handlers");
        httpd_register_uri_handler(server, &api_setup_cert);
        httpd_register_uri_handler(server, &api_setup_project);
        httpd_register_uri_handler(server, &api_info);
        return server;
    }
    Loge("esp_server", "Error starting server!");
    return NULL;
}
void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}
static void parse_API_setup_project(char *buffer){
    cJSON *root = cJSON_CreateObject();
    char *host_tmp = NULL;
    char *projectID_tmp = NULL;
    char *location_tmp = NULL;
    char *registryID_tmp = NULL;
    root = cJSON_Parse(buffer);
    if (cJSON_GetObjectItem(root,"host"))
       host_tmp = cJSON_GetObjectItem(root,"host")->valuestring;
    memcpy(project_param.host,host_tmp,sizeof(project_param.host));
    if (cJSON_GetObjectItem(root,"port")) 
        project_param.port = cJSON_GetObjectItem(root,"port")->valueint;
    if (cJSON_GetObjectItem(root,"project_id"))
        projectID_tmp = cJSON_GetObjectItem(root,"project_id")->valuestring;
    memcpy(project_param.project,projectID_tmp,sizeof(project_param.project));
    if (cJSON_GetObjectItem(root,"location"))
        location_tmp = cJSON_GetObjectItem(root,"location")->valuestring;
    memcpy(project_param.location,location_tmp,sizeof(project_param.location));
    if (cJSON_GetObjectItem(root,"registry_id"))
        registryID_tmp = cJSON_GetObjectItem(root,"registry_id")->valuestring;
    memcpy(project_param.registry,registryID_tmp,sizeof(project_param.registry));
    Logi("esp_server","Host: %s", project_param.host);
    Logi("esp_server","Port: %d", project_param.port);
    Logi("esp_server","ProjectID: %s", project_param.project);
    Logi("esp_server","Location: %s", project_param.location);
    Logi("esp_server","RegistryID: %s", project_param.registry);
    nvs_write_str("HOST",project_param.host);
    nvs_write_u32("PORT",project_param.port);
    nvs_write_str("PROJECT",project_param.project);
    nvs_write_str("LOCATION",project_param.location);
    nvs_write_str("REGISTRY",project_param.registry);
}
static void parse_API_setup_cert(char *buffer){
    cJSON *root = cJSON_CreateObject();
    root = cJSON_Parse(buffer);
    if (cJSON_GetObjectItem(root,"root_ca")){
        cJSON *root_ca = cJSON_GetObjectItem(root,"root_ca");
        uint16_t root_ca_len = 0;
        if (cJSON_GetObjectItem(root_ca,"length"))
            root_ca_len = cJSON_GetObjectItem(root_ca,"length")->valueint;
        char *root_ca_data = malloc(root_ca_len * sizeof(char) + 1);
        memset(root_ca_data,0,root_ca_len);
        if (cJSON_GetObjectItem(root_ca,"data"))
            root_ca_data = cJSON_GetObjectItem(root_ca,"data")->valuestring;
        char *cabuf = malloc(root_ca_len * sizeof(char));
        memset(cabuf,0,root_ca_len * sizeof(char));
        project_param.ca_len = 0;
        mbedtls_base64_decode((uint8_t*)cabuf,root_ca_len,&project_param.ca_len,(uint8_t*)root_ca_data,root_ca_len);
        Logi("root_ca","[%d]%s",project_param.ca_len,cabuf);
        custom_partition_write(user_partition.partition,0,(uint8_t*)cabuf,project_param.ca_len);
        nvs_write_u32("CA_LEN",(uint32_t)project_param.ca_len);
        free(root_ca_data);
        free(cabuf);
    }
    if (cJSON_GetObjectItem(root,"private_key")){
        cJSON *prv_key = cJSON_GetObjectItem(root,"private_key");
        uint16_t prv_key_len = 0;
        if (cJSON_GetObjectItem(prv_key,"length"))
            prv_key_len = cJSON_GetObjectItem(prv_key,"length")->valueint;
        char *prv_key_data = malloc(prv_key_len * sizeof(char) + 1);
        memset(prv_key_data,0,prv_key_len);
        if (cJSON_GetObjectItem(prv_key,"data"))
            prv_key_data = cJSON_GetObjectItem(prv_key,"data")->valuestring;
        char *prv_key_buf = malloc(prv_key_len * sizeof(char));
        memset(prv_key_buf,0,prv_key_len * sizeof(char));
        project_param.prv_key_len = 0;
        mbedtls_base64_decode((uint8_t*)prv_key_buf,prv_key_len,&project_param.prv_key_len,(uint8_t*)prv_key_data,prv_key_len);
        Logi("private_key","[%d]%s",project_param.prv_key_len,prv_key_buf);
        custom_partition_write(user_partition.partition,4096,(uint8_t*)prv_key_buf,project_param.prv_key_len);
        nvs_write_u32("PRVKEY_LEN",(uint32_t)project_param.prv_key_len);
        free(prv_key_data);
        free(prv_key_buf);
    }
}
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            Logi("MQTT_CLIENT","Connect success to MQTT Broker");
            // User and pwd valid, stop http server and save mqtt config to nvs here
            xEventGroupSetBits(device.mqtt_event_group, MQTT_CONNECT_OK_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            Logi("MQTT_CLIENT","MQTT_EVENT_DISCONNECTED");
            xEventGroupSetBits(device.mqtt_event_group, MQTT_CONNECT_FAIL_BIT);
            break;
        case MQTT_EVENT_ERROR:
            Logi("MQTT_CLIENT", "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                Loge("MQTT_CLIENT","MQTT_ERROR_TYPE_TCP_TRANSPORT");
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            }
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                Loge("MQTT_CLIENT","MQTT_ERROR_TYPE_CONNECTION_REFUSED, return code %d",event->error_handle->connect_return_code);                
            }
            xEventGroupSetBits(device.mqtt_event_group, MQTT_CONNECT_FAIL_BIT);
            break;
        default:
            // Logi("LOG", "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}
static esp_err_t esp_server_get_handler(httpd_req_t *req){
    char resp[128];
    sprintf(resp,"{\"id\":%u,\"model\":\"%s\",\"firmware\":%d,\"hardware\":%d,\"mac\":\"%s\"}",
                        device.id,device.product_id,FIRMWARE_VERSION,HARDWARE_VERSION,device.mac_addr_str);
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
/* An HTTP POST handler */
static esp_err_t esp_server_post_handler(httpd_req_t *req){
    int data_len = req->content_len;
    char *buf = malloc(data_len * sizeof(char)+1);
    memset(buf,0,data_len * sizeof(char)+1);
    int ret = 0, remaining = req->content_len;
    static bool is_cf_project = false, is_cf_cert = false;
    char *uri = malloc(HTTPD_MAX_URI_LEN + 1);
    memcpy(uri,req->uri,HTTPD_MAX_URI_LEN + 1);
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, data_len))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }
    Logi("esp_server","content-len[%d]",data_len);
    if (strstr(uri,"api/setup/project")!=NULL){
        Logi("esp_server","uri = api/setup/project");
        parse_API_setup_project(buf);
        is_cf_project = true;
        httpd_resp_send_chunk(req, NULL, 0);
    } 
    else if (strstr(uri,"api/setup/cert")!=NULL) {
        custom_partition_erase(user_partition.partition,0,user_partition.partition->size);
        Logi("uri","api/setup/cert");
        parse_API_setup_cert(buf);
        char resp[64]={0};
        sprintf(resp,"{\"ca_length\":%d,\"private_key_length\":%d}",project_param.ca_len,project_param.prv_key_len);
        is_cf_cert = true;
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);  
    }
    if (is_cf_cert && is_cf_project){
        /* Send back the same data */
        // char status_true[] = "{\"status\":true,\"error_code\":null}";
        xEventGroupSetBits(device.mqtt_event_group, MQTT_CONFIG_BIT);
    }
    return ESP_OK;
}
static void time_out_task(void *pvParameter){
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 1000;
	xLastWakeTime = xTaskGetTickCount ();
    uint8_t count_sec = 0;
    for (;;){
        count_sec ++;
        if (count_sec == 120){
            xEventGroupSetBits(device.mqtt_event_group,HTTP_SERVER_TIMEOUT_BIT);
            vTaskDelete(NULL);
        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency );
    }
}
void syslog_event_handler(syslog_id_e evt_id, void* param){
    if (evt_id == WIFI_EVENT_CONNECTED){
        save_log(CAUSE_WIFI_CONNECT,VALUE_SUCCESS,0,device.time_stamp);
    }
    else if (evt_id == WIFI_EVENT_DISCONNECTED) {
        save_log(CAUSE_WIFI_CONNECT,VALUE_FAIL,0,device.time_stamp);
    }
}
void app_config_start(){
    EventBits_t uxBits;
    uint8_t ret = 0;
    #ifdef MQTT_CONNECT_DEV
    static esp_mqtt_client_handle_t client_mqtt = NULL;
    #endif
    wifi_register_syslog_callback(syslog_event_handler);
    user_partition.partition = custom_partition_init((char*)"user_param");
    Logi("user_param","\"%s\" partition found:  ,0x%.4x ,0x%.4x  ",user_partition.partition ->label,user_partition.partition ->address,user_partition.partition ->size);
    initialise_wifi();
    app_sync_time();
    mqtt_register_callback(mqtt_event_handler_cb);
    if (nvs_read_str("HOST",project_param.host) == nvs_ok) ret++;
    if (nvs_read_str("PROJECT",project_param.project) == nvs_ok) ret++;
    if (nvs_read_str("LOCATION",project_param.location) == nvs_ok) ret++;
    if (nvs_read_str("REGISTRY",project_param.registry) == nvs_ok) ret++;

    if (nvs_read_u32("PORT",&project_param.port) == nvs_ok) ret++;
    if (nvs_read_u32("CA_LEN",&project_param.ca_len) == nvs_ok) ret++;
    if (nvs_read_u32("PRVKEY_LEN",&project_param.prv_key_len) == nvs_ok) ret++;
    device.mqtt_event_group = xEventGroupCreate();
    if (ret < 7) {
        server = start_webserver();
        led_control(120);
        xTaskCreate(time_out_task,"TimeOut_task",configMINIMAL_STACK_SIZE,NULL,12,NULL);
        app_sync_time();
        while(!device.mqtt_connect_ok){
            uxBits = xEventGroupWaitBits(device.mqtt_event_group, MQTT_CONFIG_BIT|HTTP_SERVER_TIMEOUT_BIT,true, false, portMAX_DELAY);
            if (uxBits & MQTT_CONFIG_BIT){  
                device.mqtt_connect_ok = 1;
            }
            if (uxBits & HTTP_SERVER_TIMEOUT_BIT) {
                Loge("LOG","No response from APP, restart ESP");
                nvs_erase_brokerCfg();
                save_log(CAUSE_APP_CONFIG,VALUE_FAIL,EXTRA_HTTP_SERVER,device.time_stamp);
                vTaskDelay(100);
                save_log(CAUSE_REBOOT,VALUE_SUCCESS,2,device.time_stamp);
                esp_restart();
            }
        } 
    }
}