#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "button.h"

#define TAG "BUTTON"

typedef enum{
    BUTTON_INTERNAL_RELEASED= 1,
    BUTTON_INTERNAL_PRESSED,
    BUTTON_INTERNAL_LONG_PRESS
}button_internal_state_t;
typedef struct
{
    gpio_num_t pin;
    button_internal_state_t state;
    bool inverted;
    uint16_t history;
    uint64_t down_time;
} debounce_t;


static void update_button(debounce_t *d)
{
    d->history = (d->history << 1) | gpio_get_level(d->pin);
}

#define MASK 0b1111000000111111
static bool button_rose(debounce_t *d)
{
    if ((d->history & MASK) == 0b0000000000111111)
    {
        d->history = 0xffff;
        return 1;
    }
    return 0;
}
static bool button_fell(debounce_t *d)
{
    if ((d->history & MASK) == 0b1111000000000000)
    {
        d->history = 0x0000;
        return 1;
    }
    return 0;
}
static bool button_down(debounce_t *d)
{
    if (d->inverted)
        return button_fell(d);
    return button_rose(d);
}
static bool button_up(debounce_t *d)
{
    if (d->inverted)
        return button_rose(d);
    return button_fell(d);
}

#define LONG_PRESS_DURATION (2000)

static uint32_t millis()
{
    return esp_timer_get_time() / 1000;
}

static void send_event(QueueHandle_t queue, debounce_t db, int ev)
{
    if(queue){
            
        button_event_t event = {
            .pin = db.pin,
            .event = ev,
        };
        xQueueSend(queue, &event, portMAX_DELAY);
    }
}

