# Button press detector

## Changes in Fork

Forked from [craftmetrics's implementation](https://github.com/craftmetrics/esp32-button).

This fork introduces the following changes:
- Can create multilple instances via structs and each one gets thier own queue.
- include gaurds and c++ ready.


## Available input GPIO pins

Only the following pins can be used as inputs on the ESP32:

0-19, 21-23, 25-27, 32-39


## Example Usage

```
button_info_t button_info = {0};

gpio_num_t gpio_arr[...] = {GPIO_NUM_X, ...};
ESP_ERROR_CHECK(button_init(&button_info, gpio_arr));

QueueHandle_t button_event_queue = button_create_queue();
ESP_ERROR_CHECK(button_set_queue(&button_info, button_event_queue));

button_event_t ev;

while (true) {
    if (xQueueReceive(button_event_queue, &ev, 1000/portTICK_PERIOD_MS)) {
        if ((ev.pin == GPIO_NUM_X) && (ev.event == BUTTON_RISING_EDGE)) {
            // ...
        }
        if ((ev.pin == GPIO_NUM_X) && (ev.event == BUTTON_FALLING_EDGE_LONG)) {
            // ...
        }
    }
}
```

## Event Types
All events contain which pin the event has occured on.
### BUTTON_NOTHING

Blank event, used as "Null"
### BUTTON_RISING_EDGE
Event triggered immediately button is pressed down. 
### BUTTON_LONG_PRESS
Event is triggered when a button has been pressed for the period specified a long press. Note that the button may still be pressed down.
### BUTTON_FALLING_EDGE
Event is triggered when button is released.
### BUTTON_FALLING_EDGE_LONG
Event is triggered when button is released after being push down long enough to trigger a BUTTON_LONG_PRESS.
