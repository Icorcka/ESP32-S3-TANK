# ESP32-S3-TANK — ESP32-S3 toy tank on ESP-IDF + FreeRTOS

Firmware for a toy tank: a tracked drivetrain on a **TB6612FNG** driver and a cannon
whose button is "pressed" by an **ST2222A** transistor. Framework — **ESP-IDF** with FreeRTOS,
built with **PlatformIO**.

## Build and flash

```bash
pio run -t upload        # build and flash the ESP32-S3
pio device monitor       # debug log (115200)
pio run -t menuconfig    # (optional) all ESP-IDF settings
```

The first build is slow (a few minutes): PlatformIO compiles the whole ESP-IDF.
