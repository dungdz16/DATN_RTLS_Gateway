#include "uart.h"
#define TAG "UART"

receivedDataUARTEvent receivedDataUARTCallback = NULL;

esp_err_t UARTRXTaskRegister(receivedDataUARTEvent cb)
{
    if (cb != NULL)
    {
        receivedDataUARTCallback = cb;
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void rx_task(void *pParameter)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUFFER_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUFFER_SIZE, 1000 / portTICK_RATE_MS);
        if (rxBytes > 0) {

            data[rxBytes] = 0;
            // ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            // ESP_LOG_BUFFER_HEX(RX_TASK_TAG, data, rxBytes);
            receivedDataUARTCallback(data, rxBytes);
        }
    }
    free(data);
}
int uart_send_data(uart_port_t uart_num, const void* src, size_t size)
{
    const int txBytes = uart_write_bytes(uart_num, src, size);
    ESP_LOGI("TX_UART", "Wrote %d bytes", txBytes);
    return txBytes;
}

esp_err_t uart_create(uart_init_t *init)
{
    if (init != NULL)
    {
        if (uart_driver_install(init->uart_port, RX_BUFFER_SIZE , TX_BUFFER_SIZE, 0, NULL, 0) != ESP_OK)
        {
            ESP_LOGE(TAG, "UART Driver Install Fail in");
            return ESP_FAIL;
        }
    uart_param_config(init->uart_port, &init->uart_config);
    uart_set_pin(init->uart_port, init->tx_pin, init->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, RX_TASK_PRIORITY, NULL);
    return ESP_OK;
    }
    return ESP_FAIL;
}