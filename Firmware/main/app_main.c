/*
 * app_main.c
 *
 *  Created on: Dec 09, 2021
 *      Author: ManhTH
 *      Celling Fan Wifi Product Firmware
 */

#include "app_main.h"
#include "driver/gpio.h"
#include "nvs_store.h"
#include "ws2812b.h"
#include "ble_nvs.h"
#include "buzzer.h"

#define LEDC_LS_TIMER          LEDC_TIMER_1
#define LEDC_LS_MODE           LEDC_LOW_SPEED_MODE

device_info_t device;

static xQueueHandle gpio_evt_queue = NULL;
static void device_param_init(device_info_t *device);
static void gpio0_init(void);
static void obtain_time(void);
static void initialize_sntp(void);
static void get_timestamp_task(void *param);
void app_sync_time();
extern void app_config_start(void);
extern void app_mqtt_client_start(void);
extern void app_ble_start(void);
extern void app_ethernet_start(void);

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}
static void gpio_task(void* arg)
{
    uint32_t io_num;
    int ret;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            ret = gpio_get_level(io_num);
            if (!ret) {
                vTaskDelay(2000);
                if (!gpio_get_level(io_num)) {
                    printf("Reset wifi setting\n");
                    esp_wifi_restore();
                    nvs_erase_brokerCfg();
                    nvs_eraseAll();
                    save_log(CAUSE_FACTORY_RST,0,0,device.time_stamp);
                    esp_restart();
                }
            }
        }
    }
}
static void get_timestamp_task(void *param)
{
    time_t now = 0;
    char strftime_buf[64] ={0};
    while(1){
        time(&now);
        localtime_r(&now, &device.time_info);
        device.time_stamp = now - 946659600;
        device.ts = (uint32_t)device.time_stamp;
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &device.time_info);
        if (esp_get_free_heap_size()<10000) save_log(CAUSE_HEAP_OVERFLOW,VALUE_SUCCESS,0,device.time_stamp);
        vTaskDelay(1000);
    }
}
static void time_sync_notification_cb(struct timeval *tv)
{
    save_log(CAUSE_APP_CONFIG,VALUE_SUCCESS,EXTRA_STNP_SYNC,device.time_stamp);
}
static void initialize_sntp(void)
{
    sntp_setservername(0,"time.digotech.net");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

static void obtain_time(void)
{
    char strftime_buf[64] ={0};
    time_t now = 0;
    int retry = 0;
    const int retry_count = 50;
    Logi("log", "Time synchronization start.");
    if(sntp_enabled()){
        sntp_stop();
    }
    initialize_sntp();
    setenv("TZ", "ICT-7", 1);
    tzset();
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        vTaskDelay(200);
        time(&now);
    }
    if (retry >= retry_count) save_log(CAUSE_APP_CONFIG,VALUE_FAIL,EXTRA_STNP_SYNC,device.time_stamp);
    localtime_r(&now, &device.time_info);
    device.time_stamp = now - 946659600;
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &device.time_info);
    Logi("log", "The current date/time in VietNam is: %s", strftime_buf);
}
void app_sync_time(){  
    obtain_time();
    xTaskCreate(get_timestamp_task,"Get_timestamp",2048,NULL,12,NULL);
}
static uint32_t read_chip_id(){
    uint8_t tmp[6]={0};
    esp_read_mac(tmp,ESP_MAC_WIFI_STA);
    sprintf(device.mac_addr_str,"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
                tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5]);        
    uint64_t mac_address64 = 0;
    uint64_t chipId = 0;
    for (int i=0; i<6; i++){
        mac_address64 |= (uint64_t)tmp[i] << (i*8);
    }
    for(int i=0; i<25; i=i+8) {
        chipId |= ((mac_address64 >> (40 - i)) & 0xff) << i;
    }
    Logi("log","Chip ID %lld",chipId);
    return (uint32_t)chipId; 
}
static void device_param_init(device_info_t *device){
    device->id = read_chip_id();
    nvs_find_partition();
    sprintf(device->product_id,"%s%u",MODEL,device->id);
    sprintf(device->mqtt_config.client_id,"%u",device->id);
    nvs_write_u32("FIRMWARE_VER",FIRMWARE_VERSION);
    nvs_write_u32("HARDWARE_VER",HARDWARE_VERSION);
    nvs_read_u32("FIRMWARE_VER",&device->firmware);
    nvs_read_u32("HARDWARE_VER",&device->hardware);
    device->rst_reason = esp_reset_reason();
    device->time_stamp = 0;
    save_log(CAUSE_BOOT_REASON,device->rst_reason,0,device->time_stamp);
    device->rssi = -50; //fake value
}

void app_main(void)
{
    Logi("log","Main APP running.");
    printf(INFO_FM, FIRMWARE_VERSION, HARDWARE_VERSION, HARDWARE_INFO,__DATE__, __TIME__);
    syslog_partition_init(); 
    led_init();
    buzzer_init();
    buzzer_beep();
    device_param_init(&device);
    gpio0_init();
    app_config_start();
    //app_ethernet_start();
    app_mqtt_client_start();
    ble_dwm1001_init();
}
static void gpio0_init(void){
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = GPIO_SEL_0;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_handler, (void*) GPIO_NUM_0);
}
