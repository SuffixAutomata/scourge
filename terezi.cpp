#include "mongoose.h"

#include <bits/stdc++.h>

#include "cqueue/bcq.h"

int worker_ping_duration = 3000; // ms

int p, width, sym, l4h;
int maxwid, stator;

namespace _searchtree {
std::mutex searchtree_mutex;
int depths[1000], total[1000];
// state: u - unqueued & done, q - queued to be sent, 
struct node { uint64_t row; int depth, shift, parent, contrib; char state; };
node* tree = new node[16777216];
int treeSize = 0, treeAlloc = 16777216;
std::vector<uint64_t> filters;
std::vector<uint64_t> leftborder[2];
std::vector<std::string> contributors; std::map<std::string, int> contributorIDs;

void dumpTree(std::string fn) {
  std::ofstream fx(fn);
  fx << p << ' ' << width << ' ' << sym << ' ' << stator << '\n';
  fx << filters.size(); for(auto s:filters){fx << ' ' << s;} fx << '\n';
  for(int t=0;t<2;t++){ fx<<leftborder[t].size(); for(auto s:leftborder[t]){fx<<' '<<s;}fx<<'\n';}
  fx<<treeSize<<'\n';
  for(int i=0;i<treeSize;i++)
    fx<<tree[i].row<<' '<<tree[i].shift<<' '<<tree[i].parent<<' '<<tree[i].contrib<<' '<<tree[i].state<<'\n';
  fx<<contributorIDs.size()<<'\n';
  for(auto s:contributors) fx<<s<<'\n';
  fx.flush(); fx.close();
}

void loadTree(std::string fn) {
  using namespace std;
  ifstream fx(fn);
  fx>>p>>width>>sym>>stator;
  int FS; fx>>FS; filters=std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)fx>>filters[i];
  for(int pp=0;pp<2;pp++){fx>>FS;leftborder[pp]=std::vector<uint64_t>(FS);
  for(int t=0;t<FS;t++)fx>>leftborder[pp][t];}
  fx>>treeSize;
  for(int i=0;i<treeSize;i++){
    fx>>tree[i].row>>tree[i].shift>>tree[i].parent>>tree[i].contrib>>tree[i].state;
    tree[i].depth = 1 + (i ? tree[tree[i].parent].depth : 0);
  }
  fx>>FS;
  contributors=std::vector<std::string>(FS);
  for(int i=0; i<FS;i++) {
    fx>>contributors[i];
    contributorIDs[contributors[i]] = i;
  }
}

// void flushTree(node* dest = tree) { assert(0); }

int newNode() {
  assert(treeSize < treeAlloc);
  // if(treeSize == treeAlloc)
  //   treeAlloc *= 2, flushTree(new node[treeAlloc]);
  // if((treeSize - time(0)) % 8192 == 0) flushTree();
  tree[treeSize] = {0,0,0,0,0,'0'};
  return treeSize++;
}

std::vector<uint64_t> getState(int onx) {
  std::vector<uint64_t> mat;
  while(onx != -1)
    mat.push_back(tree[onx].row), onx = tree[onx].parent;
  std::reverse(mat.begin(), mat.end());
  return std::vector<uint64_t>(mat.end() - 2*p, mat.end());
}

int getWidth(int i) {
  uint64_t bito = 0;
  for(uint64_t x:getState(i)){ bito |= x; }
  return 64-__builtin_clzll(bito)-__builtin_ctzll(bito);
}
}; using namespace _searchtree;

#include "logic.h"

int bdep = 0;
void emit(int state, bool d = true){
  using namespace std;
  vector<uint64_t> mat;
  while(state != -1)
    mat.push_back(tree[state].row), state = tree[state].parent;
  reverse(mat.begin(), mat.end());
  if(!d) {
    if(((int)(mat.size())) <= bdep) return;
    bdep = mat.size();
  }else
    cout<<"[[OSC1LL4TOR COMPL3T3!!!]]"<<endl;
  std::string rle;
  int cnt = 0, cur = 0;
  auto f = [&](char x) {
    if(x != cur) {
      if(cnt >= 2) rle += to_string(cnt);
      if(cnt >= 1) rle += (char)cur;
      cnt = 0, cur = x;
    }
    cnt++;
  };
  for(int r=0; r*p<((int)(mat.size())); r++){
    if(r) f('$');
    for(int x=r*p; x<((int)(mat.size())) && x<(r+1)*p; x++){
      for(int j=0;j<width;j++) f("bo"[!!(mat[x]&(1ull<<j))]);
      f('b');f('b');f('b');
    }
  }
  f('!'); 
  cout << "x = 0, y = 0, rule = B3/S23\n" + rle + '!' << endl;
}

