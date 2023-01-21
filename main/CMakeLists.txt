idf_component_register(SRCS  "application.c" "networking.c" "HTTP_server.c" "storage_interface.c" "monitor.c"
                    INCLUDE_DIRS "include")

spiffs_create_partition_image(storage ../spiff FLASH_IN_PROJECT)