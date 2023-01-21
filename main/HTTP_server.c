#include <string.h>
#include <stdio.h>
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include <lwip/netdb.h>
#include "HTTP_server.h"

#include "networking.h"
#include "storage_interface.h"
#include "monitor.h"

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
        vTaskDelay(5000 / portTICK_PERIOD_MS);
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

        char* response;
        FILE *file = NULL;
        long entity_size = 0;

        if (strcmp(method, "GET ") == 0) {

            if (strcmp(filename, "/") == 0) {
                strcat(filename, "MainPage.html");
            }

            file = open_file(&entity_size, filename, "r");

            if (entity_size == -1) {
                if (strcmp(filename, "/esp_status") == 0) {
                    get_status_json(buffer);
                    response = GET_response_header(strlen(buffer), "application/json");
                } else if (strcmp(filename, "/network_list") == 0) {
                    bzero(buffer, MAX_RESPONSE_LENGHT);
                    get_network_list(buffer);
                    response = GET_response_header(strlen(buffer), "application/json");
                } else {
                    ESP_LOGE("files", "File '%s' not found on the server", filename);
                    response = "HTTP/1.1 404 Not Found\r\n";
                }
            } else {
                response = GET_response_header(entity_size, get_file_type(filename));
            }

        } else if (strcmp(method, "POST ") == 0) {
            if (strcmp(filename, "/credentials") == 0) {
                int offset = calculate_entity_offset(buffer);
                char* credentials = malloc(PASSWORD_LENGTH + SSID_LENGTH + 3);
                bzero(credentials, PASSWORD_LENGTH + SSID_LENGTH + 3);
                memccpy(credentials, buffer + offset, '\n', PASSWORD_LENGTH + SSID_LENGTH + 2);
                init_connection(credentials);
            }
            response = "HTTP/1.1 200 OK\r\n";
        } else {
            response = "HTTP/1.1 400 Bad Request\r\n";
        }

        send(connection_socket, response, strlen(response), 0);
        if (file != NULL) {
            fseek(file, 0L, SEEK_SET);
            do {
                char send_buffer[MAX_RESPONSE_LENGHT] = {0};
                int size = fread(send_buffer, 1, MAX_RESPONSE_LENGHT, file);
                send(connection_socket, send_buffer, size, 0);
            } while (feof(file) == 0);
            fclose(file);
        } else if (strlen(buffer) > 0) {
            send(connection_socket, buffer, strlen(buffer), 0);
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

char* get_file_type(char* filename)
{
    char* extension = strchr(filename, '.');

    if (strcmp(extension, ".html") == 0) {
        return "text/html";
    } else if (strcmp(extension, ".txt") == 0) {
        return "text/plain";
    } else if (strcmp(extension, ".png") == 0 || strcmp(extension, ".ico") == 0) {
        return "image/png";
    } else {
        return "application/octet-stream";
    }
}
