#include "esp_system.h"
#include "Global_types.h"

void http_listener_task(void* pvParameters);
void handle_http_request(int connection_socket);
char* GET_response_header(long file_size, char* content_type);
int calculate_entity_offset(char* buffer);
char* get_file_type(char* filename);