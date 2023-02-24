#include <stdio.h>

#include <unistd.h>
#include <string.h>

#include <esp_system.h>
#include <esp_tls.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <lwip/dns.h>

#include <freertos/event_groups.h>

//================ WIFI ============= 
#define ESP_WIFI_SSID      "SSID"
#define ESP_WIFI_PASS      "p4zzw0rd"
#define ESP_MAXIMUM_RETRY  5
#define WIFI_CONNECTED_BIT BIT0


static bool s_is_wifi_connected = false;
static EventGroupHandle_t s_event_group_handler;
static int s_retry_count = 0;
//===================================

ip4_addr_t server_ip; //DNS server IP



//================ TLS =============
extern const uint8_t ca_crt_start[] asm("_binary_ca_crt_start");
extern const uint8_t ca_crt_end[] asm("_binary_ca_crt_end");

char * hostname = "sslserver.home";
//==================================

//event handler of the WiFi
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_count < ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_count++;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(s_event_group_handler, WIFI_CONNECTED_BIT);
        s_retry_count = 0;
    }
}

void wifi_init_sta(void)
{
    s_event_group_handler = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));

    esp_event_handler_instance_t handler_any_id;
    esp_event_handler_instance_t handler_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &handler_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &handler_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_event_group_handler,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
        s_is_wifi_connected = true;

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, handler_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, handler_any_id));
    vEventGroupDelete(s_event_group_handler);
}

void app_main(void)
{
    {// set wifi connection
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    printf("\n[WIFI] Connecting to WiFi...\n");
    wifi_init_sta();
    while (!s_is_wifi_connected)
    {
        printf("[WIFI] Waiting for the WiFi to connect\n");
        sleep(1);
    }

    printf("[WIFI] Connected to WiFi\n");
}

    IP4_ADDR(&server_ip, 192, 168, 1, 199);
    dns_setserver(0, &server_ip);

    
    esp_tls_t *tls = esp_tls_init();
    if(!tls){
        printf("[TLS] not initialised successfully\n");
        return;
    }

    ESP_ERROR_CHECK(esp_tls_set_global_ca_store(ca_crt_start, ca_crt_end - ca_crt_start));

    esp_tls_cfg_t cfg = {
        .use_global_ca_store = true
    };

    
    esp_tls_conn_new_sync(hostname, strlen(hostname) + 1, 6000, &cfg, tls);
    esp_tls_conn_write(tls, "some encrypted data", strlen("some encrypted data"));

    esp_tls_conn_destroy(tls);

    printf("bye ;)\n");
}
