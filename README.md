# ESP32-C6 ZHA 4-Button Zigbee Switch

- Fully compatible with Home Assistant ZHA
- 4 buttons, each supports single, double, and long press
- End Device (battery-friendly)
- 4MB flash layout, ready to flash with `esptool.py`
- Precompiled firmware is built automatically via GitHub Actions

## Flashing

```bash
esptool.py --chip esp32c6 --port /dev/ttyUSB0 write_flash -z \
0x0 bootloader.bin \
0x8000 partition-table.bin \
0x20000 zigbee_4btn_switch.bin
