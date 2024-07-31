#include <thread>
#include <set>
#include <vector>

#include "mongoose.h"
#include "cadical/src/cadical.hpp"
#include "cqueue/bcq.h"

int p, width, sym, l4h;
int maxwid, stator;
std::vector<uint64_t> exInitrow;
std::vector<uint64_t> filters;
std::vector<uint64_t> leftborder[2];

#include "logic.h"
#include "satlogic.h"

std::string contributorID;
uint64_t timeout;
std::vector<std::thread> universes;
int threads;
bool stop_mgr = false;
long long cid = -1;
mg_connection* hostConnection;

// std::string s_url = "https://";
std::string s_url = "http://";
const uint64_t s_timeout_ms = 1500;  // Connect timeout in milliseconds

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

moodycamel::BlockingConcurrentQueue<wakeupCall> response;

struct A2Bunit{int id, depth; std::vector<uint64_t> rows;};
moodycamel::BlockingConcurrentQueue<A2Bunit> A2B;
struct B2Aunit{int id; std::vector<uint64_t> nextrows;};
moodycamel::BlockingConcurrentQueue<B2Aunit> B2A;

void betaUniverse() {
  A2Bunit nx;
  while(1) {
    A2B.wait_dequeue(nx);
    // std::cerr <<"hi...\n";
    std::vector<uint64_t> resp;
    int dep = nx.depth;
    genNextRows(nx.rows, dep, l4h, [&](uint64_t x){ 
      resp.push_back(x);
    });
    // std::cerr << "found "<<resp.size()<<" solutions\n";
    B2A.enqueue({nx.id, resp});
  }
}


void computationManager(mg_mgr* const& mgr, const unsigned long conn) {
  // computationManager runs to end of main
  // sleep(1);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  auto woke_and_wait = [&](const wakeupCall& s) {
    woker(mgr, conn, s);
    wakeupCall r;
    response.wait_dequeue(r);
    return r.message;
  };  
  std::stringstream ix(woke_and_wait({0, ""}));
  /* res << p << ' ' << width << ' ' << sym << ' ' << l4h << ' ' << maxwid << ' ' << stator << ' ';
      res << filters.size(); for(auto i:filters) res << ' ' << i;
      for(int s=0;s<2; s++){
        res<<' '<<leftborder[s].size();
        for(auto i:leftborder[s]) res<<' '<<i;
      }
      res<<'\n';*/
  ix >> p >> width >> sym >> l4h >> maxwid >> stator;
  int FS; ix>>FS; exInitrow = std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)ix>>exInitrow[i];
  ix >> FS;
  filters = std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)ix>>filters[i];
  for(int s=0;s<2;s++){ix>>FS;leftborder[s]=std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)ix>>leftborder[s][i];}
  for(int i=0;i<threads;i++) universes.emplace_back(betaUniverse);
  int qsize=0, desired = threads*20;
  auto handle = [&]() {
    if(qsize == desired) return;
    std::stringstream x(woke_and_wait({1, std::to_string(cid)+" 1 "+std::to_string(desired - qsize)}));
    int cnt; x >> cnt;
    assert(cnt >= 0); // -1: fail, server asked us to die
    for(int i=0; i<cnt; i++) {
      int id, depth; x >> id >> depth;
      std::vector<uint64_t> rows(2*p); for(auto& s:rows)x>>s;
      A2B.enqueue({id, depth, rows});
    }
    qsize += cnt;
    // std::cerr << qsize << std::endl;
    if(qsize != desired) {
      std::this_thread::sleep_for(std::chrono::seconds(3));
    }
  };
  handle();
  std::vector<B2Aunit> nxb(desired);
  while(1) {
    int cnt = B2A.try_dequeue_bulk(nxb.begin(), desired);
    if(cnt != 0) {
      std::stringstream x; x << cnt << ' ' << cid<< '\n';;
      for(int i=0; i<cnt; i++) {
        auto& nx = nxb[i];
        qsize--;
        x<<nx.id << " 1\n";
        x<<contributorID<< ' ' << nx.nextrows.size(); for(auto t:nx.nextrows)x<<' '<<t;x<<'\n';
      }
      assert(woke_and_wait({2, x.str()}) == "OK");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    handle();
  }
  // fetch threads+1 workunits
  
  woker(mgr, conn, {1, std::to_string(cid)+" 0 1"});

}

