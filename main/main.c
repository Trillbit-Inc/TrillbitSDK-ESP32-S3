/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2s.h"
#include "bsp_board.h"
#include "bsp_codec.h"
#include "esp_dsp.h"
#include "esp_spiffs.h"
#include "bsp_i2s.h"
#include "bsp_lcd.h"
#include "lv_port.h"
#include "lvgl.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_task.h"
#include "esp_heap_caps.h"

#include "trill.h"
#include "trill_error.h"
#include "trill_test.h"

#include "storage.h"
#include "app_config.h"
#include "ui.h"
#include "version.h"
#include "serial_com.h"

static const char *TAG = "main";


#define FEED_TASK_STACK_SIZE        (4*1024)
#define N_MICS_ON_BOARD             2
#define CPU_FREQ_MHZ                240
#define CPU_CLK_PERIOD_USEC         (1.0 / CPU_FREQ_MHZ)
#define BLOCK_N_SAMPLES             1024
#define I2S_CHANNEL_NUM             (2)
#define OUTPUT_SAMPLES_N            BLOCK_N_SAMPLES
#define OUTPUT_SAMPLES_BUFFER_SIZE  (I2S_CHANNEL_NUM * OUTPUT_SAMPLES_N * sizeof(int16_t))
#define INPUT_SAMPLES_BLOCK_SIZE    (BLOCK_N_SAMPLES * sizeof(int16_t))
#define OUTPUT_SAMPLES_BLOCK_SIZE   (I2S_CHANNEL_NUM * OUTPUT_SAMPLES_N * sizeof(int16_t))

#define EG_SDK_FEED_TASK_STOP_BIT   (1<<0)
#define EG_SDK_PLAY_TASK_STOP_BIT   (1<<1)
#define EG_SDK_TRILL_TASK_STOP_BIT  (1<<2)
#define EG_SDK_FEED_TASK_REQ_BIT    (1<<3)
#define EG_SDK_PLAY_TASK_REQ_BIT    (1<<4)
#define EG_SDK_TRILL_TASK_REQ_BIT   (1<<5)

#define FIRST_MSG_FROM_BOARD        "Hello from s3-box!"

static int16_t* input_samples;
static int16_t* output_samples;
static trill_init_opts_t trill_init_opts;
static int tx_audio_enabled;
static trill_tx_params_t last_tx_params;
static EventGroupHandle_t eg_sdk_tasks_ctrl;


static char last_rx_data[TRILL_MAX_DATA_PAYLOAD_LEN+1];

static void* trill_handle;

static void provision_license(void);
static int start_sdk_tasks(void);
static int stop_sdk_tasks(void);
static int sdk_on_off(void);
static int do_init_trill(void);

