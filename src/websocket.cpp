#include "websocket.h"
#include <stdint.h>
#include <assert.h>
#include <string>
#include <iostream>

using namespace std;

void WebSocketServer::accept(struct tcp_pcb *pcb) {
  WebSocket *s = new WebSocket(pcb);
}

err_t WebSocket::recv(struct pbuf *p, err_t err) {
  char buf[64];
  if (!p) {
    puts("ws disconnected");
    // TODO, delete self
  } else {
    struct pbuf *p2 = p;
    uint16_t pos;
    while ((pos = pbuf_memfind(p2, "\n", 1, 0)) != 0xffff) {
      printf("ws tot_len: %d\n", p2->tot_len);
      printf("found newline at pos %d\n", pos);
      assert(pos < 64);
      const char *line = (const char *)pbuf_get_contiguous(p2, (void*)buf, 64, pos, 0);
      string line2 = string(line, pos);
      p2 = pbuf_skip(p2, pos+1, 0);

      cout << line2 << "\n";
    }
  }
  return ERR_OK;
}
