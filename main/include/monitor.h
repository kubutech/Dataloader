#include "Global_types.h"


void set_up_monitor();
void get_time(Uptime* time);
void get_status_json(char* json);
char* get_chip(esp_chip_model_t model);
char* get_reset_reason(esp_reset_reason_t reason);