#include "cadical/src/cadical.hpp"

#include "mongoose.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <vector>
#include <thread>
#include <vector>
#include <sstream>

using std::max;
using std::min;

#include "debug.h"
#include "globals.h"
#include "logic.h"
#include "satlogic.h"
#include "searchtree.h"

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
    if(data->flag == 0) endpoint += "/getuniqueid", method = "GET";
    if(data->flag == 1) endpoint += "/getwork";
    if(data->flag == 2) endpoint += "/returnwork";
    if(data->flag == 3) endpoint += "/trickle";
    mg_str host = mg_url_host(endpoint.c_str());
    // Send request
    int content_length = data->message.size();
    if(method == "GET") assert(content_length == 0);
    mg_printf(c,
              "%s %s HTTP/1.0\nHost: %.*s\nContent-Type: octet-stream\nContent-Length: %d\n\n",
              method.c_str(), mg_url_uri(endpoint.c_str()), (int) host.len, host.buf, content_length);
    mg_send(c, data->message.data(), content_length);
    std::cerr << "sent to " << endpoint << " " << content_length << " bytes\n";
    delete data;
  } else if (ev == MG_EV_HTTP_MSG) {
    if((*(int *) (c->data+8)) == -1) { c->is_draining = 1; return; }
    (*(int *) (c->data+8)) = -1;
    // Response is received. Print it
    mg_http_message* hm = (mg_http_message*) ev_data;
    t_response = {0, _mg_str_to_stdstring(hm->body)};
    c->is_draining = 1;
    std::cerr << "latency: " << mg_millis() - (*(uint64_t *) c->data) << " ms, repsonse len: "<<_mg_str_to_stdstring(hm->body).size()<<'\n';
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

searchTree* T;

struct A2BUnit { int onx, side, idx; };
std::queue<A2BUnit> searchQueue;
std::map<int, std::vector<std::vector<std::vector<halfrow>>>> staging;
std::map<int, int> remaining;

struct B2AUnit { uint64_t row; int response; A2BUnit fa; };
std::queue<B2AUnit> backQueue;

int enqTreeNode(int onx) {
  for(int x=0; x<2; x++)
    for(int j=0; j<T->a[onx].n[x]; j++)
      searchQueue.push({onx, x, j}), remaining[onx]++;
  staging[onx] = std::vector<std::vector<std::vector<halfrow>>>(4, std::vector<std::vector<halfrow>>(2));
  return onx;
}

void betaUniverse() {
  A2BUnit nx;
  while (searchQueue.size()) {
    nx = searchQueue.front();
    searchQueue.pop();
    int dep = T->a[nx.onx].depth;
    std::vector<uint64_t> h = T->getState(nx.onx, nx.side, nx.idx);
    genNextRows(h, dep, l4h, nx.side ? enforce2 : enforce,
                nx.side ? remember2 : remember, [&](uint64_t x) {
                  backQueue.push({x, dep + 1, nx});
                });
    backQueue.push({0, -1, nx});
    // while(!searchQueue.size() && toEnq.size() && ((t2 - t1).count() < 1 * 3600 * 1e9)) {
    //   enqTreeNode(toEnq.front());
    //   toEnq.pop();
    // }
  }
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
  int wsid;
  {
    std::istringstream ix(woke_and_wait({0, ""}));
    ix >> wsid;
    assert(!ix.fail());
  }
  auto actualgetWork = [&]() {
    int amnt = 0;
    while(amnt == 0) {
      std::string v = std::to_string(wsid) + " 1 1";
      std::istringstream ix(woke_and_wait({1, v}), std::ios::binary);
      int amnt; readInt(amnt, ix);
      if(amnt == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }else {
        assert(amnt == 1);
        std::string p;
        readString(p, ix);
        return p;
      }
    }
  };
  while(1) {
    std::istringstream ix(actualgetWork(), std::ios::binary);
    T = loadf(ix, "loadWorkunit");
    for (int i = 0; i < T->treeSize; i++) {
      if (T->a[i].n[0] == 0 || T->a[i].n[1] == 0)
        T->a[i].tags &= ~TAG_QUEUED;
      if (T->a[i].tags & TAG_QUEUED)
        enqTreeNode(i);
    }
    assert(staging.size() == 1);
    std::queue<int> toEnq;
    auto process = [&](B2AUnit x) {
      int id = x.fa.onx, depth = T->a[x.fa.onx].depth + 1;
      if (x.response == -1) {
        if(!--remaining[id]) {
          {
            auto& w = staging[id];
            for(int v=0; v<4; v++)
              if(sz(w[v][0]) && sz(w[v][1])) // <- Pruning is reduced to one literal line
                toEnq.push(T->newNode({v, depth, TAG_QUEUED, id}, w[v]));
          }
          T->a[x.fa.onx].tags &= ~TAG_QUEUED;
          staging.erase(staging.find(id));
        }
      } else {
        if ((depth - 1) / p < sz(filters) && ((x.row & filters[(depth - 1) / p]) != x.row)) {
          WARN << x.row << ' ' << filters[(depth - 1) / p] << '\n';
          assert(0);
          // cout << "[1NFO] NOD3 1GNOR3D" << endl;
        } else
          staging[id][CENTER_ID(x.row)][x.fa.side].push_back({x.row, x.fa.idx});
      }
    };
    A2BUnit nx;
    auto t2 = std::chrono::high_resolution_clock::now();
    int it = 0;
    // TODO: send file
    // std::ofstream fx(argv[2]);
    // dumpf(fx, "dumpWorkunitResponse");
    // fx.close();
    t2 = std::chrono::high_resolution_clock::now();
  }
  // std::cerr << tree.treeSize << std::endl;
  // std::cerr << toEnq.size() << std::endl;
  return 0;
}