#include "command.h"
#include "uart.h"
#define TAG "COMMAND"
static void UARTConfig()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .parity = UART_PARITY_DISABLE, 
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
        .data_bits = UART_DATA_8_BITS,
    };
    uart_init_t init = {
        .rx_pin = RX_PIN,
        .tx_pin = TX_PIN,
        .uart_config = uart_config,
        .uart_port = UART_NUM_1,
    };
    uart_create(&init);
}

static void RXHandle(uint8_t* data, size_t size)
{
    ESP_LOGI(TAG, "Read %d bytes: '%s'", size, data);
    ESP_LOG_BUFFER_HEX(TAG, data, size);
}

int dwm_pos_get(dwm_pos_t* p_pos)
{
    
}

esp_err_t DWM1001Init()
{
    UARTConfig();
    UARTRXTaskRegister(RXHandle);
}