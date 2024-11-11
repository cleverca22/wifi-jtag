#include "custom_usb.h"
#include "dap.h"
#include "device/usbd_pvt.h"
#include "hardware/irq.h"
#include "main.h"
#include "pico/bootrom.h"
#include "pico/mutex.h"
#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/unique_id.h"
#include "pico/usb_reset_interface.h"
#include "tusb.h"
#include <string.h>

// based on pico-sdk pico_stdio_usb

#define USBD_STR_0 (0x00)
#define USBD_STR_MANUF (0x01)
#define USBD_STR_PRODUCT (0x02)
#define USBD_STR_SERIAL (0x03)
#define USBD_STR_CDC0 (0x04)
#define USBD_STR_CDC1 (0x05)
// #define USBD_STR_RPI_RESET (0x05)
#define USBD_STR_CMIS_DAP 6

#define USBD_ITF_CDC0      (0)
#define USBD_ITF_CDC1      (2)
// #define USBD_ITF_RPI_RESET (2)
#define USBD_ITF_CMIS_DAP  4
#define USBD_ITF_MAX       (5)

#define TUD_RPI_RESET_DESC_LEN  9
#define USBD_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_CDC_DESC_LEN + 9 + 7 + 7)

#define USBD_CDC0_EP_CMD (0x81)
#define USBD_CDC0_EP_OUT (0x02)
#define USBD_CDC0_EP_IN (0x82)

#define USBD_CDC1_EP_CMD (0x83)
#define USBD_CDC1_EP_OUT (0x03)
#define USBD_CDC1_EP_IN (0x84)

#define USBD_CMIS_DAP_EP_OUT 0x04
#define USBD_CMIS_DAP_EP_IN 0x85

#define USBD_CDC_CMD_MAX_SIZE (8)
#define USBD_CDC_IN_OUT_MAX_SIZE (64)

#if !PICO_STDIO_USB_DEVICE_SELF_POWERED
#  define USBD_CONFIGURATION_DESCRIPTOR_ATTRIBUTE (0)
#  define USBD_MAX_POWER_MA (250)
#else
#  define USBD_CONFIGURATION_DESCRIPTOR_ATTRIBUTE TUSB_DESC_CONFIG_ATT_SELF_POWERED
#  define USBD_MAX_POWER_MA (1)
#endif

#define TUD_RPI_RESET_DESCRIPTOR(_itfnum, _stridx) 9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_VENDOR_SPECIFIC, RESET_INTERFACE_SUBCLASS, RESET_INTERFACE_PROTOCOL, _stridx

static void stdio_usb_out_chars(const char *buf, int length);
static void stdio_usb_out_flush(void);
int stdio_usb_in_chars(char *buf, int length);

stdio_driver_t stdio_usb = {
  .out_chars = stdio_usb_out_chars,
  .out_flush = stdio_usb_out_flush,
  .in_chars = stdio_usb_in_chars,
  .crlf_enabled = 1,
};

static critical_section_t one_shot_timer_crit_sec;
static volatile bool one_shot_timer_pending;
static uint8_t low_priority_irq_num;
static mutex_t stdio_usb_mutex;

static const tusb_desc_device_t usbd_desc_device = {
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = TUSB_CLASS_MISC,
  .bDeviceSubClass = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor = 0x2E8A,
  .idProduct = 0x0009,
  .bcdDevice = 0x0100,
  .iManufacturer = USBD_STR_MANUF,
  .iProduct = USBD_STR_PRODUCT,
  .iSerialNumber = USBD_STR_SERIAL,
  .bNumConfigurations = 1,
};

#define MK_ENDPOINT(endpoint)    7, TUSB_DESC_ENDPOINT, endpoint, 2, 64,0, 0

#define CMIS_DAP(iface, stridx, epout, epin) 9, TUSB_DESC_INTERFACE, iface, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0, 0, stridx, MK_ENDPOINT(epout), MK_ENDPOINT(epin)

