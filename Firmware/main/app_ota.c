#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "freertos/task.h"
#include "app_main.h"
#include "cJSON.h"

typedef struct{
    uint16_t firm_ver;
    uint16_t hard_ver;
    char *model;
}version_info_t;

extern esp_mqtt_client_handle_t client_mqtt;
extern char request_topic[];
static version_info_t version_info;
static void update_ota_result(uint8_t result);
static void ota_task(void *pvParameters);
static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt);
static void get_info_firmware(char *version);
static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        Logd("OTA", "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        Logd("OTA", "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        Logd("OTA", "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        Logd("OTA", "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        Logd("OTA", "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        Logd("OTA", "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        Logd("OTA", "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}
static void get_info_firmware(char *version){
    cJSON *root = cJSON_Parse(version);
    if (cJSON_GetObjectItem(root,"firmware"))
        version_info.firm_ver = cJSON_GetObjectItem(root,"firmware")->valueint;
    if (cJSON_GetObjectItem(root,"hardware"))
        version_info.hard_ver = cJSON_GetObjectItem(root,"hardware")->valueint;
    if (cJSON_GetObjectItem(root,"model"))
        version_info.model = cJSON_GetObjectItem(root,"model")->valuestring;
}
static void __attribute__((noreturn)) task_fatal_error(void)
{
    Loge("LOG", "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);
    while (1){
        ;
    }
}
static void update_ota_result(uint8_t result){
    int msg_id = 0;
    cJSON *TxJSON_root = cJSON_CreateObject();
    cJSON *values = cJSON_CreateObject();
    Logi("LOG","Update OTA result to Broker.");
    cJSON_AddNumberToObject(TxJSON_root,"ts",device.ts);
    cJSON_AddNumberToObject(TxJSON_root,"id",device.id);   
    cJSON_AddStringToObject(TxJSON_root,"method","ota_result");
    cJSON_AddTrueToObject(TxJSON_root,"success");
    if (result == 0) cJSON_AddStringToObject(values,"result","failed");
    else if (result == 1) cJSON_AddStringToObject(values,"result","successed");
    cJSON_AddItemToObject(TxJSON_root,"values",values);
    char *tx_json = cJSON_Print(TxJSON_root);
    cJSON_Delete(TxJSON_root);
    Logi("LOG", "%s",tx_json);
    msg_id = esp_mqtt_client_publish(client_mqtt, response_topic, tx_json, 0, 0, 0);
    Logi("MQTT_CLIENT", "sent publish successful, msg_id=%d", msg_id);
    free(tx_json);
}
void ota_task(void *pvParameters)
{  
    static esp_http_client_handle_t http_client = NULL; 
    esp_err_t err;
    int data_read;
    char version_data[128]={0};
    char url_version[128]={0};
    Logi("OTA","Running firmware: ver %d",device.firmware);
    esp_http_client_config_t config = {
        .url = "http://osuno-ota.dol-tech.com/CFWF",
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = ota_http_event_handler,
        .keep_alive_enable = true,
    };
    http_client = esp_http_client_init(&config);
    if (http_client == NULL) {
        Loge("LOG", "Failed to initialise HTTP connection");
        task_fatal_error();
    }
    //Receive version
    sprintf(url_version,"http://osuno-ota.dol-tech.com/%s/%d/2/version.txt",MODEL,HARDWARE_VERSION);
    // Logi("LOG","%s",url_version);
    esp_http_client_set_url(http_client,url_version);
    err = esp_http_client_open(http_client, 0);
    if (err != ESP_OK) {
        Loge("LOG", "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(http_client);
        task_fatal_error();
    }
    esp_http_client_fetch_headers(http_client);
    data_read =  esp_http_client_read(http_client,version_data,128);
    if (data_read<=0){
        Loge("LOG", "Read version failed");
        task_fatal_error();
    }
    get_info_firmware(version_data);
    Logi("OTA","Request update new firmware [MODEL: %s] [HARDWARE_VER: %d] [FIRMWARE_VER: %d] ",version_info.model,version_info.hard_ver,version_info.firm_ver);
    //Receive firmware
    char url[128]={0};
    esp_http_client_close(http_client);
    sprintf(url,"http://osuno-ota.dol-tech.com/%s/%d/2/%d.bin",version_info.model,version_info.hard_ver,version_info.firm_ver);
    config.url = url;
    config.cert_pem = (const char*)"CFWF";
    Logi("OTA","%s",url);
    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        save_log(CAUSE_OTA_RESULT,VALUE_SUCCESS,0,device.time_stamp);
        update_ota_result(1);
        save_log(CAUSE_REBOOT,VALUE_SUCCESS,6,device.time_stamp);
        vTaskDelay(1000);
        esp_restart();
    } else {
        save_log(CAUSE_OTA_RESULT,VALUE_FAIL,0,device.time_stamp);
        update_ota_result(0);
    }
    vTaskDelete(NULL);
}
void app_ota(void){
    xTaskCreate(ota_task,"ota_task",4096,NULL,12,NULL);
}