std::mutex pendingOutbound_mutex;
std::queue<int> pendingOutbound;
struct pendingInboundMessage {
  int id;
  bool completed;
  std::string contributor;
  std::vector<uint64_t> children;
};
moodycamel::BlockingConcurrentQueue<pendingInboundMessage> pendingInbound;

std::string _mg_str_to_stdstring(mg_str x) {
  return std::string(x.buf, x.buf + x.len);
}


struct adminhandlerMessage {
  int flag = 0; // 1 - broadcast, 2 - halt all connections, 4 - connection closed,
  unsigned long conn_id;  // Parent connection ID
  std::string message;
};

struct wakeupCall {
  int flag = 0; // 1 - WS, 2 - WS close conn
  std::string message;
};

std::map<unsigned long, mg_mgr*> adminConnections;
std::mutex adminConnections_mutex;
moodycamel::BlockingConcurrentQueue<adminhandlerMessage> adminConsoleHandler_queue;
int adminConsoleHandler_running = 0;
void woker(mg_mgr* const& mgr, const unsigned long conn, const wakeupCall& s) {
  wakeupCall* resp = new wakeupCall {s};
  mg_wakeup(mgr, conn, &resp, sizeof(resp));
}

struct workerInfo {
  mg_mgr* mgr;
  uint64_t contime, uptime, latency;
  std::vector<int> attachedWorkunits;
};
std::map<unsigned long, workerInfo> workerConnections;
std::mutex workerConnections_mutex;

void workunitHandler() {
  // every T seconds clear out B2A and regenerate A2B node
  pendingInboundMessage nx;
  int idx = 0, maxcnt = 1000;
  while(/* */) {
    // process pendingInbound but process at most 1000 to ensure that pendingOutbound can be refreshed
    // only acquire the lock for a shot amount of time
    std::vector<int> addPendingOutbound;
    std::vector<pendingInboundMessage> to_process(1000);
    to_process.resize(pendingInbound.try_dequeue_bulk(to_process.begin(), maxcnt));
    { const std::lock_guard<std::mutex> lock(searchtree_mutex);
      for(auto x:to_process) {
        if(x.completed) {
          // ...
        } else
          addPendingOutbound.push_back(x.id);
      }
    }
    { const std::lock_guard<std::mutex> lock(pendingOutbound_mutex);
      for(int i:addPendingOutbound)
        pendingOutbound.push(i);
    }
    if((++idx) % 256 == 0) {
      std::stringstream res;
      /* output stats */
      adminConsoleHandler_queue.enqueue({1, 0, res.str()});
    }
  }
}

