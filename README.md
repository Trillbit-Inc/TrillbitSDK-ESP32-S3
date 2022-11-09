Trillbit SDK Demo on ESP32-S3-Box demonstrates how to use Trillbit SDK APIs. The demo also uses ESP-IDF and LVGL libraries.

# Setup Build Environment
## Software Requirements
- ESP-IDF: Trillbit SDK and Demo application uses v4.4 release of esp-idf, its repo can be found [here](https://github.com/espressif/esp-idf/tree/release/v4.4).
- ESP-DSP: Trillbit SDK library depends on esp-dsp, its repo and commit used can be found [here](https://github.com/espressif/esp-dsp/tree/8ec1402467a20b81dffedde30194e826419fe263).
- ESP-Box: Trillbit has modified the BSP layer of esp-box library to support 48kHz sample rate instead of the default 16kHz rate. Modified repo can be found [here](https://github.com/Trillbit-Inc/Trillbit-esp-box).
  

## Flashing from Pre-built Binary - Windows

- Download the ESP32 flash tool from [here](https://www.espressif.com/sites/default/files/tools/flash_download_tool_3.9.3.zip)
- Pre-built demo binary is located in repo at *pre_built_bins\trillbit_sdk_demo_v1.0.bin*
- Run the flash tool and select start up settings as shown below

![Flash tool settings](images/esp32s3_flash_tool_1.jpg)

- Browse for the downloaded pre-built binary file and select SPI flash download settings as shown below
  
![SPI flash download settings](images/esp32s3_flash_tool_2.jpg)

- Click *Start*. Once the download progress shows *Finish* status, reboot s3-box by pressing the *Reboot* switch (below USB connector).

## Flashing from Pre-built Binary - macOS and Linux

- python3 must be installed 

- Connect your development board to the computer through a USB Type-C cable. 

- Install esptool by entering the following command in Terminal (pip3 can be specified as pip):

  - ``` pip3 install esptool ```

- Upload the binary using the correct path

  - ``` python3 -m esptool --chip esp32s3 write_flash 0x0 download_path/test_bin.bin ```

4. Press reset button to test the firmware.

# Licensing
License for the Trillbit SDK can either be downloaded from developer portal and stored in [spiffs](spiffs/README.md) partition or can be acquired over USB by running the License provisioning script.

## License over USB
### Setup Python Environment

Ensure you have python3 installed. Plug the USB-C cable to S3-Box and your PC. On power up you will see license missing message on the display of s3-box.

```
$ cd ~/trillbit
$ python -m venv py_env
$ . py_env/bin/activate
$ pip install pyserial
```

```
$ cd scripts/license
$ python3 license_device_script.py
```

Follow the instructions given by the script.

# Using the Demo

On successful licensing, you would see the following SDK demo display

![Demo main display](images/esp32s3_demo_1.jpg)

- Use the Mobile app to send data over sound.
- Received message would be displayed on the screen. Number prefixing the message increments for each received message.
- Tap *Echo* button to send back the last received message. If no last message is present then a default message would be sent.
- Tap *Stop* to stop and de-initialize the SDK.
- If stopped, tap *Start* to re-initialize the SDK to start receiving messages again.
- You can also view the console messages over USB/UART. Use the *monitor* tool of *idf*

  ```
  $ cd ~/trillbit
  $ . esp-idf/export.sh
  $ cd mtfsk-esp32-s3-box
  $ idf.py -p </dev/tty?> monitor
  ```
