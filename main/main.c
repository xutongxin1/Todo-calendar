/*
 * @Author: xtx
 * @Date: 2022-05-09 23:14:36
 * @LastEditors: xtx
 * @LastEditTime: 2022-05-11 03:57:51
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
#include "esp_sntp.h"

#include <string.h>
#include "driver/ledc.h"
#include "cJSON.h"

#define EX_UART_NUM UART_NUM_1
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
//static QueueHandle_t uart0_queue;
typedef uint8_t u8;
#define PATTERN_CHR_NUM (3) /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define MAX_HTTP_OUTPUT_BUFFER 4096

static const char *TAG = "xtxTest";

static QueueHandle_t GetAeecssCode_queue;

static char AccessCode[1024] = "";
static char Today[100] = "";

static char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};

void Uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .source_clk = UART_SCLK_APB,
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, 6, 7, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
    ESP_LOGI(TAG, "Uart1 Prepare in GPIO6_TX GPIO7_RX");

    char test[100] = "t6.txt=\"����������������QAQ\"";
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    uart_write_bytes(UART_NUM_1, test, strlen(test));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    uart_write_bytes(UART_NUM_1, test, strlen(test));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
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
static void GetAeecssCode(void *pvParameters)
{
    esp_http_client_config_t config;
    memset(&config, 0, sizeof(config));
    //�����ýṹ���ڲ�д��url
    config.buffer_size_tx = 4096;
    config.buffer_size = 4096;
    static const char *URL = "https://login.microsoftonline.com/c2e34095-aca9-442b-9f55-6256fc452083/oauth2/v2.0/token";
    config.url = URL;
    config.host = "login.microsoftonline.com";
    // config.path = "/c2e34095-aca9-442b-9f55-6256fc452083/oauth2/v2.0/token";

    //��ʼ���ṹ��
    esp_http_client_handle_t client = esp_http_client_init(&config); //��ʼ��http����

    //���÷�������
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    //esp_http_client_set_header(client, "Host", "login.microsoftonline.com");

    int content_length = 0;
    const char *post_data = "grant_type=refresh_token&refresh_token=0.AUoAlUDjwqmsK0SfVWJW_EUggwjS333L5ChOpm5_9DAFFNNKAKs.AgABAAAAAAD--DLA3VO7QrddgJg7WevrAgDs_wQA9P9AVE_9wR05hdSZ11EaOPGkiE16kZJHPmVGp06DVKzFKuam2Fa71rpCUx4wa_hHWpnmM88Zu8y45wWj-5SCgI4owAh1k7xuvG0GD2jUBsCyQoa3XdKLiNWFjU28Gdn7mL-_-B0WtjLJ6m-hwnGWfM6DXQcgKrSiz7_r46l11KTAi6XKlh1eyi3F6U1D33ST5tvj3Tw70KFy1h6fg-mGg9iHdzXeCE0qpTyVB57I4sO5RegPu5X956QO8Jl_C5_DKJvD0KDJHfA2g2frCSCOX0RNTXjBsYsbEKJhgySeWYVPAVj3q-qMQfMH3Gh2uVqXMXKViV9eao-N7L19RVfldmsjPnE4f71IWrpMlTKxuahhBUcpjgasOolxQMkKc7oeCNh0Zom0jqtD84o9nFfMTjaiy8_9cMIgv6KQLG_wYmsITF91iK-PyrY4M_17TczGtzFmsVJRjQSwlKM0482_ZMVZMp4YVVqGbVA2DU_75tNUDDaImMtO7cEh5Y8YQnnQ4FM4gyUO5eTntgpnaNHQfMW6bZ7-ltPSOkautb1qYrj0qCi0dNyTDvVGAU7Vqy4vGwLWxsprXc_kFGm0gXO6IMgCJTrg1scyoYAkgfwZ9EqcwLRLFkZhSwryDft25qCAxGHrPSk7Dli_WmvcxfMLcAFh7XnHQ3OfPQvZzu0y7zsNtpn2HlO_MinvLK81MqvEBjttT2VFkilwsquuuHfF3lePqfx7GkNB6OhJ-V3kytSM7d46fLhPddOZ1KcSqUZtBcldjX4Lbn5V33Uh7OOSxpgBnvfxgLsCQdc5u_ajx_3R_S2rfv6Sgq6azlRbJYDsyHiwh2gcdvrcwDF9cGQe7svdCkGaz6_g4VV03pfpjsJMMNWflVgE3V1yIfRdPhkBxZnF4GAOVg&client_id=7ddfd208-e4cb-4e28-a66e-7ff4300514d3&client_secret=G~t8Q~wMLfVgx6jw1T3Z8POWIRLpNXxPxJVh5b5K&scope=https://graph.microsoft.com/.default offline_access&redirect_uri=https://oauth.pstmn.io/v1/callback";
    //esp_http_client_set_post_field(client, post_data, strlen(post_data));

    while (1)
    {

        // ��Ŀ�������������ӣ���������д�����ݳ���Ϊ0
        //��Ϊ�����post���󣬻��ڱ��ĵ�ͷ���������Ҫ����������͵�����
        //������get���������͵����ݶ���URL���棬���ڱ���ͷ��������Ҫ�������Ĳ��֣����д�볤�Ⱦ���0
        esp_err_t err = esp_http_client_open(client, strlen(post_data));
        // esp_err_t err = esp_http_client_perform(client);
        //�������ʧ��
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        }
        //������ӳɹ�
        else
        {
            int wlen = esp_http_client_write(client, post_data, strlen(post_data));
            if (wlen < 0)
            {
                ESP_LOGE(TAG, "Write failed");
            }
            content_length = esp_http_client_fetch_headers(client);
            if (content_length < 0)
            {
                ESP_LOGE(TAG, "HTTP client fetch headers failed");
            }
            else
            {
                int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
                if (data_read >= 0)
                {
                    ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                             esp_http_client_get_status_code(client),
                             esp_http_client_get_content_length(client));
                    //ESP_LOG_BUFFER_HEX(TAG, output_buffer, strlen(output_buffer));
                    printf("Data:%s\r\n", output_buffer);
                    cJSON *root = NULL;
                    root = cJSON_Parse(output_buffer);
                    cJSON *CJSON_access_token = cJSON_GetObjectItem(root, "access_token");
                    if (CJSON_access_token != NULL)
                    {
                        strcpy(AccessCode, CJSON_access_token->valuestring);
                        ESP_LOGI(TAG, "Get access code!");
                        // printf("AccessCode is:%s\r\n",AccessCode);
                        uint32_t res = 1;
                        xQueueSend(GetAeecssCode_queue, (void *)&res, (TickType_t)10);
                        break;
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to read response");
                }
            }
        }
        ESP_LOGE(TAG, "Failed to get the access code!");
        vTaskDelay(6000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "Delete the Task!");
    vTaskDelete(NULL);
}
/*static void GetTime(void *pvParameters)
{
    esp_http_client_config_t config;
    memset(&config, 0, sizeof(config));
    //�����ýṹ���ڲ�д��url
    config.buffer_size_tx = 4096;
    config.buffer_size = 2048;
    static const char *URL = "https://graph.microsoft.com/v1.0/me/todo/lists/AQMkAGFmYjhjNWNiLTA0YmMtNGZiZC05ZGMxLTMwM2I1ODNkMWE5YwAuAAADuNMsdt9ULkG68AWXdu9SlgEAzPKXK_YW5UibTHO5BXCzowAAAgESAAAA/tasks";
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
                    printf("data:%s\r\n", output_buffer);
                    cJSON *root = NULL;
                    root = cJSON_Parse(output_buffer);
                    cJSON *error = cJSON_GetObjectItem(CJSON_value, "error");
                    if (error != NULL)
                    {
                        cJSON *innerError = cJSON_GetObjectItem(CJSON_value, "innerError");
                        cJSON *date = cJSON_GetObjectItem(CJSON_value, "date");
                        char a[100] = "";
                        sprintf(a, "%s", date->valuestring);
                        printf("%s\r\n", a);
                        char DateCommand[100];
                        if()
                        int count = uart_write_bytes(UART_NUM_1, test, strlen(test));
                        uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
                    }
                }
                //������ɹ�
                else
                {
                    ESP_LOGE(TAG, "Failed to read response");
                }
            }
        }
        ESP_LOGI(TAG, "End This Post");
        vTaskDelay(6000 / portTICK_PERIOD_MS);
    }
}*/
static void http_post_task(void *pvParameters)
{
    esp_http_client_config_t config;
    memset(&config, 0, sizeof(config));
    //�����ýṹ���ڲ�д��url
    config.buffer_size_tx = 4096;
    config.buffer_size = 2048;
    static const char *URL = "https://graph.microsoft.com/v1.0/me/todo/lists/AQMkAGFmYjhjNWNiLTA0YmMtNGZiZC05ZGMxLTMwM2I1ODNkMWE5YwAuAAADuNMsdt9ULkG68AWXdu9SlgEAzPKXK_YW5UibTHO5BXCzowAAAgESAAAA/tasks";
    config.url = URL;

    //��ʼ���ṹ��
    esp_http_client_handle_t client = esp_http_client_init(&config); //��ʼ��http����

    //���÷�������
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Authorization", AccessCode);
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
                    printf("data:%s\r\n", output_buffer);
                    //Analyse_Weather(output_buffer);
                    cJSON *root = NULL;
                    root = cJSON_Parse(output_buffer);
                    cJSON *CJSON_value_array = cJSON_GetObjectItem(root, "value");
                    if (CJSON_value_array != NULL)
                    {
                        cJSON *CJSON_value = cJSON_GetArrayItem(CJSON_value_array, 0);
                        cJSON *title = cJSON_GetObjectItem(CJSON_value, "title");
                        char a[100] = "";
                        sprintf(a, "%s", title->valuestring);
                        printf("%s\r\n", a);
                    }
                }
                //������ɹ�
                else
                {
                    ESP_LOGE(TAG, "Failed to read response");
                }
            }
        }
        ESP_LOGI(TAG, "End This Post");
        vTaskDelay(6000 / portTICK_PERIOD_MS);
    }
}
void Sntp_init(void)
{

    ESP_LOGI(TAG, "------------Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "cn.pool.ntp.org"); //���÷��ʷ�����	�й��ṩ��

    sntp_init();
    
}

void stopSNTP(void)
{
    sntp_stop();
}
struct tm getNowTime(void)
{
    char strftime_buf[64];
    time_t now;
    struct tm timeinfo;
    setenv("TZ", "CST-8", 1);
    tzset();

    GetTime:

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    if(timeinfo.tm_year==70)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGE(TAG, "Get Time Error!");
        goto GetTime;
    }
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
    char command[100] = "";

    sprintf(Today,"%04d-%02d-%02d\"",timeinfo.tm_year+1900,timeinfo.tm_mon+1,timeinfo.tm_mday);
    ESP_LOGI(TAG, "Today is: %s", Today);

    sprintf(command,"t3.txt=\"%d��%02d��%02d��\"",timeinfo.tm_year+1900,timeinfo.tm_mon+1,timeinfo.tm_mday);
    uart_write_bytes(UART_NUM_1, command, strlen(command));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    
    if(timeinfo.tm_sec+8>60)
    {
        timeinfo.tm_sec+=(8-60);
        if(timeinfo.tm_min==59)
        {
            timeinfo.tm_min=0;
            timeinfo.tm_hour+=1;
        }
        else timeinfo.tm_min+=1;
    }
    else timeinfo.tm_sec+=8;
    sprintf(command,"hour.val=%d",timeinfo.tm_hour);
    uart_write_bytes(UART_NUM_1, command, strlen(command));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    sprintf(command,"min.val=%d",timeinfo.tm_min);
    uart_write_bytes(UART_NUM_1, command, strlen(command));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    sprintf(command,"sec.val=%d",timeinfo.tm_sec);
    uart_write_bytes(UART_NUM_1, command, strlen(command));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    sprintf(command,"tm0.en=1");
    uart_write_bytes(UART_NUM_1, command, strlen(command));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    return timeinfo;
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

    Sntp_init();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    getNowTime();
    stopSNTP();

    xTaskCreate(&GetWeather, "GetWeather", 16384, NULL, 5, NULL);
    uint32_t result = 0;
    while (1)
    {

        if (xQueueReceive(GetAeecssCode_queue, &result, (portTickType)portMAX_DELAY))
        {
            ESP_LOGI(TAG, "Get Access Code");
            break;
        }
    }


    xTaskCreate(&GetAeecssCode, "GetAeecssCode", 16384, NULL, 5, NULL);
    while (1)
    {

        if (xQueueReceive(GetAeecssCode_queue, &result, (portTickType)portMAX_DELAY))
        {
            ESP_LOGI(TAG, "Get Access Code");
            break;
        }
    }
    xTaskCreate(&http_post_task, "http_post_task", 16384, NULL, 5, NULL);
}