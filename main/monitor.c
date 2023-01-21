#include "networking.h"
#include "storage_interface.h"
#include "esp_timer.h"

#include "monitor.h"

int initialized = FAILURE;

struct {
    Networking_struct* network;
    const char* name;
    esp_app_desc_t* app_description;
    esp_reset_reason_t reset_reason;
    uint8_t mac_address[8];
    Uptime timer;
    Storage storage;
    esp_chip_info_t chip_info;
} monitor;


void set_up_monitor()
{
    monitor.network = get_networking_status();
    monitor.name = ESP_NAME;
    monitor.app_description = esp_app_get_description();
    monitor.reset_reason = esp_reset_reason();
    esp_chip_info(&monitor.chip_info);
    initialized = SUCCESS;
}


void get_time(Uptime* time)
{
    int64_t uptime_sec = esp_timer_get_time() / 1000000;

    time->days = uptime_sec / (3600 * 24);

    time->hours = (uptime_sec % (3600 * 24)) / 3600;

    time->minutes = ((uptime_sec % (3600 * 24)) % 3600) / 60;

    time->seconds = ((uptime_sec % (3600 * 24)) % 3600) % 60;
}

void get_status_json(char* json)
{
    if (initialized == SUCCESS) {
        get_time(&monitor.timer);

        spiffs_info(&monitor.storage);

        esp_base_mac_addr_get(monitor.mac_address);

        sprintf(json, "{\n\
        \"uptime\": {\n\
            \"days\":\"%d\",\n\
            \"hours\":\"%d\",\n\
            \"minutes\":\"%d\",\n\
            \"seconds\":\"%d\"\n\
        },\n\
        \"storage\":{\n\
            \"total\":\"%d\",\n\
            \"used\":\"%d\"\n\
        },\n\
        \"esp_name\":\"%s\",\n\
        \"network_status\":\"%s\",\n\
        \"software\":{\n\
            \"name\":\"%s\",\n\
            \"revision\":\"%s\"\n\
        },\n\
        \"chip\":{\n\
            \"model\":\"%s\",\n\
            \"revision\":\"%d\",\n\
            \"cores\":\"%d\"\n\
        },\n\
        \"reset\":\"%s\",\n\
        \"mac_address\":\"%x-%x-%x-%x-%x-%x\"\n\
    }", monitor.timer.days, monitor.timer.hours, monitor.timer.minutes, monitor.timer.seconds,
    monitor.storage.total, monitor.storage.used,
        ESP_NAME, 
        monitor.network->status,
        monitor.app_description->project_name, monitor.app_description->version,
        get_chip(monitor.chip_info.model), monitor.chip_info.revision, monitor.chip_info.cores,
        get_reset_reason(monitor.reset_reason),
        monitor.mac_address[0], monitor.mac_address[1], monitor.mac_address[2], monitor.mac_address[3], monitor.mac_address[4], monitor.mac_address[5]);
    } else {
        sprintf(json, "{\n\t\"Status\":\"Not initalized\"\n}");
    }
}

char* get_chip(esp_chip_model_t model)
{
    if (model == CHIP_ESP32) {
        return "ESP32";
    } else if (model == CHIP_ESP32S2) {
        return "ESP32-S2";
    } else if (model == CHIP_ESP32S3) {
        return "ESP32-S3";
    } else if (model == CHIP_ESP32C2) {
        return "ESP32-C2";
    } else if (model == CHIP_ESP32H2) {
        return "ESP32-H2";
    }

    return "unknown";
}

char* get_reset_reason(esp_reset_reason_t reason)
{
    if (reason == ESP_RST_POWERON) {
        return "Power";
    } else if (reason == ESP_RST_EXT) {
        return "External";
    } else if (reason == ESP_RST_SW) {
        return "Software";
    } else if (reason == ESP_RST_PANIC) {
        return "Exception";
    } else if (reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT) {
        return "Watchdog";
    } else if (reason == ESP_RST_DEEPSLEEP) {
        return "Deep-sleep";
    }

    return "Other";
}