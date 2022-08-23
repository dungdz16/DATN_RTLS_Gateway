#include <stdio.h>
#include "command.h"


uint8_t CalculateChecksum(uint8_t *buf, uint32_t len) 
{
	uint8_t csum = 0;
	uint32_t i = 0;
	for (i = 0; i < len; i++) {
		csum ^= buf[i];
	}
	return csum;
}
uint32_t Cal_CheckSum32(uint8_t *buf, uint32_t len){
    uint32_t csum = 0;
    uint32_t i  = 0;
    for (i = 0; i < len; i++){
        csum += buf[i];
    }
    return csum;
}
req_err_t esp_write_reg(esp_mcu_t *request, uint8_t address, uint8_t value, uint8_t retry){
    uint8_t *Tx_buffer = malloc(9);
    static pkg_header_t header = {START, 5, {TYPE_WRITE_REG, ESP_TO_MCU}};
    static uint8_t count = 0;
    request->tx.header = header;
    request->retry = retry;
    Tx_buffer[0] =  header.start;
    Tx_buffer[1] = (uint8_t)header.length;
    Tx_buffer[2] = (uint8_t)(header.length>>8);
    Tx_buffer[3] = header.type.type_l;
    Tx_buffer[4] = header.type.type_h;
    Tx_buffer[5] = address;
    Tx_buffer[6] = value;
    Tx_buffer[7] = CalculateChecksum(Tx_buffer,7);
    Tx_buffer[8] = STOP;
    request->tx.csum = Tx_buffer[7];
    request->tx.stop = STOP;
    request->send_status = SENDING;
    for(count = 0; count < request->retry; count++){
        Logi("LOG","ESP ----> MCU [WRITE REG ADDR 0x%.2x VALUE 0x%.2x] try %d",address, value, count);
        request->rx.header.type.type_l = 0;
        request->rx.header.type.type_h = 0;
        uart_send_data(request->uart->uart_port,Tx_buffer,9);
        request->tick = (uint32_t) xTaskGetTickCount() + 1000;
        while ((request->rx.header.type.type_l == 0)&&(request->rx.header.type.type_h == 0)){
            vTaskDelay(10);
            if (request->tick < xTaskGetTickCount()) break;
        }
        if ((request->rx.header.type.type_l == TYPE_WRITE_REG)&&(request->rx.header.type.type_h == MCU_TO_ESP)){
            // Logi("LOG","ESP <---- MCU [WRITE REG VALUE OK]");
            // for (int i = 0; i < request->rx_data_size; i++) printf("0x%.2x ",request->rx.data[i]);
            // printf("\r\n");
            request->send_status = NO_SEND;
            free(Tx_buffer);
            return REQ_OK;
        }     
    }
    Loge("LOG","Time out");
    request->send_status = NO_SEND;
    free(Tx_buffer);
    return REQ_FAIL;

}
uint16_t esp_response(esp_mcu_t *request, uint8_t type_l, uint8_t *data, uint8_t data_len){
    uint8_t *Tx_buffer = malloc(32);
    static pkg_header_t header = {START, 4, {0, ESP_TO_MCU}};  
    uint16_t i = 0;
    uint16_t tx_len = 0;
    header.type.type_l = type_l;
    header.length = data_len;
    request->tx.header = header;
    Tx_buffer[0] =  header.start;
    Tx_buffer[1] = (uint8_t)header.length;
    Tx_buffer[2] = (uint8_t)(header.length>>8);
    Tx_buffer[3] = header.type.type_l;
    Tx_buffer[4] = header.type.type_h;
    for (i=0; i < data_len - 3; i++){
        Tx_buffer[i+5] = data[i];
    }
    Tx_buffer[i+5] = CalculateChecksum(Tx_buffer,i+5);
    Tx_buffer[i+6] = STOP;
    request->tx.csum = Tx_buffer[i+5];
    request->tx.stop = STOP;
    tx_len = (uint16_t)uart_send_data(request->uart->uart_port,Tx_buffer,data_len+4);
    free(Tx_buffer);
    return tx_len;
}
