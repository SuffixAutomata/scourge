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
  // args: host id/name timeout threads
  if(argc != 5) {
    std::cerr << "usage: " << argv[0] << " hostURL contributorID timeout threads\n";
    return 1;
  }
  s_url += argv[1];
  contributorID = argv[2];
  threads = std::stoi(argv[4]);
  mg_mgr mgr;
  mg_mgr_init(&mgr);
  mg_wakeup_init(&mgr);  // Initialise wakeup socket pair
  timeout = mg_millis() + std::stoi(argv[3]) * 1000ull;
  // c = mg_ws_connect(&mgr, host_endpoint.c_str(), fn, &done, NULL);
  std::this_thread::sleep_for(std::chrono::seconds(1));
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
  std::stringstream ix(woke_and_wait({0, ""}));
  ix >> p >> width >> sym >> l4h >> maxwid >> stator;
  int FS; ix>>FS; exInitrow = std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)ix>>exInitrow[i];
  ix >> FS;
  filters = std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)ix>>filters[i];
  for(int s=0;s<2;s++){ix>>FS;leftborder[s]=std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)ix>>leftborder[s][i];}
  ix >> cid;
  std::cerr << "received connection id: "<<cid<<'\n';
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
  int idx = 0;
  while(1) {
    int cnt = B2A.try_dequeue_bulk(nxb.begin(), desired);
    if(cnt != 0 || idx == 8) {
      idx = 0;
      std::stringstream x; x << cnt << ' ' << cid<< '\n';;
      for(int i=0; i<cnt; i++) {
        auto& nx = nxb[i];
        qsize--;
        x<<nx.id << " 1\n";
        x<<contributorID<< ' ' << nx.nextrows.size(); for(auto t:nx.nextrows)x<<' '<<t;x<<'\n';
      }
      assert(woke_and_wait({2, x.str()}) == "OK");
    } else if(cnt == 0) idx++;
    if(mg_millis() > (timeout)) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    handle();
  }
  // fetch threads+1 workunits
  woke_and_wait({1, std::to_string(cid)+" 0 1"});
  mg_mgr_free(&mgr);
  return 0;
}