#ifndef _BLE_NVS_H_
#define _BLE_NVS_H_

#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "print_log.h"
#include "time.h"
#include "nvs_store.h"

// typedef struct {
//     const esp_partition_t *log_partition;
//     bool instance;
//     uint32_t offset_address; 
//     uint32_t block_index;
// }ble_log_t;

// void ble_log_partition_init();

void save_node_id(char* node_id, uint8_t node_idx, uint8_t node_type);
void save_node_bda(char* node_bda, uint8_t node_idx, uint8_t node_type);
void save_node_operation(uint8_t* operation_data, uint8_t node_idx, uint8_t node_type);
void save_network_info(uint8_t num_tag, uint8_t num_anchor);
esp_err_t read_node_id(char* node_id, uint8_t node_idx, uint8_t node_type);
esp_err_t read_node_bda(uint8_t* node_bda, uint8_t node_idx, uint8_t node_type);
esp_err_t read_node_operation(uint8_t* operation_data, uint8_t node_idx, uint8_t node_type);
esp_err_t read_network_info(uint8_t *num_tag, uint8_t *num_anchor);

#endif