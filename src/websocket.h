#pragma once

#include "server.h"
#include "socket.h"

class WebSocketServer : public TcpServer {
public:
  virtual void accept(struct tcp_pcb *pcb);
};

class WebSocket : public TcpSocket {
public:
  WebSocket(struct tcp_pcb *pcb) : TcpSocket(pcb) {
  }
  virtual err_t recv(struct pbuf *p, err_t err);
};
