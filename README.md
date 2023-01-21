| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |


# Application

This project contains code in C for the ESP32 microcontroller that implements a TFTP protocol server with WIFI support.

## Getting Started

First compile the code using ESP-IDF toolchain and upload it to the device.

At first boot ESP will set up Access Point - named {ESP_NAME} (ESP_NAME is a configurable name of your device "Ganymede" as default).

Connect to the Access Point and send WRQ to address 192.168.4.1 for file NETWORK_CREDENTIAL.KUB.

When the device sends ACK back, send DATA packet containing SSID\x00PASSOWRD\x00.

The device should now attempt to connect to specified newtwork and be ready for use. Also, correct credentals will be saved to flash so that when device boots again, it automatically connects to remembered network.

If the connection is unsuccessful, the Access Point will be started again.