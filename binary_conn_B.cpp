#include <thread>
#include <set>
#include <vector>
#include <sstream>

#include "mongoose.h"

#include "globals.h"

std::atomic_bool done = false;
long long cid = -1;
mg_connection* hostConnection;

// std::string s_url = "https://";
std::string s_url = "http://";
const uint64_t s_timeout_ms = 5000;  // Connect timeout in milliseconds

std::string _mg_str_to_stdstring(mg_str x) {
  return std::string(x.buf, x.buf + x.len);
}

struct wakeupCall {
  int flag = 0; // 0 - send getinfo, 1 - send getwork, 2 - send returnwork
  std::string message;
};
void woker(mg_mgr* const& mgr, const unsigned long conn, const wakeupCall& s) {
  wakeupCall* resp = new wakeupCall {s};
  mg_wakeup(mgr, conn, &resp, sizeof(resp));
}

wakeupCall t_response;

void handler_httpconn(mg_connection* c, int ev, void* ev_data) {
  // ripped from https://github.com/cesanta/mongoose/blob/master/tutorials/http/http-client/main.c
  if (ev == MG_EV_OPEN) {
    // std::cerr << "connection opened\n";
    *(uint64_t *) c->data = mg_millis();
  } else if (ev == MG_EV_POLL) {
    if (mg_millis() - s_timeout_ms > *(uint64_t *) c->data && (c->is_connecting || c->is_resolving)) {
      mg_error(c, "Connect timeout");
      std::cerr << "timeout\n";
      done = true;
      t_response = {-1, ""};
    }
  } else if (ev == MG_EV_CONNECT) {
    std::string endpoint = s_url, method = "POST";
    wakeupCall* data = (wakeupCall*) c->fn_data;
    if(data->flag == 0) endpoint += "/get", method = "GET";
    if(data->flag == 1) endpoint += "/send";
    mg_str host = mg_url_host(endpoint.c_str());
    // Send request
    int content_length = data->message.size();
    if(method == "GET") assert(content_length == 0);
    mg_printf(c,
              "%s %s HTTP/1.0\nHost: %.*s\nContent-Type: octet-stream\nContent-Length: %d\n\n",
              method.c_str(), mg_url_uri(endpoint.c_str()), (int) host.len, host.buf, content_length);
    mg_send(c, data->message.data(), content_length);
    std::cerr << "sent to " << endpoint << ": " << data->message << "\n";
    delete data;
  } else if (ev == MG_EV_HTTP_MSG) {
    if((*(int *) (c->data+8)) == -1) { c->is_draining = 1; return; }
    (*(int *) (c->data+8)) = -1;
    // Response is received. Print it
    mg_http_message* hm = (mg_http_message*) ev_data;
    t_response = {0, _mg_str_to_stdstring(hm->body)};
    c->is_draining = 1;
    std::cerr << "latency: " << mg_millis() - (*(uint64_t *) c->data) << " ms, repsonse: "<<_mg_str_to_stdstring(hm->body)<<'\n';
    done = true;
  } else if (ev == MG_EV_ERROR) {
    std::cerr << "error\n";
    t_response = {-1, ""};
    done = true;
  }
}

void graceful_exit(mg_mgr* mgr) {
  mg_mgr_free(mgr);
  exit(0);
}

int main(int argc, char* argv[]) {
  s_url += argv[1];
  mg_mgr mgr;
  mg_mgr_init(&mgr);
  // mg_wakeup_init(&mgr);  // Initialise wakeup socket pair
  auto woke_and_wait = [&](const wakeupCall& s) {
    // woker(mgr, conn, s);
    wakeupCall* data = new wakeupCall {s};
    done = false;
    mg_http_connect(&mgr, s_url.c_str(), handler_httpconn, data);
    auto stT = mg_millis();
    while(!done) {
      mg_mgr_poll(&mgr, 100);
      if(mg_millis() > stT + 60*1000ul)
        graceful_exit(&mgr);
    }
    if(t_response.flag != 0)
      graceful_exit(&mgr);
    // std::cerr << "got a response\n";
    return t_response.message;
  };  
  std::stringstream ix(woke_and_wait({0, ""}), std::ios::binary);
  int v1; readInt(v1, ix);
  std::string v2; readString(v2, ix);
  std::cerr << v1 << ' ' << v2 << '\n';
  std::stringstream res(std::ios::binary);
  writeInt(16777216, res);
  writeString("serket", res);
  woke_and_wait({1, res.str()});
  mg_mgr_free(&mgr);
  return 0;
}