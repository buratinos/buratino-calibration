    /* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_adc_cal.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/dac.h"

#include "ds18b20.h"

#define ADC2_LIGHT_CHANNEL ADC2_CHANNEL_2
#define ADC2_HUM_CHANNEL ADC2_CHANNEL_3
#define ADC2_TEMP_CHANNEL ADC2_CHANNEL_9
#define ADC2_SOIL_CHANNEL_L ADC2_CHANNEL_7
#define ADC2_SOIL_CHANNEL_H ADC2_CHANNEL_5

#define BLINK_GPIO 18
#define LED_GPIO 26

void app_main()
{
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("Wake up from GPIO %d\n", pin);
            } else {
                printf("Wake up from GPIO\n");
            }
            break;
        }
        default:
            printf("Not a deep sleep reset\n");
    }

    /* Print chip information */

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    /* init sensor GPIOs */

    int read_raw_light, read_raw_hum, read_raw_temp, read_l, read_h;
    esp_err_t r;
    gpio_num_t adc_gpio_num;

    r = adc2_pad_get_io_num( ADC2_LIGHT_CHANNEL, &adc_gpio_num );
    assert( r == ESP_OK );

    printf("ADC channel %d @ GPIO %d.\n", ADC2_LIGHT_CHANNEL, adc_gpio_num );
    printf("adc2_init...\n");
    adc2_config_channel_atten( ADC2_LIGHT_CHANNEL, ADC_ATTEN_DB_11 );
    adc2_config_channel_atten( ADC2_HUM_CHANNEL, ADC_ATTEN_DB_11 );
    adc2_config_channel_atten( ADC2_SOIL_CHANNEL_L, ADC_ATTEN_DB_11 );
    adc2_config_channel_atten( ADC2_SOIL_CHANNEL_H, ADC_ATTEN_DB_11 );

    ds18b20_init(LED_GPIO);

    float temp = ds18b20_get_temp() * 100; // 0.2f to int
    read_raw_temp = (int)temp;

    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    /* reading sensors */

    vTaskDelay(2 * portTICK_PERIOD_MS);
    printf("start reading sensors..\n");

    for (int i = 0; i < 3;   i++) {
        gpio_set_level(BLINK_GPIO, 1);

        r = adc2_get_raw( ADC2_LIGHT_CHANNEL, ADC_WIDTH_12Bit, &read_raw_light);
        r = adc2_get_raw( ADC2_HUM_CHANNEL, ADC_WIDTH_12Bit, &read_raw_hum);
        r = adc2_get_raw( ADC2_SOIL_CHANNEL_L, ADC_WIDTH_12Bit, &read_l);
        r = adc2_get_raw( ADC2_SOIL_CHANNEL_H, ADC_WIDTH_12Bit, &read_h);

        temp = ds18b20_get_temp() * 100; // 0.2f to int
        read_raw_temp = (int)temp;

        if ( r == ESP_OK ) {
            printf("Light: %d, fert: %d, temp: %d, L: %d, H: %d, DIFF: %d\n", read_raw_light, read_raw_hum, read_raw_temp, read_l, read_h, read_l - read_h);
        } else if ( r == ESP_ERR_INVALID_STATE ) {
            printf("%s: ADC2 not initialized yet.\n", esp_err_to_name(r));
        } else {
            printf("%s\n", esp_err_to_name(r));
        }

        vTaskDelay( 3 * portTICK_PERIOD_MS );
        gpio_set_level(BLINK_GPIO, 0);

        vTaskDelay( 3 * portTICK_PERIOD_MS );
    }

    /* enabling button wakeup */

    const int ext_wakeup_pin = 14;
    const uint64_t ext_wakeup_pin_mask = 1ULL << ext_wakeup_pin;

    printf("Enabling EXT1 wakeup on pin GPIO%d\n", ext_wakeup_pin);
    esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_mask, ESP_EXT1_WAKEUP_ANY_HIGH);

    printf("Going to sleep...\n");
    esp_deep_sleep_start();
}
