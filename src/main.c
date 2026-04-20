#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

#define LED_PIN GPIO_NUM_2 

//http://192.168.4.1

static const char *TAG = "WEB_SERVER";


static const char* html_page = 
    "<html><body><h1>ESP32-S3 Control</h1>"
    "<button onclick=\"location.href='/on'\">ON</button>"
    "<button onclick=\"location.href='/off'\">OFF</button>"
    "</body></html>";


esp_err_t on_handler(httpd_req_t *req) {
    gpio_set_level(LED_PIN, 1);
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


esp_err_t off_handler(httpd_req_t *req) {
    gpio_set_level(LED_PIN, 0);
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = root_handler };
        httpd_register_uri_handler(server, &uri_root);
        
        httpd_uri_t uri_on = { .uri = "/on", .method = HTTP_GET, .handler = on_handler };
        httpd_register_uri_handler(server, &uri_on);

        httpd_uri_t uri_off = { .uri = "/off", .method = HTTP_GET, .handler = off_handler };
        httpd_register_uri_handler(server, &uri_off);
    }
}

void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32S3_AP",
            .ssid_len = strlen("ESP32S3_AP"),
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void) {

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    wifi_init_softap();
    start_webserver();
}