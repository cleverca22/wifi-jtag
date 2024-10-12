#pragma once

#include <stdint.h>

class TcpServer {
public:
  void listen(uint16_t port);
  virtual void accept(struct tcp_pcb *pcb) = 0;
private:
  struct tcp_pcb *socket;
};
