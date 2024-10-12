#include "ws.h"

extern "C" {
  void http_recvcb(int conn, char *pusrdata, unsigned short length);
  int httpserver_connectcb( int socket ); // return which HTTP it is.  -1 for failure
};

WebSocket2::WebSocket2(struct tcp_pcb *pcb) : TcpSocket(pcb) {
  conn = httpserver_connectcb(0);
}

err_t WebSocket2::recv(struct pbuf *p, err_t err) {
  if (p) {
    printf("tot_len: %d\n", p->tot_len);
    struct pbuf *p2 = p;
    while (p2) {
      printf("len %d ptr %x\n", p2->len, p2->payload);
      http_recvcb(conn, (char*)p2->payload, p2->len);
      p2 = p2->next;
    }
    tcp_recved(socket, p->tot_len);
    pbuf_free(p);
  } else {
    puts("ws2 disconnected");
    // TODO, delete self
  }
  return ERR_OK;
}
