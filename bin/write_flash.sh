python ../tools/add_disable_rf_cmd.py eagle.flash.bin
../tools/esptool.py --port /dev/tty.usbserial-AL00ALCV write_flash \
0x00000   eagle.flash_disable_rf.bin   \
0x40000   eagle.irom0text.bin          \
0x7c000   esp_init_data_vdd33.bin  \
0x7e000   blank.bin                    \
0x3e000   key.bin
