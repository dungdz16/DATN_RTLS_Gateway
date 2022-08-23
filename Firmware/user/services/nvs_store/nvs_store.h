#ifndef __NVS_STORE_H__
#define __NVS_STORE_H__
#include "nvs_flash.h"
#include "esp_partition.h"
#include "print_log.h"

typedef struct {
    esp_partition_t *partition;
    char *name;
}custom_partition_t;

typedef enum {
    nvs_ok = 0,
    nvs_not_init,
    nvs_error
}nvs_err_code;
//Function for NVS
void nvs_find_partition();
nvs_err_code nvs_read_str(char *key, char *data_out);
nvs_err_code nvs_write_str(char *key, char *data);
nvs_err_code nvs_write_u16(char *key, uint16_t value);
nvs_err_code nvs_read_u16(char *key, uint16_t *value);
nvs_err_code nvs_write_u32(char *key, uint32_t value);
nvs_err_code nvs_read_u32(char *key, uint32_t *value);
nvs_err_code nvs_write_u64(char *key, uint64_t value);
nvs_err_code nvs_read_u64(char *key, uint64_t *value);
nvs_err_code nvs_eraseAll(void);
nvs_err_code nvs_eraseKey(char *key);
void nvs_erase_brokerCfg(void);

//Function for operate with partition
// void custom_partition_init(const esp_partition_t *partition, char * name);
const esp_partition_t * custom_partition_init(char * name);
void custom_partition_write(const esp_partition_t *partition,size_t offset, uint8_t *data, size_t data_length);
void custom_partition_read(const esp_partition_t *partition,size_t offset, uint8_t *buffer, size_t buffer_length);
void custom_partition_erase(const esp_partition_t *partition,size_t offset, size_t size);

#endif