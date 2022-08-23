#include "usart.h"

static const char* TAG = "USART";

esp_err_t uart_create(uart_init_t *init, uart_port_t uart_port, uint32_t tx_pin, uint32_t rx_pin, uint32_t baudrate, uint32_t event_queue_size, QueueHandle_t *event_queue ){
    if (init != NULL) {
        init->uart_port = uart_port;
        init->tx_pin = tx_pin;
        init->rx_pin = rx_pin;
        init->event_queue_size = event_queue_size;
        init->queue_handle = event_queue;
        init->uart_config.baud_rate = baudrate;
        init->uart_config.data_bits = UART_DATA_8_BITS;
        init->uart_config.parity    = UART_PARITY_DISABLE;
        init->uart_config.stop_bits = UART_STOP_BITS_1;
        init->uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        init->uart_config.source_clk = UART_SCLK_APB;
        if (uart_driver_install(init->uart_port, RX_BUFFER_SIZE, TX_BUFFER_SIZE, (int)event_queue_size, event_queue, 0) != ESP_OK) {
            ESP_LOGE(TAG, "install uart driver failed");
            return ESP_FAIL;
        }
        if (uart_param_config(init->uart_port, &init->uart_config) != ESP_OK) {
            ESP_LOGE(TAG, "config uart parameter failed");
            return ESP_FAIL;
        }
        if (uart_set_pin(init->uart_port, init->tx_pin, init->rx_pin,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
            ESP_LOGE(TAG, "config uart gpio failed");
            return ESP_FAIL;
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}
int uart_send_data(uart_port_t uart_num, const void* src, size_t size){
    return uart_write_bytes(uart_num, src, size);
}