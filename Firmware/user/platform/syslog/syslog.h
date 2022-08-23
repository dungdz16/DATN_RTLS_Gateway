#ifndef __SYSLOG_H__
#define __SYSLOG_H__

#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "print_log.h"
#include "time.h"

#define CAUSE_BOOT_REASON   1
#define CAUSE_OTA_RESULT    7
#define CAUSE_WIFI_CONNECT  12
#define CAUSE_MQTT_CONNECT  13
#define CAUSE_TCP_CONNECT   14
#define CAUSE_SC_RESULT     15
#define CAUSE_APP_CONFIG     15
#define CAUSE_FACTORY_RST   16
#define CAUSE_REBOOT        17
#define CAUSE_HEAP_OVERFLOW     18
#define CAUSE_MQTT_DISCONNECT   19

#define EXTRA_MBEDTLS   0
#define EXTRA_ESPTLS    1
#define EXTRA_SOCK      2

#define EXTRA_SMARTCF   0
#define EXTRA_HTTP_SERVER   1
#define EXTRA_STNP_SYNC 2
#define EXTRA_MQTT_CONNECT  3

#define VALUE_SUCCESS   0
#define VALUE_FAIL      1

typedef enum{
    WIFI_EVENT_DISCONNECTED,
    WIFI_EVENT_CONNECTED,
}syslog_id_e;

typedef struct {
    const esp_partition_t *syslog_partition;
    bool instance;
    uint32_t offset_address; 
    uint32_t block_index;
}syslog_t;

void syslog_partition_init();
void save_one_record(uint8_t *data);
void read_block_index(uint32_t block_index, uint8_t *data);
uint32_t get_block_index(void);
void save_log(uint8_t cause, uint16_t value, uint8_t extra, time_t timestamp);
#endif