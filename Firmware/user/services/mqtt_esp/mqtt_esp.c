#include <stdio.h>
#include "mqtt_esp.h"
#include "smartconfig.h"

// #define USE_LOCAL_BROKER_TEST

static const char *TAG = "mqtt_esp";
extern const uint8_t ca_pem_start[]   asm("_binary_ca_pem_start");
extern const uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");

mqtt_event_callback_t ptrFunction = NULL;
void log_error_if_nonzero(const char * message, int error_code)
{
    if (error_code != 0) {
        Loge(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ptrFunction(event_data);
}
void mqtt_register_callback(mqtt_event_callback_t callback){
    ptrFunction = callback;
}
esp_mqtt_client_handle_t mqtt_app_start(char *host, uint16_t port, char *user, 
                                        char *password, char *client_id, const char *root_ca)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = host,
        .port = port,
        .username = user,
        .password = password,
        .client_id = client_id,
        .cert_pem = (const char *)root_ca,
        .transport = MQTT_TRANSPORT_OVER_SSL,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
    return client;
}
void mqtt_app_stop(esp_mqtt_client_handle_t client){
    esp_mqtt_client_stop(client);
}


