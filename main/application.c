#include "Global_types.h"
#include "networking.h"
#include "HTTP_server.h"
#include "esp_ota_ops.h"
#include "ota_update.h"

void app_main(void)
{
    nvs_flash_init();
    xTaskCreate(networking_task, "Networking task", 4096, NULL, 5, NULL);
    xTaskCreate(display_device_status, "Display status", 4096, NULL, 5, NULL);
    xTaskCreate(http_listener_task, "HTTP server", 8192, NULL, 5, NULL);
    find_update_partition();
}
