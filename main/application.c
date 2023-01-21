#include "Global_types.h"
#include "networking.h"
#include "HTTP_server.h"
#include "storage_interface.h"
#include "monitor.h"

void app_main(void)
{
    nvs_flash_init();
    mount_drive();
    xTaskCreate(networking_task, "Networking task", 4096, NULL, 5, NULL);
    xTaskCreate(display_device_status, "Display status", 4096, NULL, 5, NULL);
    xTaskCreate(http_listener_task, "HTTP server", 8192, NULL, 5, NULL);
    set_up_monitor();
}
