/*
 * @Author: xtx
 * @Date: 2022-05-09 23:14:36
 * @LastEditors: xtx
 * @LastEditTime: 2022-05-12 03:31:25
 * @FilePath: /template-app/main/main.c
 * @Description: 请填写简介
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

#include "driver/ledc.h"
#include "cJSON.h"

#define EX_UART_NUM UART_NUM_1
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
//static QueueHandle_t uart1_queue;
typedef uint8_t u8;
#define PATTERN_CHR_NUM (3) /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define MAX_HTTP_OUTPUT_BUFFER 4096

static const char *TAG = "xtxTest";

static QueueHandle_t GetAeecssCode_queue;

static QueueHandle_t uart1_queue;
int cnt = 0;

static char data_recv[1024] = "";

struct info
{
    char AccessCode[2048];

    char NowDate[100];

} info;

struct taskInfo
{
    u8 state;
    char id[1024];
    u8 isNeedFinish;
} taskInfo[4];

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
    // uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart1_queue, 0);
    ESP_LOGI(TAG, "Uart1 Prepare in GPIO6_TX GPIO7_RX");

    // char test[100] = "t6.txt=\"今天天气是下雨呢QAQ\"";
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    // uart_write_bytes(UART_NUM_1, test, strlen(test));
    // uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    // uart_write_bytes(UART_NUM_1, test, strlen(test));
    // uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
}
static void uart_analyse(char *dtmp)
{
    for (int i = 0; i < strlen((const char *)dtmp); i++)
    {
        data_recv[cnt++] = dtmp[i];
        if (data_recv[cnt - 1] == '@')
        {

            int taskNum = data_recv[4] - '0';
            if (taskInfo[taskNum].state == 1)
            {
                taskInfo[taskNum].isNeedFinish = 1;
                ESP_LOGI(TAG, "Want to finish %s task", data_recv);
            }
            cnt = 0;
            memset(data_recv, 0, sizeof(data_recv));
        }
    }
}
static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(uart1_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                    uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
                    ESP_LOGI(TAG, "[DATA EVT]:");
                    uart_write_bytes(EX_UART_NUM, (const char*) dtmp, event.size);
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart1_queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart1_queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error");
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                    int pos = uart_pattern_pop_pos(EX_UART_NUM);
                    ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                    if (pos == -1) {
                        // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                        // record the position. We should set a larger queue size.
                        // As an example, we directly flush the rx buffer here.
                        uart_flush_input(EX_UART_NUM);
                    } else {
                        uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                        uint8_t pat[PATTERN_CHR_NUM + 1];
                        memset(pat, 0, sizeof(pat));
                        uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                        ESP_LOGI(TAG, "read data: %s", dtmp);
                        ESP_LOGI(TAG, "read pat : %s", pat);
                    }
                    break;
                //Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}


static void GetAeecssCode(void *pvParameters)
{
    esp_http_client_config_t config;
    memset(&config, 0, sizeof(config));
    //向配置结构体内部写入url
    config.buffer_size_tx = 4096;
    config.buffer_size = 4096;
    static const char *URL = "https://login.microsoftonline.com/c2e34095-aca9-442b-9f55-6256fc452083/oauth2/v2.0/token";
    config.url = URL;
    config.host = "login.microsoftonline.com";
    // config.path = "/c2e34095-aca9-442b-9f55-6256fc452083/oauth2/v2.0/token";

    //初始化结构体
    esp_http_client_handle_t client = esp_http_client_init(&config); //初始化http连接

    //设置发送请求
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    //esp_http_client_set_header(client, "Host", "login.microsoftonline.com");

    int content_length = 0;
    const char *post_data = "grant_type=refresh_token&refresh_token=0.AUoAlUDjwqmsK0SfVWJW_EUggwjS333L5ChOpm5_9DAFFNNKAKs.AgABAAAAAAD--DLA3VO7QrddgJg7WevrAgDs_wQA9P9AVE_9wR05hdSZ11EaOPGkiE16kZJHPmVGp06DVKzFKuam2Fa71rpCUx4wa_hHWpnmM88Zu8y45wWj-5SCgI4owAh1k7xuvG0GD2jUBsCyQoa3XdKLiNWFjU28Gdn7mL-_-B0WtjLJ6m-hwnGWfM6DXQcgKrSiz7_r46l11KTAi6XKlh1eyi3F6U1D33ST5tvj3Tw70KFy1h6fg-mGg9iHdzXeCE0qpTyVB57I4sO5RegPu5X956QO8Jl_C5_DKJvD0KDJHfA2g2frCSCOX0RNTXjBsYsbEKJhgySeWYVPAVj3q-qMQfMH3Gh2uVqXMXKViV9eao-N7L19RVfldmsjPnE4f71IWrpMlTKxuahhBUcpjgasOolxQMkKc7oeCNh0Zom0jqtD84o9nFfMTjaiy8_9cMIgv6KQLG_wYmsITF91iK-PyrY4M_17TczGtzFmsVJRjQSwlKM0482_ZMVZMp4YVVqGbVA2DU_75tNUDDaImMtO7cEh5Y8YQnnQ4FM4gyUO5eTntgpnaNHQfMW6bZ7-ltPSOkautb1qYrj0qCi0dNyTDvVGAU7Vqy4vGwLWxsprXc_kFGm0gXO6IMgCJTrg1scyoYAkgfwZ9EqcwLRLFkZhSwryDft25qCAxGHrPSk7Dli_WmvcxfMLcAFh7XnHQ3OfPQvZzu0y7zsNtpn2HlO_MinvLK81MqvEBjttT2VFkilwsquuuHfF3lePqfx7GkNB6OhJ-V3kytSM7d46fLhPddOZ1KcSqUZtBcldjX4Lbn5V33Uh7OOSxpgBnvfxgLsCQdc5u_ajx_3R_S2rfv6Sgq6azlRbJYDsyHiwh2gcdvrcwDF9cGQe7svdCkGaz6_g4VV03pfpjsJMMNWflVgE3V1yIfRdPhkBxZnF4GAOVg&client_id=7ddfd208-e4cb-4e28-a66e-7ff4300514d3&client_secret=G~t8Q~wMLfVgx6jw1T3Z8POWIRLpNXxPxJVh5b5K&scope=https://graph.microsoft.com/.default offline_access&redirect_uri=https://oauth.pstmn.io/v1/callback";
    //esp_http_client_set_post_field(client, post_data, strlen(post_data));
    int errorTime = 0;
    while (1)
    {

        // 与目标主机创建连接，并且声明写入内容长度为0
        //因为如果是post请求，会在报文的头部后面跟着要向服务器发送的数据
        //而对于get方法，发送的内容都在URL里面，都在报文头部，不需要定义后面的部分，因此写入长度就是0
        esp_err_t err = esp_http_client_open(client, strlen(post_data));
        // esp_err_t err = esp_http_client_perform(client);
        //如果连接失败
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            errorTime++;
            if (errorTime == 10)
            {
                ESP_LOGE(TAG, "Error for 10 times!Rebot!");
                esp_restart();
            }
        }
        //如果连接成功
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
                    errorTime = 0;
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
                        strcpy(info.AccessCode, CJSON_access_token->valuestring);
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
static void GetWeather(void *pvParameters)
{
    esp_http_client_config_t config;
    memset(&config, 0, sizeof(config));
    //向配置结构体内部写入url
    static const char *URL = "https://api.seniverse.com/v3/weather/now.json?key=Suo-L-0OoOH2-YLPj&location=guangzhou&language=zh-Hans&unit=c";
    config.url = URL;

    //初始化结构体
    esp_http_client_handle_t client = esp_http_client_init(&config); //初始化http连接

    //设置发送请求
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
    int content_length = 0;
    while (1)
    {

        // 与目标主机创建连接，并且声明写入内容长度为0
        //因为如果是post请求，会在报文的头部后面跟着要向服务器发送的数据
        //而对于get方法，发送的内容都在URL里面，都在报文头部，不需要定义后面的部分，因此写入长度就是0
        esp_err_t err = esp_http_client_open(client, 0);

        //如果连接失败
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        }
        //如果连接成功
        else
        {

            //读取目标主机的返回内容的协议头
            content_length = esp_http_client_fetch_headers(client);

            //如果协议头长度小于0，说明没有成功读取到
            if (content_length < 0)
            {
                ESP_LOGE(TAG, "HTTP client fetch headers failed");
            }

            //如果成功读取到了协议头
            else
            {

                //读取目标主机通过http的响应内容
                int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
                if (data_read >= 0)
                {

                    //打印响应内容，包括响应状态，响应体长度及其内容
                    ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                             esp_http_client_get_status_code(client),     //获取响应状态信息
                             esp_http_client_get_content_length(client)); //获取响应信息长度
                    printf("data:%s\r\n", output_buffer);
                    cJSON *root = NULL;
                    root = cJSON_Parse(output_buffer);

                    cJSON *cjson_item = cJSON_GetObjectItem(root, "results");

                    if (cjson_item != NULL)
                    {
                        cJSON *cjson_results = cJSON_GetArrayItem(cjson_item, 0);
                        cJSON *cjson_now = cJSON_GetObjectItem(cjson_results, "now");
                        cJSON *cjson_temperature = cJSON_GetObjectItem(cjson_now, "temperature");
                        cJSON *cjson_weather = cJSON_GetObjectItem(cjson_now, "text");
                        // printf("%s\n", cjson_weather->valuestring);
                        //if(text)
                        char command[100] = "";
                        sprintf(command, "t6.txt=\"今天天气是%s呢\n气温是%s度撒\"", cjson_weather->valuestring, cjson_temperature->valuestring);
                        uart_write_bytes(UART_NUM_1, command, strlen(command));
                        uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);

                        uint32_t res = 1;
                        xQueueSend(GetAeecssCode_queue, (void *)&res, (TickType_t)10);
                        break;
                    }
                }
                //如果不成功
                else
                {
                    ESP_LOGE(TAG, "Failed to read response");
                }
            }
        }
        ESP_LOGI(TAG, "End This Post");
        vTaskDelay(6000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
static void FinishTask(void *pvParameters)
{
    int *tmp;
    tmp = (int *)pvParameters;
    int TaskNum = *tmp;
    esp_http_client_config_t config;
    memset(&config, 0, sizeof(config));
    //向配置结构体内部写入url
    config.buffer_size_tx = 4096;
    config.buffer_size = 2048;
    char URL[1024] = {0};
    sprintf(URL, "https://graph.microsoft.com/v1.0/me/todo/lists/AQMkAGFmYjhjNWNiLTA0YmMtNGZiZC05ZGMxLTMwM2I1ODNkMWE5YwAuAAADuNMsdt9ULkG68AWXdu9SlgEAzPKXK_YW5UibTHO5BXCzowAAAgESAAAA/tasks/%s", taskInfo[TaskNum].id);
    //static const char *URL = "https://graph.microsoft.com/v1.0/me/todo/lists/AQMkAGFmYjhjNWNiLTA0YmMtNGZiZC05ZGMxLTMwM2I1ODNkMWE5YwAuAAADuNMsdt9ULkG68AWXdu9SlgEAzPKXK_YW5UibTHO5BXCzowAAAgESAAAA/tasks";
    config.url = URL;

    //初始化结构体
    esp_http_client_handle_t client = esp_http_client_init(&config); //初始化http连接

    //设置发送请求
    esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    int content_length = 0;
    const char *post_data = "{\"status\": \"completed\"}";

    esp_http_client_set_header(client, "Authorization", info.AccessCode);
    char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
    int errorTime = 0;
    while (1)
    {

        // 与目标主机创建连接，并且声明写入内容长度为0
        //因为如果是post请求，会在报文的头部后面跟着要向服务器发送的数据
        //而对于get方法，发送的内容都在URL里面，都在报文头部，不需要定义后面的部分，因此写入长度就是0
        esp_err_t err = esp_http_client_open(client, strlen(post_data));

        //如果连接失败
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            errorTime++;
            if (errorTime == 10)
            {
                ESP_LOGE(TAG, "Error for 10 times!Rebot!");
                esp_restart();
            }
        }
        //如果连接成功
        else
        {

            //读取目标主机的返回内容的协议头
            content_length = esp_http_client_fetch_headers(client);

            //如果协议头长度小于0，说明没有成功读取到
            if (content_length < 0)
            {
                ESP_LOGE(TAG, "HTTP client fetch headers failed");
                errorTime++;
                if (errorTime == 10)
                {
                    ESP_LOGE(TAG, "Error for 10 times!Rebot!");
                    esp_restart();
                }
            }

            //如果成功读取到了协议头
            else
            {
                int wlen = esp_http_client_write(client, post_data, strlen(post_data));
                if (wlen < 0)
                {
                    ESP_LOGE(TAG, "Write failed");
                }
                //读取目标主机通过http的响应内容
                int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
                if (data_read >= 0)
                {
                    errorTime = 0;
                    //打印响应内容，包括响应状态，响应体长度及其内容
                    ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                             esp_http_client_get_status_code(client),     //获取响应状态信息
                             esp_http_client_get_content_length(client)); //获取响应信息长度
                    printf("data:%s\r\n", output_buffer);
                    //Analyse_Weather(output_buffer);
                    cJSON *root = NULL;
                    root = cJSON_Parse(output_buffer);
                    cJSON *status = cJSON_GetObjectItem(root, "status");
                    if (status != NULL)
                    {

                        taskInfo[TaskNum].state = 0;
                        taskInfo[TaskNum].isNeedFinish = 0;
                        break;
                    }
                }
                //如果不成功
                else
                {
                    ESP_LOGE(TAG, "Failed to read response");
                    errorTime++;
                    if (errorTime == 10)
                    {
                        ESP_LOGE(TAG, "Error for 10 times!Rebot!");
                        esp_restart();
                    }
                }
            }
        }
        ESP_LOGI(TAG, "End This Post");
        vTaskDelay(6000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void GetTask(void *pvParameters)
{
    esp_http_client_config_t config;
    memset(&config, 0, sizeof(config));
    //向配置结构体内部写入url
    config.buffer_size_tx = 4096;
    config.buffer_size = 2048;
    static const char *URL = "https://graph.microsoft.com/v1.0/me/todo/lists/AQMkAGFmYjhjNWNiLTA0YmMtNGZiZC05ZGMxLTMwM2I1ODNkMWE5YwAuAAADuNMsdt9ULkG68AWXdu9SlgEAzPKXK_YW5UibTHO5BXCzowAAAgESAAAA/tasks";
    config.url = URL;

    //初始化结构体
    esp_http_client_handle_t client = esp_http_client_init(&config); //初始化http连接

    //设置发送请求
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Authorization", info.AccessCode);
    char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
    int content_length = 0;
    int errorTime = 0;
    while (1)
    {

        // 与目标主机创建连接，并且声明写入内容长度为0
        //因为如果是post请求，会在报文的头部后面跟着要向服务器发送的数据
        //而对于get方法，发送的内容都在URL里面，都在报文头部，不需要定义后面的部分，因此写入长度就是0
        esp_err_t err = esp_http_client_open(client, 0);

        //如果连接失败
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            errorTime++;
            if (errorTime == 10)
            {
                ESP_LOGE(TAG, "Error for 10 times!Rebot!");
                esp_restart();
            }
        }
        //如果连接成功
        else
        {
            errorTime = 0;
            //读取目标主机的返回内容的协议头
            content_length = esp_http_client_fetch_headers(client);

            //如果协议头长度小于0，说明没有成功读取到
            if (content_length < 0)
            {
                ESP_LOGE(TAG, "HTTP client fetch headers failed");
                errorTime++;
                if (errorTime == 10)
                {
                    ESP_LOGE(TAG, "Error for 10 times!Rebot!");
                    esp_restart();
                }
            }

            //如果成功读取到了协议头
            else
            {

                //读取目标主机通过http的响应内容
                int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
                if (data_read >= 0)
                {
                    //打印响应内容，包括响应状态，响应体长度及其内容
                    ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                             esp_http_client_get_status_code(client),     //获取响应状态信息
                             esp_http_client_get_content_length(client)); //获取响应信息长度
                    printf("data:%s\r\n", output_buffer);
                    //Analyse_Weather(output_buffer);
                    cJSON *root = NULL;
                    root = cJSON_Parse(output_buffer);
                    cJSON *CJSON_value_array = cJSON_GetObjectItem(root, "value");
                    if (CJSON_value_array != NULL)
                    {
                        int taskNum = 0;
                        int num = cJSON_GetArraySize(CJSON_value_array);
                        for (int i = 0; i < num; i++)
                        {
                            if (taskNum == 4)
                                break;
                            cJSON *CJSON_value = cJSON_GetArrayItem(CJSON_value_array, i);
                            cJSON *reminderDateTime = cJSON_GetObjectItem(CJSON_value, "reminderDateTime");
                            if (reminderDateTime == NULL)
                            {
                                ESP_LOGI(TAG, "No reminderDateTime");
                                continue;
                            }
                            cJSON *dateTime = cJSON_GetObjectItem(reminderDateTime, "dateTime");
                            printf("UTC is:%s\r\n", dateTime->valuestring);

                            //UTC时间处理
                            char ChinaTime[100] = {0};
                            strcpy(ChinaTime, dateTime->valuestring);
                            int hour = (ChinaTime[11] - '0') * 10 + (ChinaTime[12] - '0');
                            // ESP_LOGI(TAG, "UTCHour is:%d\r\n", hour);
                            if (hour >= 16)
                            {
                                hour = hour + 8 - 24;
                                int day = (ChinaTime[8] - '0') * 10 + (ChinaTime[9] - '0');
                                day += 1;
                                ChinaTime[8] = day / 10 + '0';
                                ChinaTime[9] = day % 10 + '0';
                            }
                            else
                            {
                                hour += 8;
                            }
                            // ESP_LOGI(TAG, "NowHour is:%d\r\n", hour);
                            ChinaTime[11] = hour / 10 + '0';
                            ChinaTime[12] = hour % 10 + '0';
                            ESP_LOGI(TAG, "UTC+8 is:%s\r\n", ChinaTime);

                            //TODO:已知bug，对于日的进位，月的判断都没做

                            if (strncmp(ChinaTime, info.NowDate, 10) == 0)
                            {
                                cJSON *status = cJSON_GetObjectItem(CJSON_value, "status");
                                if (strcmp(status->valuestring, "completed") == 0)
                                {
                                    continue;
                                }

                                //有效任务，添加到队列中
                                taskNum += 1;
                                //如果该任务需要被完成
                                if (taskInfo[taskNum].isNeedFinish == 1)
                                {
                                    ESP_LOGI(TAG, "Delete %d task at first", taskNum);
                                    xTaskCreate(&FinishTask, "FinishTask", 16384, (void *)taskNum, 5, NULL);
                                    while (1)
                                    {
                                        int result;
                                        if (xQueueReceive(GetAeecssCode_queue, &result, (portTickType)portMAX_DELAY))
                                        {
                                            ESP_LOGI(TAG, "Finish a task");
                                            break;
                                        }
                                    }
                                    continue;
                                }
                                cJSON *id = cJSON_GetObjectItem(CJSON_value, "id");
                                taskInfo[taskNum].state = 1;
                                strcpy(taskInfo[taskNum].id, id->valuestring);

                                cJSON *title = cJSON_GetObjectItem(CJSON_value, "title");
                                char a[100] = "";
                                sprintf(a, "%s", title->valuestring);
                                printf("Mission %s\r\n", a);

                                char command[100] = "";
                                char time[100] = "";
                                strncpy(time, ChinaTime + 11, 5);
                                sprintf(command, "task%d.txt=\"%s %s\"", taskNum, a, time);
                                uart_write_bytes(UART_NUM_1, command, strlen(command));
                                uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
                            }
                            else
                            {
                                ESP_LOGI(TAG, "No =");
                                printf("Today is:%s\r\n", info.NowDate);
                            }
                        }

                        //显示任务透明度
                        ESP_LOGI(TAG, "taskNum=%d", taskNum);
                        char command[100] = "";
                        if (taskNum >= 1)
                        {
                            sprintf(command, "task1.aph=90");
                        }
                        else
                        {
                            sprintf(command, "task1.aph=0");
                            taskInfo[1].state = 0;
                        }
                        uart_write_bytes(UART_NUM_1, command, strlen(command));
                        uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
                        if (taskNum >= 2)
                        {
                            sprintf(command, "task2.aph=90");
                        }
                        else
                        {
                            sprintf(command, "task2.aph=0");
                            taskInfo[2].state = 0;
                        }
                        uart_write_bytes(UART_NUM_1, command, strlen(command));
                        uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
                        if (taskNum >= 3)
                        {
                            sprintf(command, "task3.aph=90");
                        }
                        else
                        {
                            sprintf(command, "task3.aph=0");
                            taskInfo[3].state = 0;
                        }
                        uart_write_bytes(UART_NUM_1, command, strlen(command));
                        uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
                    }
                }
                //如果不成功
                else
                {
                    ESP_LOGE(TAG, "Failed to read response");
                    errorTime++;
                    if (errorTime == 10)
                    {
                        ESP_LOGE(TAG, "Error for 10 times!Rebot!");
                        esp_restart();
                    }
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
    sntp_setservername(0, "cn.pool.ntp.org"); //设置访问服务器	中国提供商

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
    if (timeinfo.tm_year == 70)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGE(TAG, "Get Time Error!");
        goto GetTime;
    }
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
    char command[100] = "";

    sprintf(info.NowDate, "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    ESP_LOGI(TAG, "Today is: %s", info.NowDate);

    sprintf(command, "t3.txt=\"%d年%02d月%02d日\"", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    uart_write_bytes(UART_NUM_1, command, strlen(command));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);

    if (timeinfo.tm_sec + 5 > 60)
    {
        timeinfo.tm_sec = timeinfo.tm_sec + 5 - 60;
        if (timeinfo.tm_min == 59)
        {
            timeinfo.tm_min = 0;
            timeinfo.tm_hour += 1;
        }
        else
            timeinfo.tm_min += 1;
    }
    else
    {
        timeinfo.tm_sec += 5;
    }
    sprintf(command, "hour.val=%d", timeinfo.tm_hour);
    uart_write_bytes(UART_NUM_1, command, strlen(command));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    sprintf(command, "min.val=%d", timeinfo.tm_min);
    uart_write_bytes(UART_NUM_1, command, strlen(command));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    sprintf(command, "sec.val=%d", timeinfo.tm_sec);
    uart_write_bytes(UART_NUM_1, command, strlen(command));
    uart_write_bytes(UART_NUM_1, "\xff\xff\xff", 3);
    sprintf(command, "tm0.en=1");
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

    GetAeecssCode_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(&GetWeather, "GetWeather", 16384, NULL, 5, NULL);
    uint32_t result = 0;
    while (1)
    {

        if (xQueueReceive(GetAeecssCode_queue, &result, (portTickType)portMAX_DELAY))
        {
            ESP_LOGI(TAG, "Get Weather!");
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

    printf("AccessCode is:%s\r\n", info.AccessCode);
    printf("Today is:%s\r\n", info.NowDate);
    // ESP_LOGI(TAG, "%x\n",&AccessCode[0]);
    // ESP_LOGI(TAG, "%x\n",&dateTime[0]);
    xTaskCreate(&GetTask, "GetTask", 16384, NULL, 5, NULL);
    xTaskCreate(&uart_event_task, "uart_event_task", 2048, NULL, 5, NULL);
}