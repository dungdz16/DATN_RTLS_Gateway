#include "syslog.h"

#define USE_SYSLOG  
syslog_t syslog_part;
static const char *TAG = "syslog";
static const esp_partition_t* find_syslog_partition(void){
    esp_err_t err = nvs_flash_init();
    ESP_ERROR_CHECK(err);
    esp_partition_iterator_t esp_partition_it = esp_partition_find(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,"log_data");
    if (esp_partition_it == NULL){
        Loge(TAG,"Error find Syslog partition");
        return 0;
    }
    const esp_partition_t *syslog_partition = esp_partition_get(esp_partition_it);
    Logi(TAG,"SysLog partition found: %s ,0x%.4x ,0x%.4x  ",syslog_partition->label,syslog_partition->address,syslog_partition->size);
    esp_partition_iterator_release(esp_partition_it);
    return syslog_partition;
}

static int32_t find_offset_address(const esp_partition_t *partition){
    uint32_t timestamp = 0;
    uint8_t record_buffer[8] ={0};
    uint32_t offset = 0;
    memset(record_buffer,0xff,8);
    while (timestamp != 0xffffffff){
        esp_partition_read(partition,offset,record_buffer,8);
        timestamp = (record_buffer[4]<<24) | (record_buffer[5] << 16) | (record_buffer[6] << 8) | record_buffer[7];
        if (offset + 8 > partition->size) break;
        offset += 8;
    }
    return offset-8;
}

void save_one_record(uint8_t *data){
    if (syslog_part.syslog_partition == NULL) return;
    
    Logi(TAG,"Write next record at offset %u",syslog_part.offset_address);
    if (syslog_part.offset_address % 4096 == 0){
        Logi(TAG,"Erase One Page of Syslog Partition [4096 Bytes]");
        esp_partition_erase_range(syslog_part.syslog_partition,syslog_part.offset_address,4096);
    }    
    esp_partition_write(syslog_part.syslog_partition,syslog_part.offset_address,data,8);
    if (syslog_part.offset_address > syslog_part.syslog_partition->size) syslog_part.offset_address = 0;
    else syslog_part.offset_address +=8;
    syslog_part.block_index = syslog_part.offset_address/128;
}

void read_block_index(uint32_t block_index, uint8_t *data){
    if (syslog_part.syslog_partition == NULL) return;
    Logi(TAG,"Read block ID = %u",block_index);
    esp_partition_read(syslog_part.syslog_partition,block_index*128,data,128);
}

void syslog_partition_init(){
    syslog_part.syslog_partition = find_syslog_partition();
    if (syslog_part.syslog_partition == NULL) syslog_part.instance = false;
    else syslog_part.instance = true;
    if (syslog_part.instance){
        syslog_part.offset_address = find_offset_address(syslog_part.syslog_partition);
        syslog_part.block_index = syslog_part.offset_address/128;
        Logi(TAG,"Empty location to Save data: Offset %u, Block index %d",syslog_part.offset_address, syslog_part.block_index);
    }
}
uint32_t get_block_index(void){
    return syslog_part.block_index;
}
void save_log(uint8_t cause, uint16_t value, uint8_t extra, time_t timestamp){
    #ifdef USE_SYSLOG
    uint8_t buffer[8] = {0};
    buffer[0] = cause;
    buffer[1] = value >> 8;
    buffer[2] = value & 0xFF;
    buffer[3] = extra;
    *(uint32_t *)&buffer[4] = timestamp; 
    save_one_record(buffer);
    #endif
}