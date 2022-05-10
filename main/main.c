/*
 * @Author: xtx
 * @Date: 2022-05-09 23:14:36
 * @LastEditors: xtx
 * @LastEditTime: 2022-05-10 00:34:35
 * @FilePath: /template-app/main/main.c
 * @Description: ����д���
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_http_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"

#include <string.h>
#include "driver/ledc.h"
#include "cJSON.h"

#define EX_UART_NUM UART_NUM_1
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
static QueueHandle_t uart0_queue;
typedef uint8_t u8;
#define PATTERN_CHR_NUM (3) /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define MAX_HTTP_OUTPUT_BUFFER 1024

static const char *TAG = "xtxTest";

void Uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, 5, 6, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set uart pattern detect function.
    uart_enable_pattern_det_baud_intr(EX_UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    uart_pattern_queue_reset(EX_UART_NUM, 20);
    ESP_LOGI(TAG, "Uart1 Prepare in GPIO5_TX GPIO6_RX");
}

void Analyse_Weather(char *output_buffer)
{
    cJSON *root = NULL;
    root = cJSON_Parse(output_buffer);

    cJSON *cjson_item = cJSON_GetObjectItem(root, "results");
    cJSON *cjson_results = cJSON_GetArrayItem(cjson_item, 0);
    cJSON *cjson_now = cJSON_GetObjectItem(cjson_results, "now");
    cJSON *cjson_temperature = cJSON_GetObjectItem(cjson_now, "temperature");

    printf("%s\n", cjson_temperature->valuestring);
}
static void http_post_task(void *pvParameters)
{
    esp_http_client_config_t config;
    memset(&config, 0, sizeof(config));
    //�����ýṹ���ڲ�д��url
    static const char *URL = "https://api.seniverse.com/v3/weather/now.json?key=Suo-L-0OoOH2-YLPj&location=beijing&language=zh-Hans&unit=c";
    config.url = URL;

    //��ʼ���ṹ��
    esp_http_client_handle_t client = esp_http_client_init(&config); //��ʼ��http����

    //���÷�������
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
    int content_length = 0;
    while (1)
    {

        // ��Ŀ�������������ӣ���������д�����ݳ���Ϊ0
        //��Ϊ�����post���󣬻��ڱ��ĵ�ͷ���������Ҫ����������͵�����
        //������get���������͵����ݶ���URL���棬���ڱ���ͷ��������Ҫ�������Ĳ��֣����д�볤�Ⱦ���0
        esp_err_t err = esp_http_client_open(client, 0);

        //�������ʧ��
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        }
        //������ӳɹ�
        else
        {

            //��ȡĿ�������ķ������ݵ�Э��ͷ
            content_length = esp_http_client_fetch_headers(client);

            //���Э��ͷ����С��0��˵��û�гɹ���ȡ��
            if (content_length < 0)
            {
                ESP_LOGE(TAG, "HTTP client fetch headers failed");
            }

            //����ɹ���ȡ����Э��ͷ
            else
            {

                //��ȡĿ������ͨ��http����Ӧ����
                int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
                if (data_read >= 0)
                {

                    //��ӡ��Ӧ���ݣ�������Ӧ״̬����Ӧ�峤�ȼ�������
                    ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                             esp_http_client_get_status_code(client),     //��ȡ��Ӧ״̬��Ϣ
                             esp_http_client_get_content_length(client)); //��ȡ��Ӧ��Ϣ����
                    printf("data:%s\n", output_buffer);
                    Analyse_Weather(output_buffer);
                }
                //������ɹ�
                else
                {
                    ESP_LOGE(TAG, "Failed to read response");
                }
            }
        }

        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    Uart_init();

    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(&http_post_task, "http_post_task", 4096, NULL, 5, NULL);
}