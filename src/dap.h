#pragma once

#include <stdint.h>
#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif
void dap_init(void);
void dap_reset(uint8_t rhport);
uint16_t dap_open(uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t max_len);
bool dap_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool dap_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
#ifdef __cplusplus
};
#endif
