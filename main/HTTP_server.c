#include <string.h>
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include <lwip/netdb.h>
#include "HTTP_server.h"
#include "ota_update.h"
#include "networking.h"

void http_listener_task(void* pvParameters)
{
    struct sockaddr_in client_addr, http_addr;
    int socklen = sizeof(client_addr);
    int connection_socket;

    http_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    http_addr.sin_family = AF_INET;
    http_addr.sin_port = htons(HTTP_PORT);

    while (1)
    {
        Networking_struct* network_status = get_networking_status();
        if (network_status->networking_status != NETWORKING_NONE) {
            int http_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);  
            if (http_socket == -1) {
                ESP_LOGE(SERVER_TAG, "Unable to create socket: errno %d ... Retrying in 10 seconds...", errno);
                vTaskDelay(10000 / portTICK_PERIOD_MS);
                continue;
            }

            int opt = 1;
            setsockopt(http_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

            if (bind(http_socket, (struct sockaddr *)&http_addr, sizeof(http_addr)) != 0) {
                ESP_LOGE(SERVER_TAG, "Socket unable to bind: errno %d ... Retrying in 10 seconds...", errno);
                shutdown(http_socket, SHUT_RDWR);
                vTaskDelay(10000 / portTICK_PERIOD_MS);
                continue;
            }

            while (1)
            {
                if ((listen(http_socket, 5)) != 0) {
                    ESP_LOGE(SERVER_TAG, "Listen failed: errno %d ... Retrying in 1 second...", errno);
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    break;
                }

                ESP_LOGI(SERVER_TAG, "Listening for incoming HTTP requests!");
                
                connection_socket = accept(http_socket, (struct sockaddr *)&client_addr, &socklen);
                if (connection_socket < 0) {
                    ESP_LOGE(SERVER_TAG, "Connection failed: errno %d ... Retrying in 1 second...", errno);
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    break;;
                }
                
                struct timeval timeout;
                timeout.tv_sec = 1;
                timeout.tv_usec = 0;
                setsockopt (connection_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

                ESP_LOGI(SERVER_TAG, "Received connection from: %s, port: %d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                handle_http_request(connection_socket);
            }
            close(http_socket);
        }
        ESP_LOGE(SERVER_TAG, "Cannot start up HTTP server- connection not yet established");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void handle_http_request(int connection_socket)
{
    char buffer[MAX_RESPONSE_LENGHT];
    bzero(buffer, MAX_RESPONSE_LENGHT);
    int result = recv(connection_socket, buffer, sizeof(buffer), 0);
    if (result < 0) {
        ESP_LOGI(SERVER_TAG, "No request received, dropping connection!");
    }
    else {
        char* method = malloc(6);
        bzero(method, 6);
        memccpy(method, buffer, '\x20', 6);
        
        char* filename = malloc(MAX_PATHLENGTH);
        bzero(filename, MAX_PATHLENGTH);
        memccpy(filename + strlen(filename), buffer + strlen(method), '\x20', MAX_PATHLENGTH);
        filename[strlen(filename) - 1] = 0;
        
        char* entity;
        long entity_size = 0;
        char* response;

        if (strcmp(method, "GET ") == 0) {

            if (strcmp(filename, "/") == 0) {
                extern const uint8_t page_start[] asm("_binary_Dataloader_html_start");
                extern const uint8_t page_end[]   asm("_binary_Dataloader_html_end");
                entity = page_start;
                entity_size = page_end - page_start;
                response = GET_response_header(entity_size, "text/html");
                
            } else if (strcmp(filename, "/esp_status") == 0) {
                entity = malloc(MAX_RESPONSE_LENGHT);
                bzero(entity, MAX_RESPONSE_LENGHT);
                get_status_json(entity);
                entity_size = strlen(entity);
                response = GET_response_header(entity_size, "application/json");
                
            } else if (strcmp(filename, "/switch_to_ota") == 0) {
                enum Uplaod_status status = switch_to_ota_partition();
                if (status == UPLOAD_SUCCESSFUL) {
                    entity = "Operation accepted";
                    entity_size = strlen(entity);
                    response = GET_response_header(entity_size, "text/plain");
                    strcat(response, entity);
                    send(connection_socket, response, strlen(response), 0);
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    esp_restart();
                    return;
                } else {
                    entity = "Operation rejected";
                }
                entity_size = strlen(entity);
                response = GET_response_header(entity_size, "text/plain");
            } else {
                response = "HTTP/1.1 404 Not Found\r\n";
            }

        } else if (strcmp(method, "POST ") == 0) {
            if (strcmp(filename, "/credentials") == 0) {

                response = "HTTP/1.1 200 OK\r\n";
            } else if (strcmp(filename, "/main_app.bin") == 0) {
                ESP_LOGI(SERVER_TAG, "Received upload request!!!!!");
                enum Uplaod_status status = initialize_ota();
                
                if (status == UPLAOD_FAILED) {
                    response = "HTTP/1.1 500 Internal Server Error\r\n\t\nError initalizing update!";
                } else {
                    char buffer[MAX_RESPONSE_LENGHT];
                    int recv_size = 1;

                    while (recv_size > 0 && status != UPLAOD_FAILED) {
                        bzero(buffer, MAX_RESPONSE_LENGHT);
                        recv_size = recv(connection_socket, buffer, MAX_RESPONSE_LENGHT, 0);
                        if (recv_size > 0) {
                            status = write_ota_data(buffer, recv_size);
                        }
                    }

                    status = end_ota_update();

                    if (status == UPLAOD_FAILED) {
                        response = "HTTP/1.1 500 Internal Server Error\r\n\t\nCorrupted image";
                    } else {
                        response = "HTTP/1.1 200 OK\r\n";
                    }
                }
            } else if (strcmp(filename, "/spiffs.bin") == 0) {
                ESP_LOGI(SERVER_TAG, "Received upload request!!!!!");
                enum Uplaod_status status = initialize_spiffs_update();
                if (status != UPLOAD_INITIALIZED) {
                    response = "HTTP/1.1 500 Internal Server Error\r\n";
                } else {
                    char buffer[MAX_RESPONSE_LENGHT];
                    int recv_size = 1;

                    while (recv_size > 0 && status != UPLAOD_FAILED) {
                        bzero(buffer, MAX_RESPONSE_LENGHT);
                        recv_size = recv(connection_socket, buffer, MAX_RESPONSE_LENGHT, 0);
                        if (recv_size > 0) {
                            status = write_to_partition(buffer, recv_size);
                        }
                    }
                    if (status != UPLAOD_FAILED) {
                        response = "HTTP/1.1 200 OK\r\n";
                        ESP_LOGI("OTA", "UPLOAD_SUCCESSFULL!!!!!!!");
                    } else {
                        response = "HTTP/1.1 500 Internal Server Error\r\n";
                        ESP_LOGE("OTA", "UPLOAD_FAILED!!!!!!!!!");
                    }
                }

        } else {
                response = "HTTP/1.1 404 Not Found\r\n";
            }
        } else {
            response = "HTTP/1.1 400 Bad Request\r\n";
        }

        send(connection_socket, response, strlen(response), 0);
        
        if (entity_size > 0) {
            int chunks = (entity_size - 1) / MAX_RESPONSE_LENGHT + 1;
            for (int i = 0; i < chunks; i++) {
                int size;
                if (i * MAX_RESPONSE_LENGHT < entity_size) {
                    size = entity_size - i * MAX_RESPONSE_LENGHT;
                } else {
                    size = MAX_RESPONSE_LENGHT;
                }
                send(connection_socket, entity + i * MAX_RESPONSE_LENGHT, size, 0);
            }
        }

    }
    shutdown(connection_socket, SHUT_RDWR);
    close(connection_socket);
}

char* GET_response_header(long file_size, char* content_type)
{
    char* reply = malloc(1024);
    bzero(reply, 1024);
    sprintf(reply, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", content_type, file_size);
    
    return reply;
}

int calculate_entity_offset(char* buffer)
{
    for (int i = 0; i < MAX_RESPONSE_LENGHT - 4; i++) {
        if (buffer[i] == '\r' && buffer[i+1] == '\n' && buffer[i+2] == '\r' && buffer[i+3] == '\n') {
            return i+4;
        }
    }
    return 0;
}
