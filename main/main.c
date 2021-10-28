/* WiFi station Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "wifi.h"
#include "udp_server.h"
#include "fire.h"

static const char *TAG = "main";

QueueHandle_t ledCommand_queue;

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ledCommand_queue = xQueueCreate( 10, sizeof(command_t));
    xTaskCreate(fire_task, "fire", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    xTaskCreate(udp_server_task, "udp_server", 4096, (void*)AF_INET, 5, NULL);

    vTaskSuspend( NULL );
}
