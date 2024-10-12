#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "server.h"
#include "socket.h"
#include "websocket.h"
#include "ws.h"

#define DEBUG_printf printf

#define RESET_PIN 2

class ReplSocket;
static void connect_callback();
static void disconnect_callback();
static void uart_worker_fun(async_context_t *context, struct async_work_on_timeout *timeout);
static void core1_main();

ReplSocket *last_repl = NULL;
async_context_t *async_ctx = NULL;

//async_at_time_worker_t uart_worker = {
//  .do_work = uart_worker_fun,
//  .next_time = 0,
//};

class ReplSocket : public TcpSocket {
public:
  ReplSocket(struct tcp_pcb *pcb) : TcpSocket(pcb) {
    write("hello\n", 0);
    last_repl = this;
    connect_callback();
  }
  ~ReplSocket() {
    if (last_repl == this) last_repl = NULL;
    disconnect_callback();
  }
  err_t recv(struct pbuf *p, err_t err) {
    if (p) {
      struct pbuf *p2 = p;
      while (p2) {
        uart_write_blocking(uart0, (const uint8_t*)p2->payload, p2->len);
        p2 = p2->next;
      }
      tcp_recved(socket, p->tot_len);
      pbuf_free(p);
      return ERR_OK;
    } else {
      puts("disconnected");
      delete this;
      return ERR_OK;
    }
  }
};


class ReplServer : public TcpServer {
public:
  virtual void accept(struct tcp_pcb *pcb) {
    ReplSocket *s = new ReplSocket(pcb);
  }
};

ReplServer server1;
WebSocketServer server2;

extern cyw43_t cyw43_state;

static void server_init() {
  server1.listen(1234);
  server2.listen(1235);
}

static void connect_callback() {
  gpio_set_dir(RESET_PIN, GPIO_IN);
}

static void disconnect_callback() {
  gpio_set_dir(RESET_PIN, GPIO_OUT);
  gpio_put(RESET_PIN, 0);
}

static void uart_handler() {
  char buffer[128];
  int n=0;
  while (uart_is_readable(uart0) && (n < 127)) {
    buffer[n] = uart_get_hw(uart0)->dr;
    n++;
  }
  buffer[n] = 0;
  //uart_worker.user_data = n;
  async_context_acquire_lock_blocking(async_ctx);
  if (last_repl) {
    last_repl->write(buffer, n);
    tcp_output(last_repl->socket);
    puts("sent packet");
  }
  async_context_release_lock(async_ctx);
}

static void led_set(int val) {
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, val);
}

int main() {
  disconnect_callback();
  stdio_init_all();

  uart_init(uart0, 115200);
  uart_set_fifo_enabled(uart0, true);
  gpio_set_function(0, GPIO_FUNC_UART);
  gpio_set_function(1, GPIO_FUNC_UART);

  gpio_set_function(RESET_PIN, GPIO_FUNC_SIO);
  gpio_set_pulls(RESET_PIN, true, false);

  //uart_putc_raw(uart0, 'A');

#if 0
  while (!tud_cdc_connected()) {
    printf(".");
    //uart_puts(uart0, "hello\n");
    sleep_ms(500);
  }
  printf("\nusb host detected!\n");
#endif

  if (cyw43_arch_init()) {
    printf("failed to initialise\n");
    return 1;
  }

  async_ctx = cyw43_arch_async_context();
  multicore_launch_core1(core1_main);

  cyw43_arch_enable_sta_mode();

  printf("Connecting to Wi-Fi...\n");
  if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
    printf("failed to connect.\n");
    return 1;
  } else {
    printf("Connected.\n");
  }

  u32_t ip_addr = cyw43_state.netif[CYW43_ITF_STA].ip_addr.addr;
  printf("IP Address: %lu.%lu.%lu.%lu\n", ip_addr & 0xFF, (ip_addr >> 8) & 0xFF, (ip_addr >> 16) & 0xFF, ip_addr >> 24);

  server_init();

  while (true) {
    led_set(1);
    sleep_ms(1000);
    led_set(0);
    sleep_ms(1000);
  }
}

static void core1_main() {
  irq_set_exclusive_handler(UART0_IRQ, uart_handler);
  irq_set_enabled(UART0_IRQ, true);
  uart_set_irq_enables(uart0, true, false);
}
