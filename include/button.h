#ifndef BUTTON_DEBOUNCER_H
#define BUTTON_DEBOUNCER_H

#include "freertos/queue.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIN_BIT(x) (1ULL<<x)

typedef enum {
    BUTTON_NOT_PRESSED=0,
    BUTTON_SHORT_PRESSED=1,
    BUTTON_LONG_PRESSED=2

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
} button_info_t;


QueueHandle_t button_create_queue(void);
esp_err_t button_set_queue(button_info_t * info, QueueHandle_t queue);

esp_err_t button_init(button_info_t* info, gpio_num_t pin_select[]);
esp_err_t pulled_button_init(button_info_t* info, gpio_num_t* pin_select, gpio_pull_mode_t pull_mode);


#ifdef __cplusplus
}
#endif

#endif  // BUTTON_DEBOUNCER_H