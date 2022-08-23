#include <stdio.h>
#include "nvs_store.h"
static const char *TAG = "nvs_store";
void nvs_find_partition(){
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    esp_partition_iterator_t esp_partition_it = esp_partition_find(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,"nvs");
    if (esp_partition_it == NULL){
        Loge(TAG,"Error find NVS partition");
    }
    else {
        const esp_partition_t *my_partition = esp_partition_get(esp_partition_it);
        Logi(TAG,"NVS partition found: %s ,0x%.4x ,0x%.4x  ",my_partition->label,my_partition->address,my_partition->size);
        esp_partition_iterator_release(esp_partition_it);
    }    
}
static void nvs_open_partition(nvs_handle_t *out_handle){
    nvs_open("storage", NVS_READWRITE, out_handle);
}

nvs_err_code nvs_read_str(char *key, char *data_out){
    nvs_handle_t nvs_handle;
    nvs_open_partition(&nvs_handle);
    esp_err_t err;
    nvs_err_code err_code;
    size_t len = 64;
    err = nvs_get_str(nvs_handle, key ,data_out,&len);
    switch (err) {
    case ESP_OK:
        Logi(TAG,"[\"%s\"] [\"%s\"]",key, data_out);
        nvs_close(nvs_handle);
        err_code = nvs_ok;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        Loge(TAG," [\"%s\"] not initialized ",key);
        err_code = nvs_not_init;
        break;
    default :
        Loge(TAG,"Error (%s) reading!", esp_err_to_name(err));
        err_code = nvs_error;
        break;
    }
    nvs_close(nvs_handle);
    return err_code;
}

nvs_err_code nvs_write_str(char *key, char *data){
    nvs_handle_t nvs_handle;
    nvs_open_partition(&nvs_handle);
    esp_err_t err;
    nvs_err_code err_code;
    err = nvs_set_str(nvs_handle,key,data);
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) 
    {
        Loge(TAG,"Update [\"%s\"] in NVS: Failed",key);
        err_code = nvs_error;
    }
    else 
    {
        Logi(TAG,"Update [\"%s\"] in NVS: Done",key);
        err_code = nvs_ok;
    }
    nvs_close(nvs_handle);
    return err_code;
}

nvs_err_code nvs_write_u16(char *key, uint16_t value){
    nvs_handle_t nvs_handle;
    nvs_open_partition(&nvs_handle);
    esp_err_t err;
    nvs_err_code err_code;
    err = nvs_set_u16(nvs_handle,key,value);
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) 
    {
        Loge(TAG,"Update [\"%s\"] in NVS: Failed",key);
        err_code = nvs_error;
    }
    else 
    {
        Logi(TAG,"Update [\"%s\"] in NVS: Done",key);
        err_code = nvs_ok;
    }
    nvs_close(nvs_handle);
    return err_code;
}

nvs_err_code nvs_read_u16(char *key, uint16_t *value){
    nvs_handle_t nvs_handle;
    nvs_open_partition(&nvs_handle);
    esp_err_t err;
    nvs_err_code err_code;
    err = nvs_get_u16(nvs_handle, key,value);
    switch (err) {
    case ESP_OK:
        Logi(TAG,"[\"%s\"] [\"%u\"]",key, *value);
        nvs_close(nvs_handle);
        err_code = nvs_ok;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        Loge(TAG," [\"%s\"] not initialized ",key);
        err_code = nvs_not_init;
        break;
    default :
        Loge(TAG,"Error (%s) reading!", esp_err_to_name(err));
        err_code = nvs_error;
    }
    nvs_close(nvs_handle);
    return err_code;
}

nvs_err_code nvs_write_u32(char *key, uint32_t value){
    nvs_handle_t nvs_handle;
    nvs_open_partition(&nvs_handle);
    esp_err_t err;
    nvs_err_code err_code;
    err = nvs_set_u32(nvs_handle,key,value);
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) 
    {
        Loge(TAG,"Update [\"%s\"] in NVS: Failed",key);
        err_code = nvs_error;
    }
    else 
    {
        Logi(TAG,"Update [\"%s\"] in NVS: Done",key);
        err_code = nvs_ok;
    }
    nvs_close(nvs_handle);
    return err_code;
}

nvs_err_code nvs_read_u32(char *key, uint32_t *value){
    nvs_handle_t nvs_handle;
    nvs_open_partition(&nvs_handle);
    esp_err_t err;
    nvs_err_code err_code;
    err = nvs_get_u32(nvs_handle, key,value);
    switch (err) {
    case ESP_OK:
        Logi(TAG,"[\"%s\"] [\"%d\"]",key, *value);
        nvs_close(nvs_handle);
        err_code = nvs_ok;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        Loge(TAG," [\"%s\"] not initialized ",key);
        err_code = nvs_not_init;
        break;
    default :
        Loge(TAG,"Error (%s) reading!", esp_err_to_name(err));
        err_code = nvs_error;
        break;
    }
    nvs_close(nvs_handle);
    return err_code;
}

