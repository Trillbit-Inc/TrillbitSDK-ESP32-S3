import serial.tools.list_ports
import re
import sys
import io
import json
import requests

import sdk_com_commands

print("Command line configuraion will override serial config mode.")
print("Args(1): <path to client_secret_json_file>")
print("Args(2): <path to client_secret_json_file> <device id> ")


def get_device_license(device_id, client_secret_json):
    # reading client credentials from json

    try:
        credentials = client_secret_json["credentials"]
        platform_key = credentials["platform_key"]
        license_uri = credentials["license_uri"]
    except Exception as e:
        print(e)
        print("Error :: Invalid credentials file. Please download again.")
        sys.exit(-1)

    # device_platform, platform_version, device_model are optional parameters that may or may not be present
    # if not present, just use empty string
    payload = {
        "platform_key": platform_key,
        "device_id": device_id,
        "device_model": "",
        "device_platform": "",
        "platform_version": "",
    }

    response = requests.request("POST", license_uri, data=payload)
    response_json = json.loads(response.text)
    if response.status_code != 200:
        error_message = response_json.get("message", "")
        print(f"Error :: licensing device failed. {error_message}")
        sys.exit(-1)

    user_lic = response_json["payload"]["license"]
    user_lic_bytes = user_lic.encode('utf-8')
    return user_lic_bytes


if len(sys.argv) == 3:
    # Args: <path to client_secret_json_file> <device id>
    device_id = sys.argv[2]
    with open(sys.argv[1], 'r') as f:
        client_secret_json = json.load(f)
    get_device_license(device_id, client_secret_json)
    sys.exit(0)
elif len(sys.argv) == 2:
    # Args: <path to client_secret_json_file>
    # get device_id from serial port
    with open(sys.argv[1], 'r') as f:
        client_secret_json = json.load(f)
else:
    print("Error: invalid arguments")
    sys.exit(-1)


guessed_port_info = ""
count = 0
detected_ports = []
print()
print("Detected Serial Ports:")
try:
    for info in serial.tools.list_ports.comports():
        desc = info[1]
        detected_ports.append(info[0])
        if re.match(".*EDBG.*", info[1], re.IGNORECASE):
            guessed_port_info = info
            desc += " - Guessed"
        count += 1
        print("{}) {} - {}".format(count, info[0], desc))
except Exception as e:
    print("Error: ", e)
    sys.exit(-1)

if count == 0:
    print("No serial ports found.")
    sys.exit(-1)

print()
print("To select a serial port to open, enter its entry number.")
if guessed_port_info:
    print("To open guessed port ({}) just press Enter.".format(
        guessed_port_info[0]))
try:
    choice = input("choice: ")
    if guessed_port_info and choice == "":
        com_port = guessed_port_info[0]
    else:
        choice = int(choice)
        if (choice < 1) or (choice > len(detected_ports)):
            raise ValueError
        choice -= 1
        com_port = detected_ports[choice]
except ValueError:
    print("Invalid choice.")
    sys.exit(-1)

print()
print("Opening", com_port)

try:
    with serial.Serial(com_port, 115200, timeout=1) as ser:
        sio = io.TextIOWrapper(io.BufferedRWPair(ser, ser))
        sdk_com_commands.flush(sio)
        id = sdk_com_commands.get_serial_id(sio)
        if id:
            print("Device id:", id)
            lic_bytes = get_device_license(id, client_secret_json)

            #print("License ({}): {}".format(len(b64_license), b64_license))
            print("Sending License")
            ok = sdk_com_commands.set_license(sio, lic_bytes)
            if ok:
                print("Done.")
            else:
                print("Failed to set license")
except KeyboardInterrupt:
    print("Aborting.")
except Exception as e:
    print("Error:", e)

print(com_port, "closed.")