static const uint8_t usbd_desc_cfg[USBD_DESC_LEN] = {
  TUD_CONFIG_DESCRIPTOR(1, USBD_ITF_MAX, USBD_STR_0, USBD_DESC_LEN, USBD_CONFIGURATION_DESCRIPTOR_ATTRIBUTE, USBD_MAX_POWER_MA),

  TUD_CDC_DESCRIPTOR(USBD_ITF_CDC0, USBD_STR_CDC0, USBD_CDC0_EP_CMD, USBD_CDC_CMD_MAX_SIZE, USBD_CDC0_EP_OUT, USBD_CDC0_EP_IN, USBD_CDC_IN_OUT_MAX_SIZE),
  TUD_CDC_DESCRIPTOR(USBD_ITF_CDC1, USBD_STR_CDC1, USBD_CDC1_EP_CMD, USBD_CDC_CMD_MAX_SIZE, USBD_CDC1_EP_OUT, USBD_CDC1_EP_IN, USBD_CDC_IN_OUT_MAX_SIZE),

  CMIS_DAP(USBD_ITF_CMIS_DAP, USBD_STR_CMIS_DAP, USBD_CMIS_DAP_EP_OUT, USBD_CMIS_DAP_EP_IN)

  //TUD_RPI_RESET_DESCRIPTOR(USBD_ITF_RPI_RESET, USBD_STR_RPI_RESET)
};

char usbd_serial_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

static const char *const usbd_desc_str[] = {
  [USBD_STR_MANUF] = "Raspberry Pi",
  [USBD_STR_PRODUCT] = "Pico",
  [USBD_STR_SERIAL] = usbd_serial_str,
  [USBD_STR_CDC0] = "Target Uart",
  [USBD_STR_CDC1] = "Pico Debug",
  [USBD_STR_CMIS_DAP] = "CMSIS-DAP",
//  [USBD_STR_RPI_RESET] = "Reset",
};

const uint8_t *tud_descriptor_device_cb(void) {
  return (const uint8_t *)&usbd_desc_device;
}

const uint8_t *tud_descriptor_configuration_cb(__unused uint8_t index) {
  return usbd_desc_cfg;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, __unused uint16_t langid) {
  static uint16_t desc_str[20];

  if (!usbd_serial_str[0]) {
    pico_get_unique_board_id_string(usbd_serial_str, sizeof(usbd_serial_str));
  }

  uint8_t len;
  if (index == 0) {
    desc_str[1] = 0x0409; // English
    len = 1;
  } else {
    if (index >= (sizeof(usbd_desc_str) / (sizeof(usbd_desc_str[0])))) {
      return NULL;
    }

    const char *str = usbd_desc_str[index];
    for (len = 0; len < 20-1 && str[len]; ++len) {
      desc_str[1+len] = str[len];
    }
  }
  desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * len + 2));
  return desc_str;
}

static int64_t timer_task(__unused alarm_id_t id, __unused void *user_data) {
  int64_t repeat_time;
  if (critical_section_is_initialized(&one_shot_timer_crit_sec)) {
    critical_section_enter_blocking(&one_shot_timer_crit_sec);
    one_shot_timer_pending = false;
    critical_section_exit(&one_shot_timer_crit_sec);
    repeat_time = 0;
  } else {
    repeat_time = 1000;
  }

  if (irq_is_enabled(low_priority_irq_num)) {
    irq_set_pending(low_priority_irq_num);
    return repeat_time;
  } else {
    return 0;
  }
}

static void low_priority_worker_irq(void) {
  if (mutex_try_enter(&stdio_usb_mutex, NULL)) {
    tud_task();
    mutex_exit(&stdio_usb_mutex);
    // TODO, check tud_cdc_available (or relatives) and fire callbacks
    if (tud_cdc_n_available(0)) cdc_has_data(0);
    if (tud_cdc_n_available(1)) cdc_has_data(1);
  }
}

static void usb_irq(void) {
  irq_set_pending(low_priority_irq_num);
}

