#include "Global_types.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"

typedef enum Uplaod_status {UPLOAD_INITIALIZED, UPLOAD_IN_PROGRESS, UPLAOD_FAILED, UPLOAD_SUCCESSFUL};

char* get_chip(esp_chip_model_t model);
void get_status_json(char* json);
enum Uplaod_status write_ota_data(char* buffer, int buffer_size);
enum Uplaod_status initialize_ota();
enum Uplaod_status initialize_spiffs_update();
void end_ota_update();
void find_update_partition();
enum Uplaod_status write_to_partition(char* buffer, int buffer_size);
enum Uplaod_status switch_to_ota_partition();