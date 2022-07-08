#ifndef _UART_H_
#define _UART_H_

#include <stdio.h>
#include "string.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define TX_BUFFER_SIZE  512
#define RX_BUFFER_SIZE  512
#define RX_TASK_PRIORITY (1)
typedef struct {
    uart_port_t uart_port;        /*!< UART port number */
    uint32_t rx_pin;              /*!< UART Rx Pin number */
    uint32_t tx_pin;
    uart_config_t uart_config;    /*!< UART specific configuration */
    uint32_t event_queue_size;    /*!< UART event queue size */  
    QueueHandle_t *queue_handle;  /*!< UART event queue handle */                         
} uart_init_t;
typedef void(*receivedDataUARTEvent)(uint8_t* data, size_t size);

esp_err_t uart_create(uart_init_t *init);
int uart_send_data(uart_port_t uart_num, const void* src, size_t size);
esp_err_t UARTRXTaskRegister(receivedDataUARTEvent cb);

#endif

