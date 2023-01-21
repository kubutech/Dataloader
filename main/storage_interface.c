#include "Global_types.h"
#include "storage_interface.h"
#include "esp_spiffs.h"

int operational = FAILURE;

esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = false
};

void mount_drive()
{
    ESP_LOGI("SPIFFS", "Initializing SPIFFS");

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("SPIFFS", "Failed to mount or format filesystem");
            operational = FAILURE;
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("SPIFFS", "Failed to find SPIFFS partition");
            operational = FAILURE;
        } else {
            ESP_LOGE("SPIFFS", "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
            operational = FAILURE;
        }
    } else {
        ESP_LOGI("SPIFFS", "Initialized SPIFFS successfully");
        operational = SUCCESS;
    }
}

FILE* open_file(long* entity_size, char* filename, char* mode)
{
    FILE* f;

    if (operational == SUCCESS) {
        char * filepath[MAX_PATHLENGTH] = {0};
        strcpy(filepath, "/spiffs");
        strcat(filepath, filename);

        f = fopen(filepath, mode);

        if (f == NULL) {
            *entity_size = -1;
        } else {
            fseek(f, 0L, SEEK_END);
            *entity_size = ftell(f);
            fseek(f, 0L, SEEK_SET);
        }

        return f;
    } else {
        *entity_size = -1;
        return NULL;
    }
}

void spiffs_info(Storage* storage)
{
    if (operational == SUCCESS) {
        esp_spiffs_info(NULL, &storage->total, &storage->used);
    } else {
        storage->total = 0;
        storage->used = 0;
    }
}