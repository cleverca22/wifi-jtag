#include "socket.h"
#include <string.h>

#define POLL_TIME_S 5

static err_t client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  return ERR_OK;
}

static err_t client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  //printf("recv %x %x %x %d\n", arg, tpcb, p, err);
  TcpSocket *s = (TcpSocket*)arg;
  return s->recv(p, err);
}

err_t TcpSocket::recv(struct pbuf *p, err_t err) {
  if (p) {
    printf("tot_len: %d\n", p->tot_len);
    struct pbuf *p2 = p;
    while (p2) {
      //printf("len %d ptr %x\n", p2->len, p2->payload);
      p2 = p2->next;
    }
    tcp_recved(socket, p->tot_len);
    pbuf_free(p);
  } else {
    puts("disconnected");
    // TODO, delete self
  }
}

static err_t client_poll(void *arg, struct tcp_pcb *tpcb) {
  return ERR_OK;
}

static void client_err(void *arg, err_t err) {
}

TcpSocket::TcpSocket(struct tcp_pcb *pcb) {
  socket = pcb;

  tcp_arg(socket, this);
  tcp_sent(socket, client_sent);
  tcp_recv(socket, client_recv);
  tcp_poll(socket, client_poll, POLL_TIME_S * 2);
  tcp_err(socket, client_err);
}

TcpSocket::~TcpSocket() {
  tcp_close(socket);
}

size_t TcpSocket::write(const char *buf, size_t len) {
  if (len == 0) len = strlen(buf);

  err_t err = tcp_write(socket, buf, len, TCP_WRITE_FLAG_COPY);
  if (err == ERR_OK) return len;
  else return -1;
}
