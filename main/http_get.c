#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "cJSON.h"

#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_tls.h"
#include "esp_log.h"

static const char *TAG = "HTTP_GET";

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "api.thinkpage.cn"
#define WEB_PORT 80
#define WEB_URL "https://api.thinkpage.cn/v3/weather/now.json?key=wcmquevztdy1jpca&location=wuxi&language=en&unit=c"


static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

static void http_get_task(void *pvParameters);


void cjson_to_struct_info(char *text)
{
    //截取有效json
    printf("JSON DATA:\n %s \n",text);
    char *index=strchr(text,'{');
    strcpy(text,index);

    cJSON *pJsonRoot = cJSON_Parse(text);
    //如果是否json格式数据
    if (pJsonRoot ==NULL) {
        cJSON_Delete(pJsonRoot);
        ESP_LOGI(TAG, "Read is not Json data \n");
        return;
    }

    //解析results字段字符串内容
    cJSON *pResultsArrayItem = cJSON_GetObjectItem(pJsonRoot, "results");
    if (pResultsArrayItem) {
        if (cJSON_IsString(pResultsArrayItem))
            ESP_LOGI(TAG, "get results:%s \n", pResultsArrayItem->valuestring);
    } else
        ESP_LOGI(TAG, "get results failed \n");

    //解析results字段字符串内容
    cJSON *pObjectJson = cJSON_GetArrayItem(pResultsArrayItem, 0);
    if (pObjectJson) {
        if (cJSON_IsString(pObjectJson))
            ESP_LOGI(TAG, "get index 0:%s \n", pObjectJson->valuestring);
    } else
        ESP_LOGI(TAG, "get index 0 failed \n");

    //解析now字段字符串内容
    cJSON *pNowData = cJSON_GetObjectItem(pObjectJson, "now");
    if (pNowData) {
        if (cJSON_IsString(pNowData))
            ESP_LOGI(TAG, "get now:%s \n", pNowData->valuestring);
    } else
        ESP_LOGI(TAG, "get now failed \n");

    //解析temperature字段字符串内容
    cJSON *pTemperatureData = cJSON_GetObjectItem(pNowData, "temperature");
    if (pTemperatureData) {
        if (cJSON_IsString(pTemperatureData))
            ESP_LOGI(TAG, "WUXI Temperature:%s \n", pTemperatureData->valuestring);
    } else
        ESP_LOGI(TAG, "get Temperature failed \n");

    cJSON_Delete(pJsonRoot);    
}

static void http_get_task(void *pvParameters)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,       // IPV4
        .ai_socktype = SOCK_STREAM, // TCP协议
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[1024] = "\0";
    char mid_buf[1024] = "\0";
    while(1) {
        int err = getaddrinfo(WEB_SERVER, "80", &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.

        Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        printf("REQUEST: %s\n", REQUEST);
        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* Read HTTP response */
        memset(mid_buf,0,sizeof(mid_buf));
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            //printf("%s\n",recv_buf);
            strcat(mid_buf,recv_buf);
        } while(r > 0);

        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d\r\n", r, errno);
        close(s);

        cjson_to_struct_info(mid_buf);

        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d... ", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
}

void http_get_app_start(void)
{
    xTaskCreate(&http_get_task, "http_get_task", 8192, NULL, 5, NULL);
}