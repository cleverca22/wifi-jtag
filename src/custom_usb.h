#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
  bool usb_init();
  extern char usbd_serial_str[];
#ifdef __cplusplus
};
#endif
