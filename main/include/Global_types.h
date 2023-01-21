#pragma once

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_app_desc.h"

#define SUCCESS 1
#define FAILURE 0

#define NETWORKING_STA     0
#define NETWORKING_AP      1
#define NETWORKING_NONE    2
#define CONNECTION_FAILED     0
#define CONNECTION_SUCCESSFUL 1

#define HTTP_PORT                  80
#define REST_PORT                  23487
#define MAX_FILENAME               32
#define MAX_PATHLENGTH             128
#define MAX_RESPONSE_LENGHT        1024
#define CHUNK_SIZE                 1024
#define SERVER_TAG                 "HTTP"

#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define MAX_NUMBER_OF_AP_RECORDS   10
#define PASSWORD_LENGTH            64
#define SSID_LENGTH                32

#define ESP_NAME                   "Ganymede"
#define AP_ESP_PASS                "esp_32_Ganymede"
#define AP_ESP_CHANNEL             6
#define AP_MAX_STA_CONN            4
#define RETRY_PERIOD               300 //time for the device to retry connection to memorized AP when in SoftAP mode

#define GPIO_HIGH                  1
#define GPIO_LOW                   0

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

typedef struct {
    char status[256];
    char my_ip[24];
    int networking_status;
} Networking_struct;

typedef struct {
    int seconds;
    int minutes;
    int hours;
    int days;
} Uptime;