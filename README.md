the goal of this project is to be able to debug an rpi (or other SBC) over wifi or usb

in usb mode, it presents itself as a usb composite device with 3 interfaces, a CDC uart for the target (gpio0/1 of the pico), a CDC uart for the pico itself, and a CMSIS-DAP device

in wifi mode, the target uart is presented over a tcp port, jtag over wifi is not yet implemented

example configs:

```
$ cat wifi-jtag.cfg
adapter driver cmsis-dap
transport select jtag
cmsis-dap vid_pid 0x2e8a 0x0009
cmsis-dap backend usb_bulk

$ cat pi2.cfg
source wifi-jtag.cfg
adapter serial AA44B7974F7DB253
source [find target/bcm2836.cfg]

$ result/bin/openocd -f pi2.cfg
```

the serial# line is optional, and is only needed if you have multiple wifi-jtag pico's attached at once

in theory, it should work on any JTAG capable SBC, but its only been tested on raspberry pi's so far

SWD mode is planned as well, that will enable targeting pico's and the pi5
