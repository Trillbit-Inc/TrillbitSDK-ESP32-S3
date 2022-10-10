import time

SER_CMD_GET_VERSION = 1
SER_CMD_GET_DEVICE_ID = 2
SER_CMD_LICENSE_START = 3
SER_CMD_LICENSE_PART = 4
SER_CMD_LICENSE_END = 5
SER_CMD_LICENSE_SHORT = 6

RESPONSE_PREFIX = "tbr"
COMMAND_PREFIX = "tbc "

RESPONSE_TIMEOUT_SECS = 2
MAX_ATTEMPTS = 3
REPORT_ATTEMPT_ON_COUNT = 1

MAX_PART_SIZE = 240

def flush(sio):
    send_string(sio, "\n\n")

def get_serial_id(sio):
    cmd = "{}{}\n".format(COMMAND_PREFIX, SER_CMD_GET_DEVICE_ID)
    attempt = 0
    id = None
    while attempt < MAX_ATTEMPTS:
        attempt += 1
        if attempt > REPORT_ATTEMPT_ON_COUNT:
            print("Attempt", attempt, "Sending command:", cmd)
    
        send_string(sio, cmd)
        fields = _get_response_fields(sio)
        if not fields or len(fields) < 2:
            continue
        
        err_code = int(fields[1])
        if (err_code < 0) or (len(fields) != 4):
            print("Failed to get device id:", err_code)
            continue

        id = fields[2]
        r_sum = int(fields[3])
        sum = _calc_checksum(id.encode('utf-8'))
        if sum != r_sum:
            print("checksum verification failed for received device id:", id, r_sum, sum)
            continue
        break
    
    return id

def send_string(sio, s):
    sio.write(s)
    sio.flush()

def _get_response_fields(sio):
    start = time.time()
    while (time.time() - start) < RESPONSE_TIMEOUT_SECS:
        try:
            line = sio.readline()
            if not line.startswith(RESPONSE_PREFIX):
                print("Ignoring response:", line)
                continue
            return line.split()
        except:
            return None
    return None

def _calc_checksum(data_bytes):
    sum = 0
    for b in data_bytes:
        sum += b
    return sum & 0xff

def send_single_data(sio, cmd_code, data_bytes):

    checksum = _calc_checksum(data_bytes)
    cmd = "{}{} {} {}\n".format(
            COMMAND_PREFIX,
            cmd_code,
            data_bytes.decode('utf-8'),
            checksum)
    
    attempt = 0
    while attempt < MAX_ATTEMPTS:
        attempt += 1
        if attempt > REPORT_ATTEMPT_ON_COUNT:
            print("Attempt", attempt, "Sending command:", cmd)
        send_string(sio, cmd)
        fields = _get_response_fields(sio)
        if not fields or len(fields) < 2:
            continue
        err_code = int(fields[1])
        if err_code < 0:
            print("Failed to set data:", err_code)
            flush(sio)
            continue
        #print(fields)
        return True
    return False

def set_license(sio, data):
    return send_single_data(sio, SER_CMD_LICENSE_SHORT, data)