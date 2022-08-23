#ifndef __USART_H
#define __USART_H

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
// #include "task.h"
// #include "FreeRTOS.h"

#define TX_BUFFER_SIZE  512
#define RX_BUFFER_SIZE  512
typedef struct {
    uart_port_t uart_port;        /*!< UART port number */
    uint32_t rx_pin;              /*!< UART Rx Pin number */
    uint32_t tx_pin;
    uart_config_t uart_config;    /*!< UART specific configuration */
    uint32_t event_queue_size;    /*!< UART event queue size */  
    QueueHandle_t *queue_handle;  /*!< UART event queue handle */                         
} uart_init_t;
esp_err_t uart_create(uart_init_t *init, uart_port_t uart_port, uint32_t tx_pin, uint32_t rx_pin, uint32_t baudrate, uint32_t event_queue_size, QueueHandle_t *event_queue );
int uart_send_data(uart_port_t uart_num, const void* src, size_t size);

uint8_t Util_CalculateChecksum(uint8_t *buf, uint32_t len);
#endif