#include "custom_usb.h"
#include "dap.h"
#include "device/usbd_pvt.h"
#include "jtag.h"
#include "main.h"
#include "pico/stdlib.h"

// based heavily on https://github.com/raspberrypi/debugprobe
// and also src/jtag/drivers/cmsis_dap.c from openocd

#define CMD_DAP_INFO            0x00
#define CMD_DAP_LED             0x01
#define CMD_DAP_CONNECT         0x02
#define CMD_DAP_DISCONNECT      0x03
#define CMD_DAP_TFER_CONFIGURE  0x04
#define CMD_DAP_SWJ_PINS        0x10
#define CMD_DAP_SWJ_CLOCK       0x11
#define CMD_DAP_SWJ_SEQ         0x12
#define CMD_DAP_JTAG_SEQ        0x14

#define DAP_OK            0
#define DAP_ERROR         0xff

#define INFO_ID_SERNUM    0x03  // string
#define INFO_ID_FW_VER    0x04  // string
#define INFO_ID_CAPS      0xf0  // 8 or 16bit int
#define INFO_ID_PKT_CNT   0xfe  // 8bit int
#define INFO_ID_PKT_SZ    0xff  // 16bit int

#define CONNECT_JTAG      0x02

#define DAP_PACKET_SIZE   64

static uint8_t out_buffer[DAP_PACKET_SIZE];
static uint8_t in_buffer[DAP_PACKET_SIZE];

static uint8_t in_ep;
static uint8_t out_ep;

static int jtag_sequence(uint8_t *request) {
  int seq_count = *request++;
  //printf("  count %d\n", seq_count);
  int byte_ptr = 2;
  int bit_ptr = 0;
  in_buffer[byte_ptr] = 0;
  uint8_t byte_buf;
  int seq_info;
  while (seq_count--) {
    seq_info = *request++;
    int len = seq_info & 0x3f;
    int in_bit_ptr = 0;
    bit_ptr = 0;
    if (len == 0) len = 64;
    //printf("  info 0x%x %d\n", seq_info, len);
    //for (int i=0; i < ( (len+7) / 8); i++) {
    //  printf("    0x%x\n", *request++);
    //}
    for (int i=0; i<len; i++) {
      if (in_bit_ptr == 0) {
        byte_buf = *request++;
        //printf("    bit %d, byte 0x%x\n", i, byte_buf);
      }
      int flags = 0;
      if (seq_info & 0x40) {
        flags |= TMS;
        //puts("TMS HI");
      } else {
        //puts("TMS LO");
      }
      if (byte_buf & (1<<in_bit_ptr)) flags |= TDI;

      in_bit_ptr++;

      if (in_bit_ptr >= 8) {
        in_bit_ptr = 0;
      }

      if (seq_info & 0x80) {
        in_buffer[byte_ptr] |= (gpio_get(tdo) << bit_ptr);
        bit_ptr++;
        if (bit_ptr >= 8) {
          //printf("bit wrap, captured 0x%x in slot %d\n", in_buffer[byte_ptr], byte_ptr);
          byte_ptr++;
          bit_ptr = 0;
          in_buffer[byte_ptr] = 0;
        }
      }
      tck_shift(flags);
    }

    if ((bit_ptr + 7) >= 8) {
      byte_ptr++;
      in_buffer[byte_ptr] = 0;
    }
  }
  #if 0
  if (seq_info & 0x80) {
    printf("bit %d byte %d, adding one more bit\n", bit_ptr, byte_ptr);
    in_buffer[byte_ptr] |= (gpio_get(tdo) << bit_ptr);
    bit_ptr++;
    if (bit_ptr >= 8) {
      byte_ptr++;
      bit_ptr = 0;
      in_buffer[byte_ptr] = 0;
    }
  }
  #endif
  in_buffer[1] = DAP_OK;

  //printf("bit %d byte %d\n", bit_ptr, byte_ptr);

  bit_ptr += 7; // for round-up
  if (bit_ptr >= 8) byte_ptr++;
  return byte_ptr;
}

void dap_init(void) {
  // called during tud_init()
}

void dap_reset(uint8_t rhport) {
  // called if SetConfiguration changes, to de-init the old config, only if the old-config is non-zero
}

uint16_t dap_open(uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t max_len) {
  // called after SetConfiguration, to init the new config
  // tinyusb will also parse the interface descriptors, and bind the endpoints to this driver
  // must return the number of bytes consumed from the configuration descriptor stream (interface+endpoints)

  if (desc_intf->bInterfaceNumber != 4) return 0;

  uint16_t const drv_len = sizeof(tusb_desc_interface_t) + (desc_intf->bNumEndpoints * sizeof(tusb_desc_endpoint_t));

  tusb_desc_endpoint_t *edpt_desc = (tusb_desc_endpoint_t *) (desc_intf + 1);
  uint8_t ep_addr = edpt_desc->bEndpointAddress;
  out_ep = ep_addr;

  if (!usbd_edpt_open(rhport, edpt_desc)) return 0;
  if (!usbd_edpt_xfer(rhport, ep_addr, out_buffer, DAP_PACKET_SIZE)) return 0;

  edpt_desc++;
  ep_addr = edpt_desc->bEndpointAddress;
  in_ep = ep_addr;

  if (!usbd_edpt_open(rhport, edpt_desc)) return 0;


  return drv_len;
}

