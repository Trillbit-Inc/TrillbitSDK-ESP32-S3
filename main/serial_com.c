#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

#include "trill.h"
#include "app_config.h"
#include "serial_com.h"


static const char* TAG = "serial_com";
static char lic_buffer[TRILL_MAX_LICENSE_STRING_SIZE+1];

static int rx_error(serial_command_errors_t code);
static int save_license_buf(void);

static int calc_check_sum_buf(const char* buf, int len)
{
    // 8-bit sum.
    unsigned char sum = 0;
    
    for (int i = 0; i < len; i++)
    {
        sum += buf[i];
    }

    return (int) sum;
}


/*
* command format: tbc<space><cmd-code><space><params...><LF>
*
* response format: tbr<space><error-code><space><data...><LF>
*
*/

static int ser_cmd_handler(int argc, char **argv)
{
    /**
     * argv index
     * 0: entire command line
     * 1: command code
     * 2: command args
	 * ...
     */

    int ret;
	
    if (argc < 2)
	{
	    return rx_error(SER_CMD_ERROR_CODE_INVALID_CMD_FORMAT);
    }

    int cmd = atoi(argv[1]);
    

	switch(cmd)
	{
		case SER_CMD_GET_VERSION:
			printf("%s %s\n", SER_CMD_RESP_PREFIX, TRILL_SDK_VERSION);
			break;
		case SER_CMD_GET_MCU_ID:
			{
				const char* id;
				trill_get_device_id(&id);
				int device_id_len = strlen(id);
				int sum = calc_check_sum_buf(id, device_id_len);
				printf("%s 0 %s %d\n",
						SER_CMD_RESP_PREFIX,
						id,
						sum);
			}
			break;
		case SER_CMD_LIC_PART:
			/* tbc cmd offset b64 checksum*/
			if (argc < 5)
			{
				printf("actual args: %d\n", argc);
				return rx_error(SER_CMD_ERROR_CODE_INVALID_CMD_FORMAT);
			}
			else
			{
				unsigned int offset = atoi(argv[2]);
				unsigned int part_size = strlen(argv[3]);
				int given_sum = atoi(argv[4]);
				
				if ((offset + part_size) >= TRILL_MAX_LICENSE_STRING_SIZE)
				{
					return rx_error(SER_CMD_ERROR_CODE_DATA_TOO_LONG);
				}

				int sum = calc_check_sum_buf(argv[3], part_size);

				if (sum != given_sum)
				{
					return rx_error(SER_CMD_ERROR_CODE_CHECKSUM_MISMATCH);
				}

				strcpy(&lic_buffer[offset], argv[3]);
				rx_error(0);
			}
			break;
		case SER_CMD_LIC_SET:
			/* tbc cmd total-checksum */
			if (argc < 3)
			{
				return rx_error(SER_CMD_ERROR_CODE_INVALID_CMD_FORMAT);
			}
			else
			{
				int given_sum = atoi(argv[2]);
				int sum = calc_check_sum_buf(lic_buffer, strlen(lic_buffer));
				if (sum != given_sum)
				{
					return rx_error(SER_CMD_ERROR_CODE_CHECKSUM_MISMATCH);
				}

				rx_error(0);

				// save license 
				ret = save_license_buf();
				if (!ret)
				{
					// reboot.
					esp_restart();
				}
			}
			break;
		default:
			printf("%s %d\n", SER_CMD_RESP_PREFIX, SER_CMD_ERROR_CODE_UNKNOWN_COMMAND);
			break;
	}

	return 0;
}

int ser_cmd_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_usb_serial_jtag_config_t uart_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();//ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = CONFIG_IDF_TARGET ">";
    repl_config.max_cmdline_length = 512;

    esp_console_register_help_command();

    const esp_console_cmd_t trill_cmd = {
        .command = "tbc",
        .help = "Provisioning commands",
        .hint = NULL,
        .func = &ser_cmd_handler,
        .argtable = NULL,
    };

	linenoiseSetDumbMode(1);

    ESP_ERROR_CHECK( esp_console_cmd_register(&trill_cmd) );

    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&uart_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

	

    return 0;
}

static int rx_error(serial_command_errors_t code)
{
	printf("%s %d\n", SER_CMD_RESP_PREFIX, code);

	return 0;
}

static int save_license_buf(void)
{
	FILE* fp = fopen(TRILLBIT_LICENSE_PATH, "w");
    if (fp == NULL)
    {
        printf("Failed to create License file.\n");
        return -1;
    }

	size_t nitems = fwrite(lic_buffer, 
						strlen(lic_buffer)+1, //include null character.
						1,
           				fp);
	if (nitems != 1)
	{
		printf("Failed to write License file.\n");
        return -1;
	}

	fclose(fp);

	return 0;
}
