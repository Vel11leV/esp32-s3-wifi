#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "esp_netif.h"

// Конфигурация Wi-Fi и API
#define WIFI_SSID      "TCL-W28TRC"
#define WIFI_PASS      "DPaW7M9HVix"
#define API_KEY        "d266cc33d8f36fe140472c7f38c6339a"
#define CITY           "Budva"
#define WEATHER_URL    "http://http://api.openweathermap.org/data/2.5/find?q=Petersburg&type=like&APPID=d266cc33d8f36fe140472c7f38c6339a"
//#define WEATHER_URL    "http://openweathermap.org" CITY "&appid=" API_KEY "&units=metric"



static const char *TAG = "WEATHER_APP";

// Обработчик событий Wi-Fi
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retry connecting to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Инициализация Wi-Fi
void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id, instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// Парсинг JSON ответа
void parse_weather(char *json_string) {
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    cJSON *main = cJSON_GetObjectItem(root, "main");
    cJSON *temp = cJSON_GetObjectItem(main, "temp");
    cJSON *humidity = cJSON_GetObjectItem(main, "humidity");
    cJSON *name = cJSON_GetObjectItem(root, "name");

    ESP_LOGI(TAG, "City: %s", name->valuestring);
    ESP_LOGI(TAG, "Temperature: %.2f C", temp->valuedouble);
    ESP_LOGI(TAG, "Humidity: %d %%", humidity->valueint);

    cJSON_Delete(root);
}

// HTTP клиент для получения погоды
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static char *output_buffer;
    static int output_len;
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (output_len == 0 && evt->user_data) {
                    memcpy(evt->user_data, evt->data, evt->data_len);
                } else {
                    // В реальном примере тут нужен realloc
                }
                output_len += evt->data_len;
                // В этом простом примере предполагаем, что ответ в одном пакете
                parse_weather((char*)evt->data);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

void weather_task(void *pvParameters) {
    esp_http_client_config_t config = {
        .url = WEATHER_URL,
        .event_handler = _http_event_handler,
        .skip_cert_common_name_check = true,
        .cert_pem = NULL
        
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    while(1) {
        esp_http_client_perform(client);
        vTaskDelay(60000 / portTICK_PERIOD_MS); // Обновление раз в минуту
    }
    esp_http_client_cleanup(client);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
    vTaskDelay(5000 / portTICK_PERIOD_MS); // Ждем подключения
    xTaskCreate(&weather_task, "weather_task", 8192, NULL, 5, NULL);
}