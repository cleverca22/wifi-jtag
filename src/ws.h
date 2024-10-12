#pragma once

#include "server.h"
#include "socket.h"

class WebSocket2 : public TcpSocket {
public:
  WebSocket2(struct tcp_pcb *pcb);
  virtual err_t recv(struct pbuf *p, err_t err);
private:
  int conn;
};

class WebSocketServer2 : public TcpServer {
public:
  virtual void accept(struct tcp_pcb *pcb) {
    WebSocket2 *s = new WebSocket2(pcb);
  }
};