// note adminConsoleHandler can block thread during runs
void adminConsoleHandler() {
  assert(adminConsoleHandler_running == 1);
  // MG_INFO(("Admin console handler started running"));
  adminhandlerMessage nx;
  while (adminConsoleHandler_queue.wait_dequeue(nx), nx.flag != 2) {
    MG_INFO(("handling %d - %s", nx.conn_id, nx.message.c_str()));
    if(nx.flag == 1) {
      const std::lock_guard<std::mutex> lock(adminConnections_mutex);
      std::cerr << "broadcasting to "<<adminConnections.size()<<" hosts\n";
      for(auto& [conn, mgr] : adminConnections)
        woker(mgr, conn, {1, nx.message});
      continue;
    }
    if(nx.flag == 4) {
      // MG_INFO(("\033[1;1;31m## connection close handler triggered ## \033[0m"));
      const std::lock_guard<std::mutex> lock(adminConnections_mutex);
      auto it =  adminConnections.find(nx.conn_id);
      if(it != adminConnections.end())
        adminConnections.erase(it);
      // MG_INFO(("remaining conns: %llu", adminConnections.size()));
      continue;
    }
    // normal
    mg_mgr* mgr;
    {
      const std::lock_guard<std::mutex> lock(adminConnections_mutex);
      mgr = adminConnections[nx.conn_id];
    }
    std::stringstream s(nx.message);
    std::string com; s>>com;
    if(com == "load") {
      woker(mgr, nx.conn_id, {1, "Loading dump.txt..."});
      sleep(2);
      woker(mgr, nx.conn_id, {1, "yee"});
    } else if(com == "bcast") {
      woker(mgr, nx.conn_id, {1, "Broadcasting..."});
      adminConsoleHandler_queue.enqueue({1, 0, nx.message});
    } else if(nx.message == "remoteclose") {
      woker(mgr, nx.conn_id, {3, "bye"});
    } else if(nx.message == "conn-stats") {
      std::stringstream f;
      std::vector<uint64_t> latencies, uptimes;
      { const std::lock_guard<std::mutex> lock(workerConnections_mutex);
        for(auto& [conn, m]:workerConnections) latencies.push_back(m.latency), uptimes.push_back(m.uptime);
      }
      f << latencies.size() << " connections<br/>latencies:";
      for(auto i: latencies) f<<" "<<i<<"ms";
      f<< "<br/>uptimes:";
      f<<std::setprecision(1);
      for(auto i: uptimes) f<<" "<<i/60000.0<<"m";
      woker(mgr, nx.conn_id, {1, f.str()});
    } else {
      woker(mgr, nx.conn_id, {1, "huh?"});
    }
  }
  // halt all connections
  { const std::lock_guard<std::mutex> lock(adminConnections_mutex);
    // hmm does this trigger nx flag 4 ...?
    adminConnections.clear();
  }
  adminConsoleHandler_running = false;
}

struct workerhandlerMessage {
  int flag = 0; // 4 - connection closed, 8 - ping time, 16 - attach workunit
  unsigned long conn_id;  // Parent connection ID
  std::string message;
  uint64_t ms = 0;
};

moodycamel::BlockingConcurrentQueue<workerhandlerMessage> workerHandler_queue;
int workerHandler_running = 0;
// note workerHandler can **NOT** block thread during runs because it has to deal with TEN THOUSAND CONNECTIONS
// role of worker handler is just to ping
void workerHandler() {
  assert(workerHandler_running == 1);
  MG_INFO(("Worker handler started running"));
  workerhandlerMessage nx;
  std::set<unsigned long> pingUnresponded;
  auto postWorkerDisconnect = [&](unsigned long conn) {
    for(int i:workerConnections[conn].attachedWorkunits)
      pendingInbound.enqueue({i, 0, "", {}});
  };
  uint64_t lastping = 0;
  while (workerHandler_queue.wait_dequeue(nx), nx.flag != 2) {
    // MG_INFO(("handling %d - %s", nx.conn_id, nx.message.c_str()));
    if(nx.flag == 4) {
      // **ALL CLOSING WORKER CONNECTIONS, WHETHER BY FAILING A PING OR BY DISCONNECTING AND TRIGGERING WEBSOCKET CONTROL, SHALL PASS THROUGH THIS FUNCTION.**
      MG_INFO(("\033[1;1;31m## worker connection %d close handler triggered ## \033[0m", nx.conn_id));
      const std::lock_guard<std::mutex> lock(workerConnections_mutex);
      auto it = workerConnections.find(nx.conn_id);
      if(it != workerConnections.end()) {
        postWorkerDisconnect(nx.conn_id);
        workerConnections.erase(it);
        auto it2 = pingUnresponded.find(nx.conn_id);
        if(it2 != pingUnresponded.end())
          pingUnresponded.erase(it2);
      } else
        MG_INFO(("tried to release already released worker %d", nx.conn_id));
      continue;
    }
    if(nx.flag == 8) {
      // disconnect hosts that did not respond to last ping...
      for(auto conn : pingUnresponded)
        workerHandler_queue.enqueue({4, conn, ""});
      pingUnresponded.clear();
      const std::lock_guard<std::mutex> lock(workerConnections_mutex);
      for(auto& [conn, m] : workerConnections)
        woker(m.mgr, conn, {1, "ping"}), pingUnresponded.insert(conn);
      lastping = nx.ms;
      continue;
    }
    if(nx.flag == 16) {
      const std::lock_guard<std::mutex> lock(workerConnections_mutex);
      workerConnections[nx.conn_id].attachedWorkunits.push_back(nx.ms); // todo less naughtily reuse this
      continue;
    }
    std::stringstream s(nx.message);
    std::string com; s>>com;
    if(com == "pong") {
      auto it = pingUnresponded.find(nx.conn_id);
      if(it != pingUnresponded.end())
        pingUnresponded.erase(it);
      { const std::lock_guard<std::mutex> lock(workerConnections_mutex); 
        auto& m = workerConnections[nx.conn_id];
        m.latency = nx.ms - lastping;
        m.uptime = nx.ms - m.contime;
      }
    }
  }
  // halt all connections
  { const std::lock_guard<std::mutex> lock(workerConnections_mutex);
    for(auto & [conn, mgr] : workerConnections)
      postWorkerDisconnect(conn);
    workerConnections.clear();
  }
  workerHandler_running = false;
}