static unsigned int print_mem_free_info(void)
{
    unsigned int def_free;

    printf("Free MALLOC_CAP_8BIT: %u\n", 
        heap_caps_get_free_size(MALLOC_CAP_8BIT));
    printf("Free MALLOC_CAP_SPIRAM: %u\n", 
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("Free MALLOC_CAP_INTERNAL: %u\n", 
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    def_free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    printf("Free MALLOC_CAP_DEFAULT: %u\n", 
        def_free);
    printf("xPortGetFreeHeapSize: %u\n", xPortGetFreeHeapSize());
    
    return def_free;
}

static void board_audio_tx_enable_cb(int enable)
{
    tx_audio_enabled = enable ? 1 : 0;

    ui_en_echo(!enable);

    printf("board_audio_tx_enable_cb, TX: %s\n", enable ? "ENABLED": "DISABLED");
}

static int send_callback(void)
{
    int msg_len = strlen(last_rx_data);

    printf("Attemping to send msg with len: %d\n", msg_len);
    int ret = trill_tx_data(trill_handle, &last_tx_params, (uint8_t*) last_rx_data, msg_len);
    if (ret < 0)
    {
        printf("trill_tx_data failed: %d\n", ret);
    }

    return ret;
}

void play_task(void *arg)
{
    int ret;
    int16_t* tx_data = NULL;
    size_t i2s_bytes_written = 0;
    size_t input_size = 0;
    
    printf("Starting play task.\n");

    while (
        (xEventGroupWaitBits(
        eg_sdk_tasks_ctrl,
        EG_SDK_PLAY_TASK_REQ_BIT,
        pdTRUE,
        pdTRUE,
        0) & EG_SDK_PLAY_TASK_REQ_BIT) == 0
    )
    {
        ret = trill_acquire_audio_block(trill_handle, &tx_data, 0);
        if (ret < 0)
        {
            if (ret != TRILL_ERR_AUDIO_TX_BLOCK_NOT_AVAILABLE)
            {
                printf("trill_acquire_audio_block = %d\n", ret);
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
        }
        
        // mono to stereo
        for (int n = 0, i = 0; n < BLOCK_N_SAMPLES; n++, i += I2S_CHANNEL_NUM)
        {
            output_samples[i] = tx_data[n];
            output_samples[i+1] = tx_data[n];
        }

        trill_release_audio_block(trill_handle);
        input_size = OUTPUT_SAMPLES_BUFFER_SIZE;

        i2s_write(I2S_NUM_0, 
            output_samples, 
            input_size, 
            &i2s_bytes_written, 
            portMAX_DELAY);
    }

    printf("play task stopped\n");
    xEventGroupSetBits(eg_sdk_tasks_ctrl, EG_SDK_PLAY_TASK_STOP_BIT);

    vTaskDelete(NULL);
}

void trill_task(void *arg)
{
    int ret;

    (void) arg;

    while (
        (xEventGroupWaitBits(
        eg_sdk_tasks_ctrl,
        EG_SDK_TRILL_TASK_REQ_BIT,
        pdTRUE,
        pdTRUE,
        0) & EG_SDK_TRILL_TASK_REQ_BIT) == 0
    )
    {
        ret = trill_process(trill_handle);
        if (ret < 0)
        {
            if (ret == TRILL_ERR_USER_ABORTED_TX)
            {
                printf("trill_process_input: App requested tx abort.\n");    
            }
            else
            {
                printf("trill_process_input = %d\n", ret);
            }
        }
    }

    xEventGroupSetBits(eg_sdk_tasks_ctrl, EG_SDK_TRILL_TASK_STOP_BIT);
    printf("trill task stopped\n");
    vTaskDelete(NULL);
}

void feed_task(void *arg)
{
    int ret;
    size_t err_count = 0;
    int feed_channels = N_MICS_ON_BOARD;
    unsigned int rx_block_size = BLOCK_N_SAMPLES * sizeof(int16_t) * feed_channels;
    
    int16_t *audio_rx_buff = heap_caps_malloc(rx_block_size, MALLOC_CAP_INTERNAL);
    assert(audio_rx_buff);
    
    printf("Starting audio rx task. channels=%d\n", feed_channels);

    while (
        (xEventGroupWaitBits(
        eg_sdk_tasks_ctrl,
        EG_SDK_FEED_TASK_REQ_BIT,
        pdTRUE,
        pdTRUE,
        0) & EG_SDK_FEED_TASK_REQ_BIT) == 0
        )
    {
        size_t bytes_read = 0;

        /* Read audio data from I2S bus */
        ret = i2s_read(I2S_NUM_0, audio_rx_buff, 
                BLOCK_N_SAMPLES * feed_channels * sizeof(int16_t), 
                &bytes_read, portMAX_DELAY);
        if (ret != ESP_OK)
        {
            printf("i2s_read failed = %d\n", ret);
        }
        
        // feed input only when tx is not in progress.
        // but keep clearing audio data from ADC.
        if (!tx_audio_enabled)
        {
            ret = trill_add_audio_block(trill_handle, audio_rx_buff);
            if (ret < 0)
            {
                printf("%u) trill_add_audio_block = %d\n", err_count++, ret);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }

    free(audio_rx_buff);
    xEventGroupSetBits(eg_sdk_tasks_ctrl, EG_SDK_FEED_TASK_STOP_BIT);

    printf("feed task stopped\n");

    vTaskDelete(NULL);
}

static void log_print(const char* data)
{
	printf("%s", data);
}

static unsigned long timer_us(void)
{
    return (dsp_get_cpu_cycle_count() * 0.00416666666); // cpu clock 240MHz.
}

static void data_link_evt_handler(const trill_data_link_event_params_t* params)
{
	static int count = 0;
    
	switch(params->event)
	{
		case TRILL_DATA_LINK_EVT_DATA_RCVD:
            count++;
			printf("chn-%d: data-link event = %d, RX SSI%d, PLen: %d, payload content: %.*s\n",
                params->channel,
                params->event, params->ssi, 
                params->payload_len,
                params->payload_len, params->payload);

            memset(last_rx_data, 0, sizeof(last_rx_data));
            last_tx_params.ssi = params->ssi;
            last_tx_params.data_cfg_range = params->data_cfg_range;
                
            // send only ascii chars to ui.
            for (int i = 0; (i < params->payload_len) && (i < (sizeof(last_rx_data)-1)); i++)
            {
                if (params->payload[i] < 127)
                    last_rx_data[i] = (char) params->payload[i];
                else
                    last_rx_data[i] = '-';
            }
                
            ui_set_rx_msg(last_rx_data);
            break;
		case TRILL_DATA_LINK_EVT_DATA_SENT:
			printf("Packet sent with length: %d\n", params->payload_len);
            break;
		default:
			printf("Unknown data link event");
			break;
	}
}

static inline void* tsdk_mem_alloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
}

static inline void tsdk_mem_free(void* ptr)
{
    if (ptr == NULL)
    {
        printf("***Trying to free NULL pointer***\n");
    }

    free(ptr);
}

static inline void* tsdk_mem_aligned_alloc(unsigned int alignment, size_t size)
{
    if (alignment != 16)
    {
        return heap_caps_aligned_alloc(alignment, size, MALLOC_CAP_SPIRAM);
    }
    else
    {
        // ESP-DSP blocks need 16 byte alignment buffers.
        // Allocate them in internal memory for better performance.
        return heap_caps_aligned_alloc(alignment, size, MALLOC_CAP_INTERNAL);
    }
    
}

static int sdk_on_off(void)
{
    int ret;

    if (!trill_handle)
    {
        ret = start_sdk_tasks();
        if (ret == 0)
        {
            ui_en_echo(1);
            return 1;
        }
        else
        {
            ui_en_echo(0);
            return ret;
        }
    }
    else
    {
        return stop_sdk_tasks();
    }
}

static int start_sdk_tasks(void)
{
    int ret;

    if (trill_handle == NULL) // need trill init
    {
        ret = do_init_trill();
        if (ret < 0)
        {
            return ret;
        }
    }

    ret = xTaskCreatePinnedToCore(
            &feed_task, // func code
            "feed", // name
            FEED_TASK_STACK_SIZE,  //stack size
            (void*)NULL, //task args
            6, //  priority
            NULL, // get handle
            1); // core id
    if (ret != pdPASS)
    {
        printf("feed_task create failed: %d\n", ret);
        return -1;
    }

    ret = xTaskCreatePinnedToCore(&play_task, "play", 4 * 1024, (void*)NULL, 5, NULL, 1);
    if (ret != pdPASS)
    {
        printf("play_task create failed: %d\n", ret);
        return -1;
    }

    ret = xTaskCreatePinnedToCore(&trill_task, "trill", 4 * 1024, (void*)NULL, 
            0, NULL, 1);
    if (ret != pdPASS)
    {
        printf("trill_task create failed: %d\n", ret);
        return -1;
    }

    // Echo will work even if no messages were received.
    strcpy(last_rx_data, FIRST_MSG_FROM_BOARD);

    last_tx_params.ssi = SSI_PLAIN_TEXT;
    last_tx_params.ck_nonce = NULL;
    last_tx_params.data_cfg_range = TRILL_DATA_CFG_RANGE_FAR;

    return 0;
}

static int stop_sdk_tasks(void)
{
    int ret;

    if (tx_audio_enabled)
    {
        ret = trill_tx_abort(trill_handle);
        if (ret < 0)
            return ret;
    }

    // Stop/Wait order is important else tasks may block indefinitely.
    printf("Waiting for trill task to stop\n");
    xEventGroupSetBits(eg_sdk_tasks_ctrl, EG_SDK_TRILL_TASK_REQ_BIT);
    xEventGroupWaitBits(
        eg_sdk_tasks_ctrl,
        EG_SDK_TRILL_TASK_STOP_BIT,
        pdTRUE,
        pdTRUE,
        portMAX_DELAY);

    printf("Waiting for feed task to stop\n");
    xEventGroupSetBits(eg_sdk_tasks_ctrl, EG_SDK_FEED_TASK_REQ_BIT);
    xEventGroupWaitBits(
        eg_sdk_tasks_ctrl,
        EG_SDK_FEED_TASK_STOP_BIT,
        pdTRUE,
        pdTRUE,
        portMAX_DELAY);

    printf("Waiting for play task to stop\n");
    xEventGroupSetBits(eg_sdk_tasks_ctrl, EG_SDK_PLAY_TASK_REQ_BIT);
    xEventGroupWaitBits(
        eg_sdk_tasks_ctrl,
        EG_SDK_PLAY_TASK_STOP_BIT,
        pdTRUE,
        pdTRUE,
        portMAX_DELAY);

    printf("Before trill_deinit\n");
    print_mem_free_info();
    ret = trill_deinit(trill_handle);
    printf("After trill_deinit = %d\n", ret);
    print_mem_free_info();

    trill_handle = NULL;

    return ret;
}

static int do_init_trill(void)
{
    int ret;
    
    printf("Using License file: %s\n", TRILLBIT_LICENSE_PATH);
    
    FILE* fp = fopen(TRILLBIT_LICENSE_PATH, "r");
    if (fp == NULL)
    {
        printf("License file not found.\n");
        return TRILL_ERR_INVALID_LICENSE_DATA;
    }

    fseek(fp, 0L, SEEK_END);
    size_t length = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    char* lic_buf = malloc(length);

    if (!lic_buf)
    {
        printf("Failed to allocate license buffer. Its length %u\n", length);
        ret = -1;
        goto err;
    }

    if ((ret = fread(lic_buf, length, 1, fp)) != 1)
    {
        printf("Failed to read license file: %d %u\n", ret, length);
        ret = -1;
        goto err;
    }

    printf("Loaded License: %s\n", lic_buf);
	
    trill_init_opts.n_rx_channels = N_MICS_ON_BOARD;
    
    /*
        Bits     7 6 5 4 3 2 1 0
        Channels 7 6 5 4 3 2 1 0
        Enabled  0 0 0 0 0 0 0 1
    */
    trill_init_opts.rx_channels_en_bm = 1; 
    trill_init_opts.aud_buf_rx_block_size_bytes = INPUT_SAMPLES_BLOCK_SIZE;
	trill_init_opts.aud_buf_rx_n_blocks = 20;
	trill_init_opts.aud_buf_rx_notify_cb = NULL;

	trill_init_opts.aud_buf_tx_block_size_bytes = OUTPUT_SAMPLES_BUFFER_SIZE / I2S_CHANNEL_NUM;
	trill_init_opts.aud_buf_tx_n_blocks = 2;
	trill_init_opts.aud_buf_tx_notify_cb = NULL;

	trill_init_opts.audio_tx_enable_fn = board_audio_tx_enable_cb;
	trill_init_opts.data_link_cb = data_link_evt_handler;

	trill_init_opts.b64_ck = NULL;
	trill_init_opts.b64_ck_nonce = NULL;
	trill_init_opts.b64_license = lic_buf;

	trill_init_opts.mem_alloc_fn = tsdk_mem_alloc;
    trill_init_opts.mem_aligned_alloc_fn = tsdk_mem_aligned_alloc;
	trill_init_opts.mem_free_fn = tsdk_mem_free;
	trill_init_opts.logger_fn = log_print;
	trill_init_opts.timer_get_fn = timer_us;

    printf("Before trill init\n");
    print_mem_free_info();
    ret = trill_init(&trill_init_opts, &trill_handle);
    printf("After trill_init = %d\n", ret);
    print_mem_free_info();
    
err:

    free(lic_buf);
    fclose(fp);
    return ret;
}

void app_main()
{
    int ret;
    const char* device_id = "";
    
    printf("Trillbit SDK Demo v%s\n", APP_VERSION);

    ESP_ERROR_CHECK(bsp_board_init());
    ESP_ERROR_CHECK(bsp_board_power_ctrl(POWER_MODULE_AUDIO, true));
    ESP_ERROR_CHECK(lv_port_init());
    bsp_lcd_set_backlight(true);  // Turn on the backlight after gui initialize

    tx_audio_enabled = 0;

    storage_init();

    //Initialize NVS
    esp_err_t esp_ret = nvs_flash_init();
    if (esp_ret == ESP_ERR_NVS_NO_FREE_PAGES || esp_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      esp_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_ret);
    
    trill_get_device_id(&device_id);

    printf("MCU/Device ID: %s\n", device_id);

    ret = do_init_trill();
    if (ret < 0)
    {
        if ((ret == TRILL_ERR_INVALID_LICENSE_DATA) ||
            (ret == TRILL_ERR_LICENSE_NOT_FOR_THIS_DEVICE))
        {
            printf("Starting serial communication.\n");
            provision_license();
        }
        return;
    }

    input_samples = heap_caps_malloc(INPUT_SAMPLES_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
    assert(input_samples);
    output_samples = heap_caps_malloc(OUTPUT_SAMPLES_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
    assert(output_samples);

    eg_sdk_tasks_ctrl = xEventGroupCreate();
    assert(eg_sdk_tasks_ctrl);

    ret = ui_start();
    if (ret < 0)
    {
        printf("ui_start failed: %d\n", ret);
    }
    else
    {
        ui_set_echo_callback(send_callback);
        ui_en_echo(1);
    }

    ret = start_sdk_tasks();
    if (ret < 0)
    {
        return;
    }

    ui_set_start_callback(sdk_on_off);

    do {
        lv_task_handler();
    } while (vTaskDelay(1), true);
}


void provision_license(void)
{
    int ret;
    const char* dev_id;

    trill_get_device_id(&dev_id);

    ui_lic_error(dev_id);

    lv_task_handler();

    ret = ser_cmd_init();
    if (ret < 0)
    {
        printf("ser_cmd_init failed: %d\n", ret);
        return;
    }

    do {
        lv_task_handler();
    } while (vTaskDelay(1), true);
}
