#ifndef BUTTON_DEBOUNCER_H
#define BUTTON_DEBOUNCER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    BUTTON_NOTHING=0,                 
    BUTTON_RISING_EDGE=1,
    BUTTON_LONG_PRESS=2,
    BUTTON_FALLING_EDGE=3,
    BUTTON_FALLING_EDGE_LONG=4
} button_state_t;


typedef struct {
	gpio_num_t pin;
    button_state_t event;
} button_event_t;

typedef struct
{
    gpio_num_t* pin_list;                   ///< Pointer to list of all assigned GPIO
    QueueHandle_t queue;                    ///< Quehandle for button events
    TaskHandle_t task_handle;               ///< Taskhandle to button task
    uint8_t pin_count;                      ///< Number of pins
    uint32_t long_press_time_ms;            ///< Time in ms for a press to be registered as a long press, defaults to 2000 if not set when process is started 
    bool inverted;                          ///< Inverted
} button_info_t;

#define GENERATE_BUTTON_INFO_T {.pin_list = NULL, .queue = NULL, .task_handle = NULL, .pin_count = 0, .long_press_time_ms = 0, .inverted =false, }
#define GENERATE_BUTTON_EVENT_T {.pin = GPIO_NUM_NC,.event= BUTTON_NOTHING,}

QueueHandle_t button_create_queue(void);
esp_err_t button_set_queue(button_info_t * info, QueueHandle_t queue);

esp_err_t button_init(button_info_t* info, gpio_num_t pin_select[]);
esp_err_t pulled_button_init(button_info_t* info, gpio_num_t* pin_select, gpio_pull_mode_t pull_mode);


#ifdef __cplusplus
}
#endif

#endif  // BUTTON_DEBOUNCER_H