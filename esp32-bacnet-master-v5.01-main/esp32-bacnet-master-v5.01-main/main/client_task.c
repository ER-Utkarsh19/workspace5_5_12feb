#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "client_task.h"
#include "led.h"

#include "config.h"
#include "address.h"
#include "bacdef.h"
#include "handlers.h"
#include "client.h"
#include "dlenv.h"
#include "bacdcode.h"
#include "npdu.h"
#include "apdu.h"
#include "iam.h"
#include "tsm.h"
#include "datalink.h"
#include "dcc.h"
#include "getevent.h"
#include "net.h"
#include "txbuf.h"
#include "version.h"
#include "device.h"
#include "bactext.h"


#define CLIENT_DEVICE_ID CONFIG_CLIENT_DEVICE_ID

static const char *TAG = "client";

unsigned timeout = 100; /* milliseconds */
time_t last_seconds = 0;
bool found = false;
static BACNET_ADDRESS Target_Address;
static uint8_t request_id = 0;
time_t elapsed_seconds = 0;
unsigned max_apdu = 0;
time_t timeout_seconds = 0;
time_t current_seconds = 0;
uint16_t pdu_len = 0;
BACNET_ADDRESS src = { 0 };


void client_task(void *arg)
{
    gpio_config_t input_config;
    input_config.intr_type = GPIO_INTR_DISABLE;
    input_config.mode = GPIO_MODE_INPUT;
    //input_config.pin_bit_mask = (1ULL << GPIO_NUM_0);
    input_config.pin_bit_mask = (1ULL << CONFIG_BUTTON_GPIO);
    input_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    input_config.pull_up_en = GPIO_PULLUP_ONLY;
    gpio_config(&input_config);

    last_seconds = time(NULL);
    timeout_seconds = (apdu_timeout() / 1000) * apdu_retries();
    /* try to bind with the device */
    found = address_bind_request(
        CLIENT_DEVICE_ID, &max_apdu, &Target_Address);
    if (!found) {
        Send_WhoIs(CLIENT_DEVICE_ID, CLIENT_DEVICE_ID);
    }

// Add this variable above the while(true) loop to track the button
int last_button_state = -1; 

while(true)
{
    current_seconds = time(NULL);

    if (current_seconds != last_seconds) {
        tsm_timer_milliseconds((uint16_t)((current_seconds - last_seconds) * 1000));
    }

    if (!found) {
        found = address_bind_request(CLIENT_DEVICE_ID, &max_apdu, &Target_Address);
        if (!found && (current_seconds % 5 == 0)) {
            ESP_LOGW(TAG, "Searching for Server %d...", CLIENT_DEVICE_ID);
            Send_WhoIs(CLIENT_DEVICE_ID, CLIENT_DEVICE_ID);
            elapsed_seconds = 0; 
        }
    }

    if (found) {
        // Read the physical button (Active Low: 0=Pressed, 1=Released)
        int current_button_state = !gpio_get_level(CONFIG_BUTTON_GPIO);

        // ONLY send if the state has changed (Prevents flooding)
        if (current_button_state != last_button_state) {
            
            BACNET_APPLICATION_DATA_VALUE application_data = {
                .tag = BACNET_APPLICATION_TAG_REAL,
                .type.Real = (float)current_button_state,
                .context_specific = false
            };  

            ESP_LOGI(TAG, "Button State Changed: %d. Sending Write...", current_button_state);

            request_id = Send_Write_Property_Request(
                CLIENT_DEVICE_ID, OBJECT_ANALOG_VALUE, 0, 
                PROP_PRESENT_VALUE, &application_data, 16, BACNET_ARRAY_ALL
            );

            if (request_id > 0) {
                ESP_LOGI(TAG, "Success! Request ID: %d", request_id);
            }
            
            // Update last state
            last_button_state = current_button_state;
        }
    }

    last_seconds = current_seconds;
    // Delay 100ms to keep the button responsive but quiet the CPU
    vTaskDelay(pdMS_TO_TICKS(100)); 
}
}