bool usb_init() {
  if (get_core_num() != alarm_pool_core_num(alarm_pool_get_default())) {
    assert(false);
    return false;
  }

  tusb_init();

  if (!mutex_is_initialized(&stdio_usb_mutex)) mutex_init(&stdio_usb_mutex);

  bool rc = true;

  low_priority_irq_num = (uint8_t) user_irq_claim_unused(true);

  irq_set_exclusive_handler(low_priority_irq_num, low_priority_worker_irq);
  irq_set_enabled(low_priority_irq_num, true);

  if (irq_has_shared_handler(USBCTRL_IRQ)) {
    critical_section_init_with_lock_num(&one_shot_timer_crit_sec, spin_lock_claim_unused(true));
    irq_add_shared_handler(USBCTRL_IRQ, usb_irq, PICO_SHARED_IRQ_HANDLER_LOWEST_ORDER_PRIORITY);
  } else {
    memset(&one_shot_timer_crit_sec, 0, sizeof(one_shot_timer_crit_sec));
    rc = add_alarm_in_us(1000, timer_task, NULL, true) >= 0;
  }

  if (rc) {
    stdio_set_driver_enabled(&stdio_usb, true);
  }
  return rc;
}

bool stdio_usb_connected(void) {
  return tud_cdc_n_connected(1);
}

static void stdio_usb_out_chars(const char *buf, int length) {
  static uint64_t last_avail_time;
  if (!mutex_try_enter_block_until(&stdio_usb_mutex, make_timeout_time_ms(1000))) {
    return;
  }
  if (stdio_usb_connected()) {
    for (int i=0; i<length;) {
      int n = length-i;
      int avail = (int)tud_cdc_n_write_available(1);
      if (n > avail) n = avail;
      if (n) {
        int n2 = (int) tud_cdc_n_write(1, buf + i, (uint32_t)n);
        tud_task();
        tud_cdc_n_write_flush(1);
        i += n2;
        last_avail_time = time_us_64();
      } else {
        tud_task();
        tud_cdc_n_write_flush(1);
        if (!stdio_usb_connected() || (!tud_cdc_n_write_available(1) && time_us_64() > last_avail_time + 500000)) {
          break;
        }
      }
    }
  } else {
    last_avail_time = 0;
  }
  mutex_exit(&stdio_usb_mutex);
}

static void stdio_usb_out_flush(void) {
  if (!mutex_try_enter_block_until(&stdio_usb_mutex, make_timeout_time_ms(1000))) {
    return;
  }
  do {
    tud_task();
  } while (tud_cdc_n_write_flush(1));
  mutex_exit(&stdio_usb_mutex);
}

int stdio_usb_in_chars(char *buf, int length) {
  int rc = PICO_ERROR_NO_DATA;
  if (stdio_usb_connected() && tud_cdc_n_available(1)) {
    if (!mutex_try_enter_block_until(&stdio_usb_mutex, make_timeout_time_ms(1000))) {
      return PICO_ERROR_NO_DATA;
    }
    if (stdio_usb_connected() && tud_cdc_n_available(1)) {
      int count = (int)tud_cdc_n_read(1, buf, (uint32_t)length);
      rc = count ? count : PICO_ERROR_NO_DATA;
    } else {
      tud_task();
    }
    mutex_exit(&stdio_usb_mutex);
  }
  return rc;
}

void tud_mount_cb(void) {
  puts("TUD mount");
}

void tud_umount_cb(void) {
  puts("TUD umount");
}

void tud_cdc_line_coding_cb(__unused uint8_t itf, cdc_line_coding_t const* p_line_coding) {
  printf("interface %d, rate %ld\n", itf, p_line_coding->bit_rate);
  if (p_line_coding->bit_rate == 1200) {
    reset_usb_boot(0, 0);
  }
  if (itf == 0) {
    //uart_set_baudrate(uart0, p_line_coding->bit_rate);
  }
}

static const usbd_class_driver_t app_drivers[] = {
  {
    .name = "DAP ENDPOINT",
    .init = dap_init,
    .reset = dap_reset,
    .open = dap_open,
    .control_xfer_cb = dap_control_xfer_cb,
    .xfer_cb = dap_xfer_cb,
    .sof = NULL,
  }
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
  *driver_count = sizeof(app_drivers) / sizeof(app_drivers[0]);
  return app_drivers;
}