// HTTP request callback
void fn(struct mg_connection* c, int ev, void* ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    mg_http_message* hm = (mg_http_message*) ev_data;
    /* admin end */
    if(mg_match(hm->uri, mg_str("/"), NULL)) {
      mg_http_reply(c, 200, "Content-Type: text/raw\n", "welcome\n");
    } else if(mg_match(hm->uri, mg_str("/admin"), NULL)) {
      mg_http_serve_opts opts = {.root_dir=".", .ssi_pattern=NULL, .extra_headers=NULL, .mime_types="html=text/html", .page404=NULL, .fs=NULL};
      mg_http_serve_file(c, hm, "admin.html", &opts);
    } else if(mg_match(hm->uri, mg_str("/admin-websocket"), NULL)) {
      mg_ws_upgrade(c, hm, NULL);
      c->data[0] = 'W'; // websocket
      c->data[1] = 'A'; // admin
      { const std::lock_guard<std::mutex> lock(adminConnections_mutex);
        adminConnections[c->id] = c->mgr;
      }
      if(adminConsoleHandler_running == 0) {
        adminConsoleHandler_running++;
        std::thread t(adminConsoleHandler); t.detach();
      }
    }
    /* connected units end*/
    else if (mg_match(hm->uri, mg_str("/worker-websocket"), NULL)) {
      mg_ws_upgrade(c, hm, NULL);
      c->data[0] = 'W'; // websocket
      c->data[1] = 'W'; // worker
      { const std::lock_guard<std::mutex> lock(workerConnections_mutex);
        workerConnections[c->id] = {c->mgr, mg_millis(), 0, 0, {}};
      }
      if(workerHandler_running == 0) {
        workerHandler_running++;
        std::thread t(workerHandler); t.detach();
      }
    } else if(mg_match(hm->uri, mg_str("/getconfig"), NULL)) {
      // GET
      // v2: merge with /getwork
      std::stringstream res;
      res << p << ' ' << width << ' ' << sym << ' ' << l4h << ' ' << maxwid << ' ' << stator << ' ';
      res << filters.size(); for(auto i:filters) res << ' ' << i;
      for(int s=0;s<2; s++){
        res<<' '<<leftborder[s].size();
        for(auto i:leftborder[s]) res<<' '<<i;
      }
      res<<'\n';
      mg_http_reply(c, 200, "Content-Type: text/raw\n", "%s", res.str().c_str());
    } 
    else if(mg_match(hm->uri, mg_str("/getwork"), NULL)) {
      // POST request, should contain three parameters: websocket id & ephemerality & amount
      // returns amount * [workunit identifier]
      std::stringstream co(_mg_str_to_stdstring(hm->body));
      std::stringstream res;
      bool ephemeral;
      co >> ephemeral;
      if(!ephemeral) {
        // TODO. not yet implemented
        res << "0\n";
      } else {
        const std::lock_guard<std::mutex> lock(pendingOutbound_mutex);
        int amnt; co >> amnt;
        int cnt = std::min(amnt, (int)pendingOutbound.size());
        res << cnt<<'\n';
        for(int i=0; i<cnt; i++) {
          int onx = pendingOutbound.front(); pendingOutbound.pop();
          res<<onx<<' '<<tree[onx].depth%p;
          for(uint64_t x:getState(onx)) res<<' '<<x;
          res << '\n';
        }
        mg_http_reply(c, 200, "Content-Type: text/raw\n", "%s", res.str().c_str());
      }
    } else if(mg_match(hm->uri, mg_str("/returnwork"), NULL)) {
      // POST request, should contain workunit id, status - 0 or 1
      // if status is 1, further contain contributor id and all children
      std::stringstream co(_mg_str_to_stdstring(hm->body));
      int id, status;
      co >> id >> status;
      if(status == 0)
        pendingInbound.enqueue({id, 0, "", {}});
      else {
        std::string cid; co >> cid;
        int ch; co >> ch;
        std::vector<uint64_t> rows(ch);
        for(int i=0; i<ch; i++) co >> rows[i];
        pendingInbound.enqueue({id, 1, cid, rows});
      }
      mg_http_reply(c, 200, "Content-Type: text/raw\n", "OK");
    } 
    /* ?? */
    else {
      MG_INFO(("unknown endpoint %.*s", hm->uri.len, hm->uri.buf));
      mg_http_reply(c, 404, "Content-Type: text/raw\n", "what\n");
    }
  } else if(ev == MG_EV_OPEN) {
    MG_INFO(("open triggered"));
  } else if(ev == MG_EV_CLOSE) {
    MG_INFO(("close triggered, %d", c->id));
  } else if(ev == MG_EV_WS_CTL) {
    mg_ws_message* wm = (mg_ws_message*) ev_data;
    uint32_t s = (wm->flags) & 0xF0;
    MG_INFO(("\033[1;1;31mwebsocket control triggered, %.*s, %x \033[0m", wm->data.len, wm->data.buf, s));
    if(s == 0x80) {
      if(c->data[1] == 'W')
        workerHandler_queue.enqueue({4, c->id, ""});
      else if(c->data[1] == 'A')
        adminConsoleHandler_queue.enqueue({4, c->id, ""});
    }
  } else if(ev == MG_EV_WS_MSG) {
    mg_ws_message* wm = (mg_ws_message*) ev_data;
    if(c->data[1] == 'W') {
      workerHandler_queue.enqueue({0, c->id, _mg_str_to_stdstring(wm->data), mg_millis()});
    } else if(c->data[1] == 'A') {
      adminConsoleHandler_queue.enqueue({0, c->id, _mg_str_to_stdstring(wm->data)});
    }
  } else if (ev == MG_EV_WAKEUP) {
    // struct mg_str *data = (struct mg_str *) ev_data;
    wakeupCall* data = * (wakeupCall**) ((mg_str*) ev_data)->buf;
    if(data->flag & 1) {
      auto sv = mg_str(data->message.c_str());
      if(sv.len)
        mg_ws_send(c, sv.buf, sv.len, WEBSOCKET_OP_TEXT);
      if(data->flag & 2) {
        mg_ws_send(c, NULL, 0, WEBSOCKET_OP_CLOSE);
      }
    } else {
      mg_http_reply(c, 200, "", "Result: %s\n", data->message.c_str());
    }
    delete data;
  } else if (ev == MG_EV_POLL) {
    static uint64_t lastping = 0, current = 0;
    if((current = mg_millis()) > lastping + worker_ping_duration) {
      lastping = current;
      if(workerHandler_running)
        workerHandler_queue.enqueue({8, 0, "", lastping});
    }
  }
}

int main(void) {
  mg_mgr mgr;
  mg_mgr_init(&mgr);        // Initialise event manager
  mg_log_set(MG_LL_INFO); 
  mg_http_listen(&mgr, "http://localhost:8000", fn, NULL);  // Create listener
  mg_wakeup_init(&mgr);  // Initialise wakeup socket pair
  for (;;) {             // Event loop
    mg_mgr_poll(&mgr, 1000);
  }
  mg_mgr_free(&mgr);
  return 0;
}