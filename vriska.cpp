#include <thread>
#include <set>
#include <vector>

#include "mongoose.h"
#include "cadical/src/cadical.hpp"
#include "cqueue/bcq.h"

int p, width, sym, l4h;
int maxwid, stator;
std::vector<uint64_t> filters;
std::vector<uint64_t> leftborder[2];

#include "logic.h"

namespace _logic {
void solve(std::vector<int> &inst, std::vector<int> crit, auto fn, bool term1=false) {
  CaDiCaL::Solver *solver = new CaDiCaL::Solver;
  solver->set("quiet", 1);
  for (int i : inst)
    solver->add(i);
  int res;
  while ((res = solver->solve()) != 20) {
    assert(res == 10);
    std::vector<int> resp(sz(crit));
    for (int i = 0; i < sz(crit); i++)
      resp[i] = solver->val(crit[i]);
    for (int i = 0; i < sz(crit); i++)
      solver->add(-resp[i]);
    fn(resp), solver->add(0);
    if(term1) break;
  }
  delete solver;
}

void genNextRows(std::vector<uint64_t> &state, int depth, int ahead, auto fn) {
  assert(p * 2 == sz(state));
  int phase = depth % p;
  std::vector<int> inst = {1, 0, -2, 0};
  auto idx = [&](int j) { return ((sym == 1) ? std::min(j, width - j - 1) : j); };
  auto get = [&](int r, int j, int t) {
    if (t == p)
      t = 0, j = (sym == 2) ? width - j - 1 : j;
    r = r * p + t - phase;
    int realRow = (r + depth - 2 * p) / p;
    if (j <= -1 && (realRow < (int)leftborder[-j-1].size())) {
      // WARN << j << ' ' << r << ' ' << t << '\n';
      return (leftborder[-j-1][realRow] & (1ull << t)) ? 1 : 2;
    }
    if (t == -1)
      r = sz(state);
    if (j <= -1 || j >= width)
      return (2 - 0);
    if (r < sz(state))
      return (2 - !!(state[r] & (1ull << j)));
    return 3 + idx(j) + (r - sz(state)) * width;
  };
  auto eqify = [&](int i, int j) {
    for (int x : std::vector<int>{i, -j, 0, -i, j, 0})
      inst.push_back(x);
  };
  for (int row = sz(state); row < sz(state) + ahead; row++) {
    int r = (row + phase) / p, t = (row + phase) % p;
    for (int j = -1; j <= width; j++)
      trans({get(r - 2, j - 1, t), get(r - 2, j, t), get(r - 2, j + 1, t),
              get(r - 1, j - 1, t), get(r - 1, j, t), get(r - 1, j + 1, t),
              get(r, j - 1, t), get(r, j, t), get(r, j + 1, t),
              get(r - 1, j, t + 1)},
            inst);
    if (t != p - 1)
      for (int j = 0; j < width; j++)
        if (width - j <= stator)
          eqify(get(r, j, t), get(r, j, t + 1));
  }
  for (int row = 0; row < sz(state) + ahead; row++) {
    int r = (row + phase) / p, t = (row + phase) % p;
    if (((row + depth - 2 * p) / p) < sz(filters))
      for (int j = 0; j < width; j++)
        if (!(filters[(row + depth - 2 * p) / p] & (1ull << j)))
          eqify(get(r, j, t), 2 - 0);
  }
  std::vector<int> crit;
  {
    std::set<int> _crit;
    for (int j = 0; j < width; j++)
      _crit.insert(get(0, j, -1));
    crit = std::vector<int>(_crit.begin(), _crit.end());
  }
  std::vector<uint64_t> bb(state.begin() + 1, state.end());
  solve(inst, crit, [&](std::vector<int> sol) {
    uint64_t x = 0;
    for (int j = 0; j < width; j++)
      for (int sdx = 0; sdx < sz(crit); sdx++)
        if (crit[sdx] == get(0, j, -1))
          if (sol[sdx] > 0)
            x |= (1ull << j);
    fn(x);
  });
}
}

std::string contributorID;
uint64_t timeout;
std::vector<std::thread> universes;
bool stop_mgr = false;
mg_connection* hostConnection;

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

void computationManager(mg_mgr* const& mgr, const unsigned long conn) {
  // sleep(1);
  std::cerr << "sending getinfo...\n";
  woker(mgr, conn, {0, ""});
  std::cerr << "waiting for response...\n";
  wakeupCall r;
  response.wait_dequeue(r);
  std::cerr << "response: " << r.message << '\n';
}

void handler_httpconn(mg_connection* c, int ev, void* ev_data) {
  // ripped from https://github.com/cesanta/mongoose/blob/master/tutorials/http/http-client/main.c
  if (ev == MG_EV_OPEN) {
    std::cerr << "connection opened\n";
    *(uint64_t *) c->data = mg_millis() + s_timeout_ms;
  } else if (ev == MG_EV_POLL) {
    if (mg_millis() > *(uint64_t *) c->data && (c->is_connecting || c->is_resolving)) {
      mg_error(c, "Connect timeout");
      response.enqueue({-1, ""});
    }
  } else if (ev == MG_EV_CONNECT) {
    // Connected to server. Extract host name from URL
    std::cerr << "connected\n";
    std::string endpoint = s_url, method = "POST";
    wakeupCall* data = (wakeupCall*) c->fn_data;
    if(data->flag == 0) endpoint += "/getconfig", method = "GET";
    if(data->flag == 1) endpoint += "/getwork";
    if(data->flag == 2) endpoint += "/sendwork";
    mg_str host = mg_url_host(endpoint.c_str());
    // Send request
    int content_length = data->message.size();
    if(method == "GET") assert(content_length == 0);
    mg_printf(c,
              "%s %s HTTP/1.0\nHost: %.*s\nContent-Type: octet-stream\nContent-Length: %d\n\n",
              method.c_str(), mg_url_uri(endpoint.c_str()), (int) host.len, host.buf, content_length);
    mg_send(c, data->message.c_str(), content_length);
    std::cerr << "sent to " << endpoint << "\n";
    delete data;
  } else if (ev == MG_EV_HTTP_MSG) {
    // Response is received. Print it
    mg_http_message* hm = (mg_http_message*) ev_data;
    response.enqueue({0, _mg_str_to_stdstring(hm->body)});
    c->is_draining = 1;
  } else if (ev == MG_EV_ERROR) {
    response.enqueue({-1, ""});
  }
}

void fn(mg_connection* c, int ev, void* ev_data) {
  if (ev == MG_EV_ERROR) {
    std::cerr << "connection error: " << c->id << '\n';
  } else if (ev == MG_EV_WS_OPEN) {
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
}