nvs_err_code nvs_write_u64(char *key, uint64_t value){
    nvs_handle_t nvs_handle;
    nvs_open_partition(&nvs_handle);
    esp_err_t err;
    nvs_err_code err_code;
    err = nvs_set_u64(nvs_handle,key,value);
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) 
    {
        Loge(TAG, "Update [\"%s\"] in NVS: Failed", key);
        err_code = nvs_error;
    }
    else 
    {
        Logi(TAG,"Update [\"%s\"] in NVS: Done",key);
        err_code = nvs_ok;
    }
    nvs_close(nvs_handle);
    return err_code;
}

nvs_err_code nvs_read_u64(char *key, uint64_t *value){
    nvs_handle_t nvs_handle;
    nvs_open_partition(&nvs_handle);
    esp_err_t err;
    nvs_err_code err_code;
    err = nvs_get_u64(nvs_handle, key,value);
    switch (err) {
    case ESP_OK:
        Logi(TAG,"[\"%s\"] [\"%lld\"]",key, *value);
        nvs_close(nvs_handle);
        err_code = nvs_ok;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        Loge(TAG," [\"%s\"] not initialized ",key);
        err_code = nvs_not_init;
        break;
    default :
        Loge(TAG,"Error (%s) reading!", esp_err_to_name(err));
        err_code = nvs_error;
        break;
    }
    nvs_close(nvs_handle);
    return err_code;
}

nvs_err_code nvs_eraseKey(char *key){
    nvs_handle_t nvs_handle;
    nvs_open_partition(&nvs_handle);
    nvs_err_code err_code;
    err_code = nvs_erase_key(nvs_handle,key);
    nvs_commit(nvs_handle);
    if (err_code != ESP_OK) 
    {
        Loge(TAG,"Erase [\"%s\"] in NVS: Failed",key);
        err_code =  nvs_error;
    }
    else
    { 
        Logi(TAG,"Erase [\"%s\"] in NVS: Done",key);
        err_code = nvs_ok;
    }
    nvs_close(nvs_handle);
    return err_code;
}
nvs_err_code nvs_eraseAll(void){
    nvs_handle_t nvs_handle;
    nvs_open_partition(&nvs_handle);
    esp_err_t err;
    nvs_err_code err_code;
    err = nvs_erase_all(nvs_handle);
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) 
    {
        Loge(TAG,"Erase all key in NVS: Failed");
        err_code = nvs_error;
    }
    else 
    {
        Logi(TAG,"Erase all key in NVS: Done");
        err_code= nvs_ok;
    }
    nvs_close(nvs_handle);
    return err_code;
}
void nvs_erase_brokerCfg(void){
    nvs_eraseKey("SSID");
    nvs_eraseKey("PASSWORD");
    nvs_eraseKey("HOST");
    nvs_eraseKey("PROJECT");
    nvs_eraseKey("LOCATION");
    nvs_eraseKey("REGISTRY");
    nvs_eraseKey("PORT");
    nvs_eraseKey("CA_LEN");
    nvs_eraseKey("PRVKEY_LEN");
}
const esp_partition_t * custom_partition_init(char * name){
    esp_err_t err = nvs_flash_init();
    ESP_ERROR_CHECK(err);
    esp_partition_iterator_t esp_partition_it = esp_partition_find(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,name);
    if (esp_partition_it == NULL){
        Loge(TAG,"Error find \"%s\" partition",name);
        return NULL;
    }
    const esp_partition_t * partition = esp_partition_get(esp_partition_it);
    // Logi(TAG,"\"%s\" partition found: %s ,0x%.4x ,0x%.4x  ",name, partition->label,partition->address,partition->size);
    esp_partition_iterator_release(esp_partition_it);
    return partition;
}
void custom_partition_write(const esp_partition_t *partition,size_t offset, uint8_t *data, size_t data_length){
    if (partition!=NULL)
        Logi("log","write %d byte to partition",data_length);
        esp_partition_write(partition,offset,data,data_length);
    return;
}
void custom_partition_read(const esp_partition_t *partition,size_t offset, uint8_t *buffer, size_t buffer_length){
    if (partition!=NULL)
        esp_partition_read(partition,offset,buffer,buffer_length);
    return;
}
void custom_partition_erase(const esp_partition_t *partition,size_t offset, size_t size){
    if (partition!=NULL)
        esp_partition_erase_range(partition,offset,size);
    return;
}