#pragma once
#include "Global_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void display_device_status(void *pvParameters);
void networking_task(void *pvParameters);
void wifi_init_sta(void);
void nvs_get_network_detail(void);
void nvs_set_network_detail(void);
void wifi_init_softap(void);
Networking_struct* get_networking_status();

#ifdef __cplusplus
}
#endif