void handler_httpconn(mg_connection* c, int ev, void* ev_data) {
  // ripped from https://github.com/cesanta/mongoose/blob/master/tutorials/http/http-client/main.c
  if (ev == MG_EV_OPEN) {
    // std::cerr << "connection opened\n";
    *(uint64_t *) c->data = mg_millis();
  } else if (ev == MG_EV_POLL) {
    if (mg_millis() - s_timeout_ms > *(uint64_t *) c->data && (c->is_connecting || c->is_resolving)) {
      mg_error(c, "Connect timeout");
      response.enqueue({-1, ""});
    }
  } else if (ev == MG_EV_CONNECT) {
    // struct mg_tls_opts opts = {.ca = mg_unpacked("/certs/ca.crt"),
    //                            .cert = mg_unpacked("certs/client.crt"),
    //                            .key = mg_unpacked("certs/client.key")};
    // mg_tls_init(c, &opts);
    // Connected to server. Extract host name from URL
    // std::cerr << "connected\n";
    std::string endpoint = s_url, method = "POST";
    wakeupCall* data = (wakeupCall*) c->fn_data;
    if(data->flag == 0) endpoint += "/getconfig", method = "GET";
    if(data->flag == 1) endpoint += "/getwork";
    if(data->flag == 2) endpoint += "/returnwork";
    mg_str host = mg_url_host(endpoint.c_str());
    // Send request
    int content_length = data->message.size();
    if(method == "GET") assert(content_length == 0);
    mg_printf(c,
              "%s %s HTTP/1.0\nHost: %.*s\nContent-Type: octet-stream\nContent-Length: %d\n\n",
              method.c_str(), mg_url_uri(endpoint.c_str()), (int) host.len, host.buf, content_length);
    mg_send(c, data->message.c_str(), content_length);
    std::cerr << "sent to " << endpoint << ": " << data->message << "\n";
    delete data;
  } else if (ev == MG_EV_HTTP_MSG) {
    if((*(int *) (c->data+8)) == -1) { c->is_draining = 1; return; }
    (*(int *) (c->data+8)) = -1;
    // Response is received. Print it
    mg_http_message* hm = (mg_http_message*) ev_data;
    response.enqueue({0, _mg_str_to_stdstring(hm->body)});
    c->is_draining = 1;
    std::cerr << "latency: " << mg_millis() - (*(uint64_t *) c->data) << " ms, repsonse: "<<_mg_str_to_stdstring(hm->body)<<'\n';
  } else if (ev == MG_EV_ERROR) {
    response.enqueue({-1, ""});
  }
}

void fn(mg_connection* c, int ev, void* ev_data) {
  if (ev == MG_EV_ERROR) {
    std::cerr << "connection error: " << c->id << '\n';
  } /*else if (ev == MG_EV_CONNECT) {
    struct mg_tls_opts opts = {.ca = mg_unpacked("/certs/ca.crt"),
                               .cert = mg_unpacked("certs/client.crt"),
                               .key = mg_unpacked("certs/client.key")};
    mg_tls_init(c, &opts);
  } */else if (ev == MG_EV_WS_OPEN) {
    // mg_ws_send(c, "hello", 5, WEBSOCKET_OP_TEXT); // replace with proper identifier
    // don't need to initialize now
    assert(hostConnection == 0);
    hostConnection = c;
    std::thread t(computationManager, c->mgr, c->id); t.detach();
  } else if (ev == MG_EV_WS_MSG) {
    mg_ws_message* wm = (mg_ws_message*)ev_data;
    if(mg_match(wm->data, mg_str("ping"), NULL)) {
      // std::cerr << "received ping\n";
      mg_ws_send(c, "pong", 4, WEBSOCKET_OP_TEXT);
    } else if(cid == -1) {
      cid = std::stoll(_mg_str_to_stdstring(wm->data));
      std::cerr << "received connection id: "<<cid<<'\n';
    }
    // printf("reply: %.*s\n", (int) wm->data.len, wm->data.buf);
  } else if (ev == MG_EV_WS_CTL) {
    mg_ws_message* wm = (mg_ws_message*) ev_data;
    uint32_t s = (wm->flags) & 0xF0;
    // MG_INFO(("\033[1;1;31mwebsocket control triggered, %.*s, %x \033[0m", wm->data.len, wm->data.buf, s));
    if(s == 0x80) {
      stop_mgr = true;
    }
  } else if (ev == MG_EV_POLL) {
    if(mg_millis() > timeout) {
      timeout = mg_millis() + 10 * 1000;
      std::cerr << "disconnecting\n";
      mg_ws_send(hostConnection, NULL, 0, WEBSOCKET_OP_CLOSE);
      // stop_mgr = true;
    }
  } else if (ev == MG_EV_WAKEUP) {
    // struct mg_str *data = (struct mg_str *) ev_data;
    wakeupCall* data = * (wakeupCall**) ((mg_str*) ev_data)->buf;
    mg_http_connect(c->mgr, s_url.c_str(), handler_httpconn, data);
  } else if (ev == MG_EV_CLOSE) {
    if(c == hostConnection) {
      stop_mgr = true;
    }
  }
}

int main(int argc, char* argv[]) {
  // args: host id/name timeout threads
  if(argc != 5) {
    std::cerr << "usage: " << argv[0] << " hostURL contributorID timeout threads\n";
    return 1;
  }
  s_url += argv[1];
  std::string host_endpoint = s_url + "/worker-websocket";
  contributorID = argv[2];
  threads = std::stoi(argv[4]);
  mg_mgr mgr;
  bool done = false;
  mg_connection* c;
  mg_mgr_init(&mgr);
  mg_wakeup_init(&mgr);  // Initialise wakeup socket pair
  timeout = mg_millis() + std::stoi(argv[3]) * 1000ull;
  c = mg_ws_connect(&mgr, host_endpoint.c_str(), fn, &done, NULL);
  while (c && !stop_mgr) {
    mg_mgr_poll(&mgr, 1000);
  }
  mg_mgr_free(&mgr);
  return 0;
}