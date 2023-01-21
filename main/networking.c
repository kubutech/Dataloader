#include "Global_types.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"

#include "networking.h"


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */

static const char *TAG = "Core";

static int s_retry_num = 0;

static int disconnect_block = 0;

Networking_struct networking = {
        .networking_status = NETWORKING_NONE
};

char ssid[SSID_LENGTH];
char pass[PASSWORD_LENGTH];

esp_netif_t *p_netif_ap;
esp_netif_t *p_netif_sta;

wifi_scan_config_t scan_config = {
    .ssid = NULL,
    .bssid = NULL,
    .channel = 0,
    .show_hidden = 0,
    .scan_time = {
        .active = {
            .min = 300,
            .max = 900,
        },
    },
};

void display_device_status(void *pvParameters)
{
    while (1) {
        if (networking.status[0] != 0) {
            ESP_LOGI(ESP_NAME, "%s", networking.status);
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_retry_num = 0;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED && disconnect_block == 0) {
        if (networking.networking_status == NETWORKING_STA) {
            networking.networking_status = NETWORKING_NONE;
            ESP_LOGE(ESP_NAME, "Disconnected from %s, starting up Access Point\n",ssid);
            wifi_init_softap();
        }
        else if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(ESP_NAME, "Couldn't connect to the specicified network... Retrying... Attepts remaining: %d", EXAMPLE_ESP_MAXIMUM_RETRY - s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        sprintf(networking.my_ip, IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(ESP_NAME, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(ESP_NAME, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void networking_task(void *pvParameters)
{
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    p_netif_sta = esp_netif_create_default_wifi_sta();
    p_netif_ap = esp_netif_create_default_wifi_ap();
    
    esp_netif_set_hostname(p_netif_sta, ESP_NAME);
    esp_netif_set_hostname(p_netif_ap, ESP_NAME);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    while (1) {
        if (networking.networking_status != NETWORKING_STA) {
            wifi_init_sta();
        }
        //if connection unsuccessful attempt reconnection in 10 minutes since router might have been switched back online
        vTaskDelay(RETRY_PERIOD * 1000 / portTICK_PERIOD_MS);
    }
}

void nvs_get_network_detail(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK( err );
    nvs_handle_t my_handle;
    err = nvs_open("network_detail", NVS_READWRITE, &my_handle);

    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        size_t ssid_length = SSID_LENGTH;
        switch(nvs_get_str(my_handle, "ssid", ssid, &ssid_length)) {
            case ESP_OK:
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                strcpy(ssid, "default");
                break;
            default :
                printf("Error: Undefined failure\n");
        }

        size_t pass_length = PASSWORD_LENGTH;
        switch (nvs_get_str(my_handle, "password", pass, &pass_length)) {
            case ESP_OK:
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                strcpy(pass, "default");
                break;
            default:
                printf("Error: Undefined failure\n");
        }
    }

    nvs_close(my_handle);

}

void nvs_set_network_detail(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK( err );
    nvs_handle_t my_handle;
    err = nvs_open("network_detail", NVS_READWRITE, &my_handle);

    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        nvs_set_str(my_handle, "ssid", ssid);
        nvs_set_str(my_handle, "password", pass);

        nvs_commit(my_handle);
    }

    nvs_close(my_handle);
    
}

void wifi_init_sta(void)
{
    disconnect_block = 1;
    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_stop());
    esp_wifi_set_mode(WIFI_MODE_NULL);
    networking.networking_status = NETWORKING_NONE;
    disconnect_block = 0;

    s_wifi_event_group = xEventGroupCreate();

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    if (strlen(ssid) == 0 || strlen(pass) == 0) {
        nvs_get_network_detail();
    }

    memcpy(wifi_config.sta.ssid, ssid, SSID_LENGTH);
    memcpy(wifi_config.sta.password, pass, PASSWORD_LENGTH);

    sprintf(networking.status, "Attempting to connect to %s", wifi_config.sta.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        sprintf(networking.status, "Connected to %s! IP: %s", ssid, networking.my_ip);
        networking.networking_status = NETWORKING_STA;
        nvs_set_network_detail();
    } else if (bits & WIFI_FAIL_BIT) {
        sprintf(networking.status, "Couldn't connect to %s! Starting Access Point", ssid);
        networking.networking_status = NETWORKING_AP;
        wifi_init_softap();
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void wifi_init_softap(void)
{
    disconnect_block = 1;
    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_stop());
    esp_wifi_set_mode(WIFI_MODE_NULL);
    networking.networking_status = NETWORKING_NONE;
    disconnect_block = 0;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_NAME,
            .ssid_len = strlen(ESP_NAME),
            .channel = AP_ESP_CHANNEL,
            .password = AP_ESP_PASS,
            .max_connection = AP_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                    .required = false,
            },
        },
    };

    if (strlen(ESP_NAME) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t if_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(p_netif_ap, &if_info));
    sprintf(networking.my_ip, IPSTR, IP2STR(&if_info.ip));
    sprintf(networking.status, "Access point started! IP: %s", networking.my_ip);   
    networking.networking_status = NETWORKING_AP;
}

void init_connection(char* data)
{
    char temp_ssid[SSID_LENGTH];
    char temp_pass[PASSWORD_LENGTH];
    
    strcpy(temp_ssid, ssid);
    strcpy(temp_pass, pass);

    strcpy(ssid, (char*)data);

    if(strlen(ssid) <= 0) {
        ESP_LOGE(SERVER_TAG, "Error parsing SSID from the message\n");
        strcpy(ssid, temp_ssid);
        strcpy(pass, temp_pass);
        return;
    }

    strcpy(pass, (char*)data + strlen(ssid) + 1);

    if(strlen(pass) <= 0) {
        ESP_LOGE(SERVER_TAG, "Error parsing Password from the message\n");
        strcpy(ssid, temp_ssid);
        strcpy(pass, temp_pass);
        return;
    }

    wifi_init_sta();

    if (networking.networking_status != NETWORKING_STA) {
        ESP_LOGE(SERVER_TAG, "Couldn't connect to AP with specified credentials\n");
        strcpy(ssid, temp_ssid);
        strcpy(pass, temp_pass);
        wifi_init_sta();
    }
}

Networking_struct* get_networking_status()
{
    return &networking;
}

void get_network_list(char* entity)
{
    esp_wifi_scan_start(&scan_config, 1);
    wifi_ap_record_t records[MAX_NUMBER_OF_AP_RECORDS];
    uint16_t number_of_records = MAX_NUMBER_OF_AP_RECORDS;
    esp_wifi_scan_get_ap_records(&number_of_records, &records);
    strcat(entity, "{\n\t\"ssids\":[\n");
    for (int i = 0; i < number_of_records; i++) {
        strcat(entity, "\t\t\"");
        strcat(entity, (char*)records[i].ssid);
        strcat(entity, "\",\n");
    }
    entity[strlen(entity) - 2] = ' ';
    strcat(entity, "\t]\n}");
}