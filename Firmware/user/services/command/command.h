
#include "usart.h"
#include "print_log.h"

#define REG_COMMAND     0x01
#define REG_FIRMWARE    0x02
#define REG_HARDWARE    0x03
#define REG_SWITCH      0x0A
#define REG_MODE    0x0B
#define REG_SPEED   0x0C
#define REG_TIMER   0x0D

#define INS_NOP     0x00
#define INS_RESET   0x01
#define INS_REBOOT  0x02
#define INS_RD_ALL_REG  0x03
#define INS_RD_ALL_PARAM    0x04

#define TYPE_WRITE_REG  0x0C
#define TYPE_RS_WIFI    0x40
#define TYPE_STATUS_WIFI    0x41

#define ESP_TO_MCU 0x00
#define MCU_TO_ESP 0x01

#define START 0xAA
#define STOP 0x55

typedef enum{
    WIFI_DISCONNECTED, WIFI_CONNECTED, CLOUD_CONNECTED,
}esp_connection_t;
typedef enum{
    REQ_OK, REQ_FAIL,
}req_err_t;

typedef struct{
    uint8_t type_l;
    uint8_t type_h;
}type_t;
typedef struct{
    uint8_t start;
    uint16_t length;
    type_t type;
}pkg_header_t;
typedef struct {
    pkg_header_t header;
    uint8_t data[15];
    uint8_t csum;
    uint8_t stop;
}pkg_t;
typedef enum{
    NO_SEND, SENDING,
}sent_status_t;
typedef struct{
    pkg_t tx;
    pkg_t rx;
    uint16_t rx_data_size;
    uart_init_t *uart;
    sent_status_t send_status; 
    uint8_t retry;
    uint32_t tick;
}esp_mcu_t;

req_err_t esp_write_reg(esp_mcu_t *request, uint8_t address, uint8_t value, uint8_t retry);
uint16_t esp_response(esp_mcu_t *request, uint8_t type_l, uint8_t *data, uint8_t data_len);
uint8_t CalculateChecksum(uint8_t *buf, uint32_t len) ;