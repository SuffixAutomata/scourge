// TODO: why do nodes swallow & forget about their work units?

#include "mongoose.h"

#include <bits/stdc++.h>

#include "globals.h"

std::string _mg_str_to_stdstring(mg_str x) {
  return std::string(x.buf, x.buf + x.len);
}

// HTTP request callback
void fn(mg_connection* c, int ev, void* ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    mg_http_message* hm = (mg_http_message*) ev_data;
    if(mg_match(hm->uri, mg_str("/get"), NULL)) {
      // GET
      std::ostringstream res(std::ios::binary);
      writeInt(69, res);
      writeString("terezi", res);
      mg_http_reply(c, 200, "Content-Type: text/binary\n", "%.*s", (int)res.str().size(), res.str().data());
    } else if(mg_match(hm->uri, mg_str("/send"), NULL)) {
      // POST
      std::istringstream co(_mg_str_to_stdstring(hm->body), std::ios::binary);
      int v1; readInt(v1, co);
      std::string v2; readString(v2, co);
      if(co.fail()) goto fail;
      std::cerr << v1 << ' ' << v2 << '\n';
      mg_http_reply(c, 200, "Content-Type: text/raw\n", "OK");
      return;
      fail:
      std::cerr << "returnwork failed" << std::endl;
      mg_http_reply(c, 200, "Content-Type: text/raw\n", "FAIL");
    } 
    /* ?? */
    else {
      MG_INFO(("unknown endpoint %.*s", hm->uri.len, hm->uri.buf));
      mg_http_reply(c, 404, "Content-Type: text/raw\n", "what\n");
    }
  }
}

int main(int argc, const char* argv[]) {
  mg_mgr mgr;
  mg_mgr_init(&mgr);        // Initialise event manager
  mg_log_set(MG_LL_INFO); 
  // mg_http_listen(&mgr, "https://localhost:8000", fn, NULL);  // Create listener
  std::string hostname = "http://localhost:8000";
  if(argc != 1) hostname = argv[1];
  mg_http_listen(&mgr, hostname.c_str(), fn, NULL);  // Create listener
  std::cerr << "Listening on " << hostname << std::endl;
  mg_wakeup_init(&mgr);  // Initialise wakeup socket pair
  for (;;) {             // Event loop
    mg_mgr_poll(&mgr, 1000);
  }
  mg_mgr_free(&mgr);
  // free(ca_cert.buf), free(server_cert.buf), free(server_key.buf);
  return 0;
}