static void button_task(void *pvParameter)
{
    ESP_LOGI(TAG,"btn task start");
    button_info_t *btn_info = (button_info_t *)pvParameter;
    ESP_LOGI(TAG,"pin %u",btn_info->pin_list[0]);
    
    // Initialize global state and queue
    debounce_t *debounce = calloc(btn_info->pin_count, sizeof(debounce_t));

    for (int index = 0; index < btn_info->pin_count; index++)
    {
        ESP_LOGI(TAG, "Registering button input: %d", btn_info->pin_list[index]);
        debounce[index].pin = btn_info->pin_list[index];
        debounce[index].down_time = 0;
        debounce[index].inverted = btn_info->inverted;
        debounce[index].state = BUTTON_INTERNAL_RELEASED;
        
        if (debounce[index].inverted)
            debounce[index].history = 0xffff;
    }

    ESP_LOGI(TAG,"btn task loop start");
    while (1)
    {
        for (int index = 0; index < btn_info->pin_count; index++)
        {
            update_button(&debounce[index]);
            // Criteria for a press to be called long_press
            if (debounce[index].down_time && (millis() - debounce[index].down_time > btn_info->long_press_time_ms))
            {
                if (button_up(&debounce[index]))
                {
                    if(debounce[index].state == BUTTON_INTERNAL_LONG_PRESS){
                        debounce[index].down_time = 0;
                        debounce[index].state = BUTTON_INTERNAL_RELEASED;
                        //ESP_LOGI(TAG, "%d UP", debounce[index].pin);
                        send_event(btn_info->queue, debounce[index], BUTTON_FALLING_EDGE_LONG);
                    }else{
                        debounce[index].down_time = 0;
                        debounce[index].state = BUTTON_INTERNAL_RELEASED;
                        //ESP_LOGI(TAG, "%d UP", debounce[index].pin);
                        send_event(btn_info->queue, debounce[index], BUTTON_FALLING_EDGE);
                    }
                }else if(debounce[index].state == BUTTON_INTERNAL_PRESSED){
                    debounce[index].state = BUTTON_INTERNAL_LONG_PRESS;
                    //ESP_LOGI(TAG, "%d LONG", debounce[index].pin);
                    send_event(btn_info->queue, debounce[index], BUTTON_LONG_PRESS);
                }
            }
            else if (button_down(&debounce[index]))
            {
                if(debounce[index].state == BUTTON_INTERNAL_RELEASED){
                    debounce[index].state = BUTTON_INTERNAL_PRESSED;
                    debounce[index].down_time = millis();
                    //ESP_LOGI(TAG, "%d DOWN", debounce[index].pin);
                    send_event(btn_info->queue, debounce[index], BUTTON_RISING_EDGE);
                }
            }
            else if (button_up(&debounce[index]))
            {
                if(debounce[index].state != BUTTON_INTERNAL_RELEASED){
                    debounce[index].down_time = 0;
                    debounce[index].state = BUTTON_INTERNAL_RELEASED;
                    //ESP_LOGI(TAG, "%d UP", debounce[index].pin);
                    send_event(btn_info->queue, debounce[index], BUTTON_FALLING_EDGE);
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
QueueHandle_t button_create_queue(void)
{
    return xQueueCreate(4, sizeof(button_event_t));
}

esp_err_t button_init(button_info_t *info, gpio_num_t *pin_select)
{
    return pulled_button_init(info, pin_select, GPIO_FLOATING);
}

esp_err_t button_set_queue(button_info_t *info, QueueHandle_t queue)
{
    esp_err_t err = ESP_OK;
    if (info)
    {
        info->queue = queue;
    }
    else
    {
        ESP_LOGE(TAG, "info is NULL");
        err = ESP_ERR_INVALID_ARG;
    }
    return err;
}

esp_err_t pulled_button_init(button_info_t *info, gpio_num_t *pin_select, gpio_pull_mode_t pull_mode)
{
    esp_err_t err = ESP_OK;
    if (!info)
    {
        ESP_LOGE(TAG, "info is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (info->pin_count != 0)
    {
        ESP_LOGE(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Scan the pin map to determine number of pins and create mask
    uint64_t pin_select_mask = 0;
    info->pin_count = 0;
    for (int inx = 0; inx <= 39; inx++)
    {
        ESP_LOGI(TAG,"%u",pin_select[inx]);
        if (pin_select[inx] == GPIO_NUM_NC)
        {
            break;
        }
        if ((0 <= pin_select[inx]) && (39 >= pin_select[inx]))
        {
        ESP_LOGI(TAG,"%u check ok",pin_select[inx]);
            if(pin_select_mask & (1ULL << pin_select[inx])){
                ESP_LOGE(TAG, "Pin occurs twich in input");
            }else{
                pin_select_mask |= (1ULL << pin_select[inx]);
                info->pin_count++;
            }
        }
    }
    // Here the pin_names array is created
    pin_select_mask = 0;
    uint8_t c_index = 0;
    info->pin_list = calloc(info->pin_count,sizeof(gpio_num_t));
    for (int inx = 0; inx <= 39; inx++)
    {
        if (pin_select[inx] == GPIO_NUM_NC)
        {
            break;
        }
        if ((0 <= pin_select[inx]) && (39 >= pin_select[inx]))
        {
            if(pin_select_mask & (1ULL << pin_select[inx])){
                ESP_LOGE(TAG, "Pin occurs twich in input");
            }else{
                info->pin_list[c_index++] = pin_select[inx]; 
                pin_select_mask |= (1ULL << pin_select[inx]);
            }
        }
    }
    ESP_LOGI(TAG,"%llu test",pin_select_mask);

    // Configure the pins
    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = (pull_mode == GPIO_PULLUP_ONLY || pull_mode == GPIO_PULLUP_PULLDOWN);
    io_conf.pull_down_en = (pull_mode == GPIO_PULLDOWN_ONLY || pull_mode == GPIO_PULLUP_PULLDOWN);
    
    io_conf.pin_bit_mask = pin_select_mask;
    err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO init failed");
        return err;
    }
    if (info->long_press_time_ms <= 0){
        info->long_press_time_ms = 2000;
    }
    // Spawn a task to monitor the pins
    xTaskCreate(&button_task, "button_task", 4096, (void *)info, 10, info->task_handle);
    return err;
}