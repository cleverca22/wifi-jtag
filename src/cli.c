#include "cli.h"
#include "pico/stdlib.h"
#include "tusb.h"

enum state {
  idle
};

static int current_state = idle;

void debug_uart_input_state() {
  while (tud_cdc_n_available(1)) {
    char key;
    tud_cdc_n_read(1, &key, 1);
    switch (current_state) {
    case idle:
      switch (key) {
      case 'D':
        puts("entering gpio debug mode");
        gpio_set_dir(6, GPIO_OUT);
        gpio_set_dir(7, GPIO_OUT);
        gpio_set_dir(8, GPIO_OUT);
        gpio_set_dir(9, GPIO_OUT);
        gpio_set_dir(11, GPIO_OUT);
        break;
      case '6':
        gpio_put(6, !gpio_get(6));
        break;
      case '7':
        gpio_put(7, !gpio_get(7));
        break;
      case '8':
        gpio_put(8, !gpio_get(8));
        break;
      case '9':
        gpio_put(9, !gpio_get(9));
        break;
      case '1':
        gpio_put(11, !gpio_get(11));
        break;
      }
    }
  }
}