bool dap_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  //printf("dap_xfer_cb(%d, 0x%x, %d, %ld)\n", rhport, ep_addr, result, xferred_bytes);

  if (ep_addr == out_ep) {
    int reply_len = 0;

    switch (out_buffer[0]) {
    case CMD_DAP_INFO:
      //printf("CMD_INFO 0x%x\n", out_buffer[1]);
      switch (out_buffer[1]) {
      case INFO_ID_SERNUM: {
        in_buffer[1] = strlen(usbd_serial_str);
        strcpy((char*)&in_buffer[2], usbd_serial_str);
        reply_len = 2 + in_buffer[1];
        break;
      }
      case INFO_ID_FW_VER:
      {
        const char *version = "wifi-jtag (usb mode)";
        in_buffer[1] = strlen(version);
        strcpy((char*)&in_buffer[2], version);
        reply_len = 2 + in_buffer[1];
        break;
      }
      case INFO_ID_CAPS:
        in_buffer[1] = 1;
        in_buffer[2] = (1<<1);
        reply_len = 3;
        break;
      case INFO_ID_PKT_CNT:
        in_buffer[1] = 1;
        in_buffer[2] = 1;
        reply_len = 3;
        break;
      case INFO_ID_PKT_SZ:
        in_buffer[1] = 2;
        in_buffer[2] = 64;
        in_buffer[3] = 0;
        reply_len = 4;
        break;
      default:
        printf("unhandled CMD_INFO\n");
        in_buffer[0] = DAP_ERROR;
        reply_len = 1;
        break;
      }
      break;
    case CMD_DAP_LED:
      //printf("CMD_DAP_LED %d %d\n", out_buffer[1], out_buffer[2]);
      in_buffer[1] = DAP_OK;
      reply_len = 2;

      if (out_buffer[1] == 0) led_set(out_buffer[2]);
      break;
    case CMD_DAP_CONNECT:
      printf("connect, mode=%d\n", out_buffer[1]);
      in_buffer[1] = CONNECT_JTAG;
      reply_len = 2;

      led_idle_blink = false;
      break;
    case CMD_DAP_DISCONNECT:
      in_buffer[1] = DAP_OK;
      reply_len = 2;

      led_idle_blink = true;
      break;
    case CMD_DAP_TFER_CONFIGURE:
      in_buffer[1] = DAP_OK;
      reply_len = 2;
      break;
    case CMD_DAP_SWJ_PINS:
      printf("CMD_DAP_SWJ_PINS pins=%d, mask=%d, delay=?\n", out_buffer[1], out_buffer[2]);
      in_buffer[1] = DAP_OK;
      reply_len = 2;
      break;
    case CMD_DAP_SWJ_CLOCK:
      puts("CMD_DAP_SWJ_CLOCK");
      in_buffer[1] = DAP_OK;
      reply_len = 2;
      break;
    case CMD_DAP_SWJ_SEQ:
      printf("CMD_DAP_SWJ_SEQ %d %ld\n", out_buffer[1], xferred_bytes - 2);
      for (int i=0; i< ( (out_buffer[1]+7) /8); i++) {
        printf("  0x%x\n", out_buffer[i+2]);
      }
      for (int i=0; i<out_buffer[1]; i++) {
        int byte = i/8;
        int bit = i%8;
        //printf("bit %d byte %d\n", bit, byte);
        if (out_buffer[2+byte] & (1<<bit)) tck_shift(TMS);
        else tck_shift(0);
      }
      in_buffer[1] = DAP_OK;
      reply_len = 2;
      break;
    case CMD_DAP_JTAG_SEQ:
      //puts("CMD_DAP_JTAG_SEQ");
      reply_len = jtag_sequence(out_buffer + 1);
      //printf("cmd 0x%x, status %d, len %d\n", out_buffer[0], in_buffer[1], reply_len);
      break;
    default:
      printf("unhandled command 0x%x with %ld bytes\n", out_buffer[0], xferred_bytes);
      in_buffer[0] = DAP_ERROR;
      reply_len = 1;
      break;
    }

    if (reply_len) {
      in_buffer[0] = out_buffer[0];
      usbd_edpt_xfer(0, in_ep, in_buffer, reply_len);
    }

    // get ready for the next OUT
    usbd_edpt_xfer(rhport, ep_addr, out_buffer, DAP_PACKET_SIZE);
  }
  return true;
}

bool dap_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
  puts("control xfer");
  led_toggle();
  return true;
}
