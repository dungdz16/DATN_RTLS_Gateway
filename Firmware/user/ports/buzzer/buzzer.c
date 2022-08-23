#include "buzzer.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO       (17)
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0

static SemaphoreHandle_t sem_buzzer_beep;
static QueueHandle_t queue_buzzer_time;
void buzzer_task(void* parameter)
{
    uint32_t buzzer_on_time = 0;
    while(true)
    {
        if (xQueueReceive(queue_buzzer_time, &buzzer_on_time, portMAX_DELAY) == pdTRUE)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_HS_CH0_CHANNEL, 512);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_HS_CH0_CHANNEL);
            vTaskDelay(buzzer_on_time);
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_HS_CH0_CHANNEL, 0);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_HS_CH0_CHANNEL);
        }
    }
}
void buzzer_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
        .freq_hz = 4000,                      // frequency of PWM signal
        .speed_mode = LEDC_HS_MODE,           // timer mode
        .timer_num = LEDC_HS_TIMER,            // timer index
        .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    };
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_HS_CH0_CHANNEL,
        .duty = 0,
        .gpio_num = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_HS_TIMER,
    };
    ledc_channel_config(&ledc_channel);
    //sem_buzzer_beep = xSemaphoreCreateBinary();
    queue_buzzer_time = xQueueCreate(5, sizeof(uint32_t));
    xTaskCreate(buzzer_task,
                "buzzer coltrol",
                1024,
                NULL,
                12, 
                NULL);
    buzzer_beep();
    //xSemaphoreGive(sem_buzzer_beep);
}

void buzzer_beep()
{
    uint32_t buzzer_on_time = 100;
    xQueueSend(queue_buzzer_time, &buzzer_on_time, 10);
}

void buzzer_long_beep()
{
    uint32_t buzzer_on_time = 750;
    xQueueSend(queue_buzzer_time, &buzzer_on_time, 10);
}
