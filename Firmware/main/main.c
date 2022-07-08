#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "string.h"

#define BUF_SIZE (1024)
#define TAG "MAIN"
#define TX_PIN GPIO_NUM_25
#define RX_PIN GPIO_NUM_26

void app_main(void)
{
    
    uint8_t data[] = {0x02, 0x00};
    while(1)
    {
        
        uart_send_data(UART_NUM_1, (void*)data, 2);
        vTaskDelay(2000/portTICK_PERIOD_MS);
    }
}




