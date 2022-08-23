#ifndef __PRINT_LOG_H__
#define __PRINT_LOG_H__

#include "esp_log.h"

#define DEBUG_MONITOR

#ifdef DEBUG_MONITOR
#define Logi(tag,fmt, ...)   ESP_LOGI(tag,fmt, ##__VA_ARGS__)
#define Loge(tag,fmt, ...)   ESP_LOGE(tag,fmt, ##__VA_ARGS__)
#define Logd(tag,fmt, ...)   ESP_LOGD(tag,fmt, ##__VA_ARGS__)
#else 
#define Logi(...)   
#define Loge(...)   
#define Logd(...)
#endif

#endif