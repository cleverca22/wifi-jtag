#pragma once
#include <cstddef>
#include <stdint.h>
#include "lwip/tcp.h"

class TcpSocket {
public:
  TcpSocket(struct tcp_pcb *pcb);
  virtual ~TcpSocket();
  size_t write(const char *buf, size_t len);
  virtual err_t recv(struct pbuf *p, err_t err);
//protected:
  struct tcp_pcb *socket;
};
