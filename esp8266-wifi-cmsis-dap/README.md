# esp8266-wifi-cmsis-dap

CMSIS-DAP USB-SWD/JTAG HID adapter firmware for ESP8266 boards

Copyright (C) 2017 Oleg Suchilov

based on <a href="https://github.com/myelin/arduino-cmsis-dap">CMSIS-DAP USB-SWD/JTAG adapter firmware for Pro Micro and Teensy 3.2 boards</a>

credits to:
- https://github.com/devanlai/dap42 for HID descriptor config
- https://github.com/lcgamboa/USBIP-Virtual-USB-Device for simple USB/IP implementation details
- https://github.com/torvalds/linux/tree/master/tools/usb/usbip USB/IP stack authors


Copyright (C) 2016 Phillip Pearson <pp@myelin.co.nz>

based on the <a href="https://github.com/mbedmicro/CMSIS-DAP">CMSIS-DAP Interface Firmware</a>

Copyright (c) 2009-2013 ARM Limited

This is a port of the core of ARM's CMSIS-DAP firmware to the Arduino environment,
which lets you turn a esp8266 into a wireless
CMSIS-DAP USB adapter, which you can use with OpenOCD and mbed to program and debug ARM chips using the SWD protocol,
and also with OpenOCD to program and debug various chips using JTAG.





# Usage:

example of flashing stm32f103 on arch linux

Using Arduino IDE:
- change SSID and WiFi password in .ino file
- compile and upload it to esp8266

connect target device to your esp8266 via swd (check the pinout in .ino file)

attach usb/ip device:
```
sudo modprobe usbip-core
sudo modprobe vhci-hcd
sudo usbip attach -r <esp8266_ip_address> -b 1-1
```

start openocd:
```
sudo openocd -f /usr/share/openocd/scripts/interface/cmsis-dap.cfg -f /usr/share/openocd/scripts/target/stm32f1x.cfg
```

connect to openocd telnet interface from another terminal:
```
telnet localhost 4444
```

execute openocd commands:

(may require to push the RST button on target for it to work)
```
reset halt
flash write_image erase /path/to/your/firmware.bin 0x08000000
```

to detach a virtual usb device run:
```
sudo usbip detach -p 0
```

