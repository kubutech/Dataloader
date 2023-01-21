#include <stdio.h>

typedef struct {
    size_t total;
    size_t used;
} Storage;

void mount_drive();
FILE* open_file(long* entity_size, char* filename, char* mode);
void spiffs_info(Storage* storage);