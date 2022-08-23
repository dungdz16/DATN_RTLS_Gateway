#include "ble_nvs.h"
#define LOG_TAG "ble_log"
#define tag_id                  "tag%02d/id"
#define tag_bda                 "tag%02d/bda"
#define tag_operation           "tag%02d/opr"
#define anchor_id               "anchor%02d/id"
#define anchor_bda              "anchor%02d/bda"
#define anchor_operation        "anchor%02d/opr"
#define anchor_pos              "anchor%02d/pos" 
#define TAG                     0
#define ANCHOR                  1

void save_node_id(char* node_id, uint8_t node_idx, uint8_t node_type)
{
    char key[15];
    if (node_type == TAG)
    {
        sprintf(key, tag_id, node_idx);
        key[9] = '\0';
    }
    else
    {
        sprintf(key, anchor_id, node_idx);
        key[12] = '\0';
    }
    node_id[16] = "\0";
    nvs_err_code err_code = nvs_write_str(key, node_id);
    if (err_code == nvs_error)
    {
        nvs_write_str(key, node_id);
    }
}

void save_node_bda(char* node_bda, uint8_t node_idx, uint8_t node_type)
{
    char key[15];
    if (node_type == TAG)
    {
        sprintf(key, tag_bda, node_idx);
        key[9] = '\0';
    }
    else
    {
        sprintf(key, anchor_bda, node_idx);
        key[12] = '\0';
    }
    // char bda[6];
    // memcpy(bda, (char*)node_bda, 6);
    ESP_LOG_BUFFER_HEX("ble_nvs", node_bda, 6);
    node_bda[6] = "\0";
    nvs_err_code err_code = nvs_write_str(key, node_bda);
    if (err_code == nvs_error)
    {
        nvs_write_str(key, node_bda);
    }
}

void save_node_operation(uint8_t* operation_data, uint8_t node_idx, uint8_t node_type)
{
    char key[15];
    if (node_type == TAG)
    {
        sprintf(key, tag_operation, node_idx);
        key[9] = '\0';
    }
    else
    {
        sprintf(key, anchor_operation, node_idx);
        key[12] = '\0';
    }
    uint16_t value = *(uint16_t*)operation_data;
    nvs_err_code err_code = nvs_write_u16(key, value);
    if (err_code == nvs_error)
    {
        nvs_write_u16(key, value);
    }
}

void save_network_info(uint8_t num_tag, uint8_t num_anchor)
{
    uint16_t value;
    value = num_tag << 8 | num_anchor;
    nvs_err_code err_code = nvs_write_u16("net_in", value);
    if (err_code == nvs_error)
    {
        nvs_write_u16("net_in", value);
    }
}

esp_err_t read_node_id(char* node_id, uint8_t node_idx, uint8_t node_type)
{
    char key[15];
    char id[16];
    if (node_type == TAG)
    {
        sprintf(key, tag_id, node_idx);
        key[9] = '\0';
    }
    else
    {
        sprintf(key, anchor_id, node_idx);
        key[12] = '\0';
    }
    nvs_err_code err_code = nvs_read_str(key, id);
    if (err_code == nvs_error)
    {
        err_code = nvs_read_str(key, node_id);
    }
    if (err_code == nvs_ok)
    {
        memcpy(node_id, id, 16);
    }
    return (err_code != nvs_ok) ? ESP_FAIL : ESP_OK;
}

esp_err_t read_node_bda(uint8_t* node_bda, uint8_t node_idx, uint8_t node_type)
{
    char key[15];
    if (node_type == TAG)
    {
        sprintf(key, tag_bda, node_idx);
        key[9] = '\0';
    }
    else
    {
        sprintf(key, anchor_bda, node_idx);
        key[12] = '\0';
    }
    char value[6];
    nvs_err_code err_code = nvs_read_str(key, value);
    if (err_code == nvs_error)
    {
        err_code = nvs_read_str(key, value);
    }
    if (err_code == nvs_ok)
    {
        memcpy((char*)node_bda, value, 6);
    }
    return (err_code != nvs_ok) ? ESP_FAIL : ESP_OK;
    //node_bda = (uint8_t*)value;
}

esp_err_t read_node_operation(uint8_t* operation_data, uint8_t node_idx, uint8_t node_type)
{
    char key[15];
    if (node_type == TAG)
    {
        sprintf(key, tag_operation, node_idx);
        key[9] = '\0';
    }
    else
    {
        sprintf(key, anchor_operation, node_idx);
        key[12] = '\0';
    }
    uint16_t value;
    nvs_err_code err_code = nvs_read_u16(key, &value);
    if (err_code == nvs_error)
    {
        err_code = nvs_read_u16(key, &value);
    }
    if (err_code == nvs_ok)
    {
            memcpy(operation_data, &value, 2);
    }
    return (err_code != nvs_ok) ? ESP_FAIL : ESP_OK;
    //operation_data = (uint8_t*)value;
}

esp_err_t read_network_info(uint8_t *num_tag, uint8_t *num_anchor)
{
    uint16_t value;
    nvs_err_code err_code = nvs_read_u16("net_in", &value);
    if (err_code == nvs_error)
    {
        nvs_read_u16("net_in", &value);
    }
    if (err_code == nvs_ok)
    {
        *num_tag = value >> 8;
        *num_anchor = value & 0xFF;
    }
    return (err_code != nvs_ok) ? ESP_FAIL : ESP_OK;
}

