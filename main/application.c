#include "Global_types.h"
#include "networking.h"
#include "HTTP_server.h"
#include "esp_ota_ops.h"

void app_main(void)
{
    nvs_flash_init();
    xTaskCreate(networking_task, "Networking task", 4096, NULL, 5, NULL);
    xTaskCreate(display_device_status, "Display status", 1024, NULL, 5, NULL);
    xTaskCreate(http_listener_task, "HTTP server", 8192, NULL, 5, NULL);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_partition_t* running = esp_ota_get_running_partition();
    printf("\n\n%lx\n\n", running->address);
    esp_partition_t* ota0 = esp_ota_get_next_update_partition(NULL);
    printf("\n\n%lx\n\n", ota0->address);
    esp_ota_handle_t update_handle = 0;
    ESP_ERROR_CHECK(esp_ota_begin(ota0, OTA_SIZE_UNKNOWN, &update_handle));
    printf("Begin was successful");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(esp_ota_abort(update_handle));
    printf("Abort successful");
}
