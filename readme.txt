low power sensor demo:

1.put these file to sdk root path
2.compile: support flash.bin+rom.bin and bootv1.1.bin+user.bin
3.download: 
eagle.flash_disable_rf.bin -> 0x0
eagle.irom0text.bin -> 0x40000
esp_init_data_vdd33.bin -> 0x7c000  
blank.bin -> 0x7e000
device_master_key -> 0x3e000

or:
user1.512.old_disable_rf.bin ->0x1000
bootv1.1.bin -> 0x0
blank.bin -> 0x7e000
device_master_key -> 0x3e000
esp_init_data_vdd33.bin -> 0x7c000


