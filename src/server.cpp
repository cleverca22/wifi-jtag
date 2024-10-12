#include "server.h"
#include "socket.h"
#include "lwip/tcp.h"

#define DEBUG_printf printf

static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
  TcpServer *t = (TcpServer*)arg;

  if (err != ERR_OK || client_pcb == NULL) {
    DEBUG_printf("Failure in accept\n");
    return ERR_VAL;
  }

  t->accept(client_pcb);

  return ERR_OK;
}

void TcpServer::listen(uint16_t port) {
  struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  err_t err = tcp_bind(pcb, NULL, port);
  socket = tcp_listen_with_backlog(pcb, 1);
  tcp_arg(socket, this);
  tcp_accept(socket, tcp_server_accept);
}
