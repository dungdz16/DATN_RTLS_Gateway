#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "print_log.h"
#include "mqtt_client.h"
// typedef esp_err_t (*ptrFunc)(esp_mqtt_event_handle_t);
void log_error_if_nonzero(const char * message, int error_code);
void mqtt_register_callback(mqtt_event_callback_t callback);
esp_mqtt_client_handle_t mqtt_app_start(char *host, uint16_t port, char *user, char *password, char *client_id, const char *root_ca);
void mqtt_app_stop(esp_mqtt_client_handle_t client);