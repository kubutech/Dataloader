#include "ota_update.h"
#include "esp_ota_ops.h"
#include "esp_spiffs.h"

struct {
    esp_partition_t* ota_partition;
    esp_ota_handle_t update_handle;
    enum Uplaod_status status;
    int image_verified;
} OTA_data;

struct {
    esp_partition_t* partition;
    esp_ota_handle_t update_handle;
    enum Uplaod_status status;
    size_t offset;
} SPIFFS_data;

void find_update_partition()
{
    OTA_data.ota_partition = NULL;
    OTA_data.ota_partition = esp_ota_get_next_update_partition(NULL);
}

enum Uplaod_status initialize_ota()
{
    OTA_data.image_verified = 0;
    OTA_data.update_handle = 0;
    if (OTA_data.ota_partition == NULL) {
        OTA_data.status = UPLAOD_FAILED;
    } else {
        OTA_data.status = UPLOAD_INITIALIZED;
    }

    int err = esp_ota_begin(OTA_data.ota_partition, OTA_SIZE_UNKNOWN, &OTA_data.update_handle);
    if (err != ESP_OK) {
        OTA_data.status = UPLAOD_FAILED;
        esp_ota_abort(OTA_data.update_handle);
        ESP_LOGI("OTA","Errror at init");
    }
    return OTA_data.status;
}

enum Uplaod_status write_ota_data(char* buffer, int buffer_size)
{
    if (OTA_data.image_verified == 0) {
        if(buffer[0] == 233) {
            OTA_data.image_verified = 1;
        } else {
            OTA_data.image_verified = 1;
            OTA_data.status = UPLAOD_FAILED;
            esp_ota_abort(OTA_data.update_handle);
        }
    }
    if (OTA_data.status != UPLAOD_FAILED) {
        int err = esp_ota_write(OTA_data.update_handle, (const void *)buffer, buffer_size);
        if (err != ESP_OK) {
            OTA_data.status = UPLAOD_FAILED;
            ESP_LOGE("OTA", "ERROR WRITING");
            esp_ota_abort(OTA_data.update_handle);
        } else {
            OTA_data.status = UPLOAD_IN_PROGRESS;
            ESP_LOGI("OTA", "Wrote: %d bytes", buffer_size);
        }
    }
    return OTA_data.status;
}

enum Uplaod_status end_ota_update()
{
    if (OTA_data.status != UPLAOD_FAILED) {
        int err = esp_ota_end(OTA_data.update_handle);
        if (err != ESP_OK) {
            OTA_data.status = UPLAOD_FAILED;
            ESP_LOGI("OTA", "ERROR ENDING");
        }
    }
    return OTA_data.status;
}

enum Uplaod_status initialize_spiffs_update()
{
    SPIFFS_data.partition = NULL;
    SPIFFS_data.partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "storage");
    if (SPIFFS_data.partition != NULL) {
        ESP_LOGI("OTA", "FOUND SPIFFS PARTITION!!!!!");
        SPIFFS_data.status = UPLOAD_INITIALIZED;
        SPIFFS_data.offset = 0;
    } else {
        ESP_LOGE("OTA", "COULDN'T FIND SPIFFS PARTITION!!!!!");
        SPIFFS_data.status = UPLAOD_FAILED;
    }
    int err = esp_partition_erase_range(SPIFFS_data.partition, 0, 524288);
    if (err != ESP_OK) {
        SPIFFS_data.status = UPLAOD_FAILED;
        ESP_LOGE("OTA", "ERASE FAILED!!!!!!");
        printf("\n\n%x\n\n", err);
    } else {
        SPIFFS_data.status = UPLOAD_INITIALIZED;
        ESP_LOGI("OTA", "ERASE SUCCESSFULL!!!!!");
    }
    return SPIFFS_data.status;
}

enum Uplaod_status write_to_partition(char* buffer, int buffer_size)
{
    int err = esp_partition_write_raw(SPIFFS_data.partition, SPIFFS_data.offset, buffer, buffer_size);
    if (err != ESP_OK) {
        SPIFFS_data.status = UPLAOD_FAILED;
        ESP_LOGE("OTA", "WRITE FAILED!!!!!!");
        printf("\n\n%x\n\n", err);
    } else {
        SPIFFS_data.status = UPLOAD_IN_PROGRESS;
        ESP_LOGI("OTA", "Wrote: %d bytes", buffer_size);
    }
    SPIFFS_data.offset += buffer_size;
    return SPIFFS_data.status;
}

enum Uplaod_status switch_to_ota_partition()
{
    enum Uplaod_status status = UPLAOD_FAILED;
    esp_app_desc_t ota_desc;
    int err = esp_ota_get_partition_description(OTA_data.ota_partition, &ota_desc);
    if (err != ESP_OK) {
        status = UPLAOD_FAILED;
    } else {
        err = esp_ota_set_boot_partition(OTA_data.ota_partition);
        if (err != ESP_OK) {
            status = UPLAOD_FAILED; 
        } else {
            status = UPLOAD_SUCCESSFUL;
        }
    }
    return status;
}

char* check_spiffs_partition()
{
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret == ESP_OK) {
        esp_vfs_spiffs_unregister("storage");
        return "Installed";
    } else {
        return "Not found";
    }
}

void get_status_json(char* json)
{
    char proj_name[32];
    char version[32];
    bzero(proj_name,32);
    bzero(version, 32);
    if (OTA_data.ota_partition != NULL) {
        esp_app_desc_t ota_desc;
        int err = esp_ota_get_partition_description(OTA_data.ota_partition, &ota_desc);
        if (err != ESP_OK) {
            strcpy(proj_name, "No software installed");
            strcpy(version, "N/A");
        } else {
            strcpy(proj_name, ota_desc.project_name);
            strcpy(version, ota_desc.version);
        }
    } else {
        strcpy(proj_name, "No partition found for update");
        strcpy(version, "N/A");
    }
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    sprintf(json, "{\n\
        \"mode\":\"software_load\",\n\
        \"software\":{\n\
            \"name\":\"%s\",\n\
            \"revision\":\"%s\"\n\
        },\n\
        \"chip\":{\n\
            \"model\":\"%s\",\n\
            \"revision\":\"%d\",\n\
            \"cores\":\"%d\"\n\
        },\n\
        \"spiffs\":\"%s\"\n\
    }", proj_name, version,
        get_chip(chip_info.model), chip_info.revision, chip_info.cores,
        check_spiffs_partition());
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
