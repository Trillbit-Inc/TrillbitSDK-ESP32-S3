#ifndef _SERIAL_COM_H_
#define _SERIAL_COM_H_

#define SER_CMD_ERROR_CODE_BASE		-1000

#define SER_CMD_CMD_WAIT_TIMEOUT	1000
#define SER_CMD_DATA_WAIT_TIMEOUT	3000

#define SER_CMD_MAX_DATA_BUFFER_SIZE	256

#define SER_CMD_FIELD_SEPARATOR " "
#define SER_CMD_RESP_PREFIX		"\ntbr"

typedef enum
{
	SER_CMD_GET_VERSION=1,
    SER_CMD_GET_MCU_ID,
	SER_CMD_LIC_PART,
	SER_CMD_LIC_SET,
    SER_CMD_LIC_SHORT=6,
} serial_command_t;

typedef enum
{
	SER_CMD_ERROR_CODE_INVALID_DATA_LEN=SER_CMD_ERROR_CODE_BASE,
	SER_CMD_ERROR_CODE_FAILED_TO_READ_DATA,
	SER_CMD_ERROR_CODE_UNKNOWN_COMMAND,
	SER_CMD_ERROR_CODE_INVALID_CMD_FORMAT,
	SER_CMD_ERROR_CODE_CHECKSUM_MISMATCH,
	SER_CMD_ERROR_CODE_OS_ERROR,
	SER_CMD_ERROR_CODE_DATA_TOO_LONG,
	SER_CMD_ERROR_CODE_APPLY_KEYS_FAILED,
} serial_command_errors_t;

int ser_cmd_init(void);

#endif //_SERIAL_COM_H_