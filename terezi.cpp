// TODO: why do nodes swallow & forget about their work units?

#include "mongoose.h"

#include <bits/stdc++.h>

#include "cqueue/bcq.h"

int worker_ping_duration = 8000; // ms

int p, width, sym, l4h;
int maxwid, stator;

namespace _searchtree {
std::mutex searchtree_mutex;
// state: d - done, q - not done
// TODO: for non-ephemeral nodes, include state s - sent to NE node
struct node { uint64_t row; int depth, shift, parent, contrib; char state; };
node* tree = new node[16777216];
int treeSize = 0, treeAlloc = 16777216;
std::vector<uint64_t> exInitrow;
std::vector<uint64_t> filters;
std::vector<uint64_t> leftborder[2];
std::vector<std::string> contributors; std::map<std::string, int> contributorIDs;

void dumpTree(std::string fn) {
  std::ofstream fx(fn);
  fx << p << ' ' << width << ' ' << sym << ' ' << stator << ' ' << l4h << '\n';
  fx << exInitrow.size(); for(auto s:exInitrow){fx << ' ' << s;} fx << '\n';
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
  fx>>p>>width>>sym>>stator>>l4h;
  int FS; fx>>FS; exInitrow=std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)fx>>exInitrow[i];
  fx>>FS; filters=std::vector<uint64_t>(FS);
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
  // std::cerr << "called newnode " << treeSize << '\n';
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

std::string banner = "Welcome to Scourge v2.1\n";
std::string longgestPartial;
std::string oscillatorComplete;

// std::mutex pendingOutbound_mutex;
// std::queue<int> pendingOutbound;
// ints store these and lock the tree instead because all the nodes will be queued here
// TODO: refactor into pendingoutbound containing only a portion of the unprocessed nodes, will make getwork unlocking
struct pendingOutboundMessage {
  int id, depth;
  std::vector<uint64_t> rows;
};
pendingOutboundMessage genPOM(int onx) {
  return pendingOutboundMessage{onx, tree[onx].depth, getState(onx)};
};
moodycamel::BlockingConcurrentQueue<pendingOutboundMessage> pendingOutbound;

struct pendingInboundMessage {
  int id; unsigned long cid;
  char state; // u - uncomplete, c - complete, s - sent
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

void emit(int state, bool d = true){
  static int bdep = 0;
  std::stringstream fxx;
  using namespace std;
  vector<uint64_t> mat;
  while(state != -1)
    mat.push_back(tree[state].row), state = tree[state].parent;
  reverse(mat.begin(), mat.end());
  if(!d) {
    if(((int)(mat.size())) <= bdep) return;
    bdep = mat.size();
  }else
    fxx<<"<br>[[OSC1LL4TOR COMPL3T3!!!]]<br>"<<endl;
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
  fxx << "<pre style='white-space: pre-wrap; word-break:break-all'><code>";
  fxx << "x = 0, y = 0, rule = B3/S23\n" + rle + "!</code></pre>" << endl;
  if(d) {
    oscillatorComplete += fxx.str();
    std::ofstream fx("osc-complete");
    fx<<oscillatorComplete<<std::endl; fx.flush(); fx.close();
  }
  else longgestPartial = fxx.str();
  adminConsoleHandler_queue.enqueue({1, 0, fxx.str()});
}

struct workerInfo {
  mg_mgr* mgr;
  uint64_t contime, uptime, latency;
  std::set<int> attachedWorkunits;
};
std::map<unsigned long, workerInfo> workerConnections;
std::mutex workerConnections_mutex;

void workunitHandler() {
  // every T seconds clear out B2A and regenerate A2B node
  std::string autosave1 = "autosave-odd.txt", autosave2 = "autosave-even.txt";
  pendingInboundMessage nx;
  int idx = 0, maxcnt = 1000;
  while(1) {
    std::this_thread::sleep_for(1 * std::chrono::seconds(1));
    // process pendingInbound but process at most 1000 to ensure that pendingOutbound can be refreshed
    // only acquire the lock for a shot amount of time
    std::vector<pendingOutboundMessage> addPendingOutbound;
    std::vector<pendingInboundMessage> to_process(1000);
    to_process.resize(pendingInbound.try_dequeue_bulk(to_process.begin(), maxcnt));
    { const std::lock_guard<std::mutex> lock(searchtree_mutex);
      const std::lock_guard<std::mutex> lock2(workerConnections_mutex);
      for(auto& x:to_process) {
        // std::cerr << x.id << ' ' <<x.cid << ' ' <<x.state << ' ' <<x.children.size() << '\n';
        // if(x.children.size() > 1000) {
        //   std::cerr << "preposterous\n";
        //   assert(0);
        // }
        if(tree[x.id].state == 'd') continue;
        if(x.state == 'c') {
          tree[x.id].state = 'd';
          if(!contributorIDs.contains(x.contributor)) {
            contributorIDs[x.contributor] = contributors.size();
            contributors.push_back(x.contributor);
          }
          tree[x.id].contrib = contributorIDs[x.contributor];
          for(uint64_t r:x.children) {
            // std::cerr << "requested newNode with "<< to_process.size() << ' ' << x.id << ' ' << x.cid << ' ' <<r<<' ' <<x.children.size()<<std::endl;
            int onx = newNode();
            tree[onx] = {r, tree[x.id].depth+1, 0, x.id, 0, 'q'};
            addPendingOutbound.push_back(genPOM(onx));
            emit(onx, Compl3t34bl3(getState(onx), tree[onx].depth, tree[onx].depth%p));
          }
        } else if(x.state == 'u') {
          addPendingOutbound.push_back(genPOM(x.id));
        }
        
        if(x.state == 'c' || x.state == 'u') {
          if(workerConnections.find(x.cid) != workerConnections.end())
            if(workerConnections[x.cid].attachedWorkunits.contains(x.id))
              workerConnections[x.cid].attachedWorkunits.erase(x.id);
        } else {
          // note that only workerhandler is allowed to delete from workerconnections
          // so if we somehow sent to a deleted connection, recycle it immediately
          if(workerConnections.find(x.cid) != workerConnections.end())
            workerConnections[x.cid].attachedWorkunits.insert(x.id);
          else addPendingOutbound.push_back(genPOM(x.id));
        }
      }
    }
    pendingOutbound.enqueue_bulk(addPendingOutbound.begin(), addPendingOutbound.size());
    // { const std::lock_guard<std::mutex> lock(pendingOutbound_mutex);
    //   for(int i:addPendingOutbound)
    //     pendingOutbound.enqueue(i);
    // }
    if((++idx) % 256 == 0) {
      const std::lock_guard<std::mutex> lock(searchtree_mutex);
      const std::lock_guard<std::mutex> lock2(workerConnections_mutex);
      std::stringstream res;
      auto now_tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
      res << std::put_time(std::gmtime(&now_tt), "%c %Z") << " conn#: " << workerConnections.size();
      res << " q#: "<<pendingOutbound.size_approx() << " pending#: "<<pendingInbound.size_approx();
      adminConsoleHandler_queue.enqueue({1, 0, res.str()});
      if(idx % 1024 == 0) {
        dumpTree(autosave1);
        swap(autosave1, autosave2);
        std::stringstream res2; res2 << "autosaved to "<<autosave1;
        adminConsoleHandler_queue.enqueue({1, 0, res2.str()});
      }
    }
  }
}

std::thread WUhandlerthread(workunitHandler);

// note adminConsoleHandler can block thread during runs
void adminConsoleHandler() {
  assert(adminConsoleHandler_running == 1);
  // MG_INFO(("Admin console handler started running"));
  adminhandlerMessage nx;
  while (adminConsoleHandler_queue.wait_dequeue(nx), nx.flag != 2) {
    MG_INFO(("handling %d - %s", nx.conn_id, nx.message.c_str()));
    if(nx.flag == 1) {
      const std::lock_guard<std::mutex> lock(adminConnections_mutex);
      // std::cerr << "broadcasting to "<<adminConnections.size()<<" hosts\n";
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
    mg_mgr* mgr;
    { const std::lock_guard<std::mutex> lock(adminConnections_mutex);
      mgr = adminConnections[nx.conn_id];
    }
    std::stringstream s(nx.message);
    std::string com; s>>com;
    if(com == "help") {
      woker(mgr, nx.conn_id, {1, "commands: remoteclose connstats init loadsave dumpsave treestats"});
    } else if(com == "remoteclose") {
      woker(mgr, nx.conn_id, {3, "bye"});
    } else if(com == "connstats") {
      std::stringstream f;
      std::vector<uint64_t> latencies, uptimes;
      { const std::lock_guard<std::mutex> lock(workerConnections_mutex);
        for(auto& [conn, m]:workerConnections) latencies.push_back(m.latency), uptimes.push_back(m.uptime);
      }
      f << latencies.size() << " connections<br/>latencies:";
      for(auto i: latencies) f<<" "<<i<<"ms";
      f<< "<br/>uptimes:";
      f<<std::fixed<<std::setprecision(1);
      for(auto i: uptimes) f<<" "<<i/60000.0<<"m";
      woker(mgr, nx.conn_id, {1, f.str()});
    } else if(com == "init") {
      std::ifstream _f("initform.html");
      std::stringstream f; f << _f.rdbuf();
      woker(mgr, nx.conn_id, {1, f.str()});
    } else if(com == "actual-init") {
      const std::lock_guard<std::mutex> lock(searchtree_mutex);
      std::string options;
      if(treeSize != 0) {
        woker(mgr, nx.conn_id, {1, "failed, tree is already initialized"});
        goto fail;
      }
      // TODO: assumes stator = 0
      s >> p >> width >> sym >> l4h;
      if(sym != 0 && sym != 1) {
        woker(mgr, nx.conn_id, {1, "failed, unsupported symmetry"});
        continue;
      }
      for (int i = 0; i < 2 * p; i++) {
        uint64_t x = 0; std::string row;
        s >> row;
        if(sz(row) != width)  {
          woker(mgr, nx.conn_id, {1, "failed, bad row length"});
          goto fail;
        }
        for (int j = 0; j < width; j++)
          if (row[j] == 'o')
            x |= (1ull << j);
// struct node { uint64_t row; int depth, shift, parent, contrib; char state; };
        tree[newNode()]={x, i+1, 0, i-1, 0, 'd'};
      } s>>options;
      if (options[0] == 'y' || options[0] == 'Y') {
        int filterrows; s>>filterrows;
        for (int i = 0; i < filterrows; i++) {
          uint64_t x = 0; std::string filter; s >> filter;
          if(sz(filter) != width) {
            woker(mgr, nx.conn_id, {1, "failed, bad filter length, must be w"});
            goto fail;
          }
          for (int j = 0; j < width; j++)
            if (filter[j] == 'o')
              x |= (1ull << j);
          filters.push_back(x);
        }
        for(int idx=0; idx<2; idx++){
          int cnt; s>>cnt;
          leftborder[idx] = std::vector<uint64_t>(cnt);
          for(int i=0; i<cnt; i++){
            std::string t; s>>t;
            if(sz(t) != p)  {
              woker(mgr, nx.conn_id, {1, "failed, bad fix length, must be p"});
              goto fail;
            }
            for(int j=0; j<p; j++) if(t[j] == 'o') leftborder[idx][i] |= (1ull << j);
          }
        }
        exInitrow = std::vector<uint64_t>(2*p);
        for(int i=0; i<2*p; i++) {
          uint64_t x = 0; std::string row;
          s >> row;
          if(sz(row) != width)  {
            woker(mgr, nx.conn_id, {1, "failed, bad row length"});
            goto fail;
          }
          for (int j = 0; j < width; j++)
            if (row[j] == 'o')
              x |= (1ull << j);
          exInitrow[i] = x;
        }
      }
      assert(treeSize == 2*p);
      tree[2*p-1].state = 'q';
      pendingOutbound.enqueue(genPOM(2*p-1));
      if(s.fail()) {
        treeSize = 0;
        woker(mgr, nx.conn_id, {1, "unknown parsing failure"});
        goto fail;
      }
      woker(mgr, nx.conn_id, {1, "successfully parsed input:\n<pre><code>" + nx.message.substr(12)+"</code></pre>"});
      fail:;
    } else if (com == "loadsave") {
      const std::lock_guard<std::mutex> lock(searchtree_mutex);
      if(treeSize != 0) {
        woker(mgr, nx.conn_id, {1, "failed, tree is already initialized"});
      } else {
        std::string fn; s>>fn;
        loadTree(fn);
        int cnt = 0;
        for(int i=0; i<treeSize; i++)
          if(tree[i].state == 'q') {
            cnt++;
            pendingOutbound.enqueue(genPOM(i));
          }
        woker(mgr, nx.conn_id, {1, "loaded " + std::to_string(treeSize) + " nodes from "+fn+
        " and queued "+std::to_string(cnt)+" nodes"});
      }
    } else if(com == "dumpsave") {
      const std::lock_guard<std::mutex> lock(searchtree_mutex);
      std::string fn; s>>fn;
      dumpTree(fn);
      // TODO: NE node process here
      woker(mgr, nx.conn_id, {1, "saved " + std::to_string(treeSize) + " nodes to "+fn});
    } else if(com == "treestats") {
      woker(mgr, nx.conn_id, {1, "crunching tree stats..."});
      const std::lock_guard<std::mutex> lock(searchtree_mutex);
      std::stringstream fx;
      int maxdep = 0;
      std::vector<std::pair<int, std::string>> conIdx;
      for(auto s:contributors) conIdx.push_back({0, s});
      for(int i=0; i<treeSize; i++) {
        maxdep = std::max(maxdep, tree[i].depth);
      }
      std::vector<int> ongoing(maxdep+1), total(maxdep+1);
      for(int i=0;i<treeSize; i++) {
        total[tree[i].depth]++;
        if(tree[i].state != 'd') ongoing[tree[i].depth]++;
        else if(i>=2*p) conIdx[tree[i].contrib].first++;
      }
      fx << treeSize << " nodes<br>";
      fx << "max depth "<<maxdep <<"<br>";
      fx << "profile";
      for(int i=0; i<=maxdep; i++){
        if(ongoing[i])fx<<' '<<ongoing[i]<<'/'<<total[i];
        else fx<<' '<<total[i];
      }
      fx<<"<br>contributors<br>";
      std::sort(conIdx.begin(), conIdx.end()); std::reverse(conIdx.begin(), conIdx.end());
      for(auto& [cnt, id]:conIdx)fx<<id<<": "<<cnt<<" nodes<br>";
      fx<<longgestPartial;
      if(oscillatorComplete.size())fx<<oscillatorComplete;
      woker(mgr, nx.conn_id, {1, fx.str()});
    }
    else {
      woker(mgr, nx.conn_id, {1, "huh?"});
    }
  }
  // halt all connections
  { const std::lock_guard<std::mutex> lock(adminConnections_mutex);
    // TODO. lmao.
    // hmm does this trigger nx flag 4 ...?
    adminConnections.clear();
  }
  adminConsoleHandler_running = false;
}

struct workerhandlerMessage {
  int flag = 0; // 4 - connection closed, 8 - ping time
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
    for(int i:workerConnections[conn].attachedWorkunits) // this is done with a lock on it anyways
      pendingInbound.enqueue({i, conn, 'u', "", {}});
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

mg_str ca_cert, server_cert, server_key;

// HTTP request callback
void fn(mg_connection* c, int ev, void* ev_data) {
  // if (ev == MG_EV_ACCEPT) {
  //   mg_tls_opts opts = {.ca = ca_cert,
  //                       .cert = server_cert,
  //                       .key = server_key};
  //   mg_tls_init(c, &opts);
  // } else 
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
      auto sv = mg_str(banner.c_str());
      mg_ws_send(c, sv.buf, sv.len, WEBSOCKET_OP_TEXT);
      if(longgestPartial.size()) {
        sv = mg_str(longgestPartial.c_str());
        mg_ws_send(c, sv.buf, sv.len, WEBSOCKET_OP_TEXT);
      }
      if(oscillatorComplete.size()) {
        sv = mg_str(oscillatorComplete.c_str());
        mg_ws_send(c, sv.buf, sv.len, WEBSOCKET_OP_TEXT);
      }
    }
    /* connected units end*/
    else if (mg_match(hm->uri, mg_str("/worker-websocket"), NULL)) {
      mg_ws_upgrade(c, hm, NULL);
      c->data[0] = 'W'; // websocket
      c->data[1] = 'W'; // worker
      auto sv = mg_str(std::to_string(c->id).c_str());
      mg_ws_send(c, sv.buf, sv.len, WEBSOCKET_OP_TEXT);
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
      res << exInitrow.size(); for(auto i:exInitrow) res << ' ' << i;
      res << ' ' << filters.size(); for(auto i:filters) res << ' ' << i;
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
      unsigned long wsid=0;
      co >> wsid;
      int ephemeral=0;
      co >> ephemeral;
      if(!ephemeral) {
        // TODO. not yet implemented
        res << "0\n";
      } else {
        int amnt=0; co >> amnt;
        std::vector<pendingOutboundMessage> nodes(amnt);
        nodes.resize(pendingOutbound.try_dequeue_bulk(nodes.begin(), amnt));
        // int cnt = std::min(amnt, (int)pendingOutbound.size());
        res << nodes.size()<<'\n';
        for(auto& pom:nodes) {
          pendingInbound.enqueue({pom.id, wsid, 's', "", {}});
          res<<pom.id<<' '<<pom.depth;
          for(uint64_t x:pom.rows) res<<' '<<x;
          res << '\n';
        }
      }
      assert(!co.fail());
      mg_http_reply(c, 200, "Content-Type: text/raw\n", "%s", res.str().c_str());
    } else if(mg_match(hm->uri, mg_str("/returnwork"), NULL)) {
      // POST request, should contain amount, websocket connection id
      // then amount times the following: workunit id, status - 0 or 1
      // if status is 1, further contain contributor id and all children
      std::stringstream co(_mg_str_to_stdstring(hm->body));
      int amnt; co >> amnt;
      unsigned long wsid; co >> wsid;
      for(int i=0; i<amnt; i++) {
        int id=0, status=0;
        co >> id >> status;
        if(status == 0)
          pendingInbound.enqueue({id, wsid, 'u', "", {}});
        else {
          std::string cid; co >> cid;
          int ch; co >> ch;
          std::vector<uint64_t> rows(ch);
          for(int i=0; i<ch; i++) co >> rows[i];
          pendingInbound.enqueue({id, wsid, 'c', cid, rows});
        }
      }
      if(co.fail())
        mg_http_reply(c, 200, "Content-Type: text/raw\n", "FAIL");
      else
        mg_http_reply(c, 200, "Content-Type: text/raw\n", "OK");
    } 
    /* ?? */
    else {
      MG_INFO(("unknown endpoint %.*s", hm->uri.len, hm->uri.buf));
      mg_http_reply(c, 404, "Content-Type: text/raw\n", "what\n");
    }
  } else if(ev == MG_EV_OPEN) {
    // MG_INFO(("open triggered"));
  } else if(ev == MG_EV_CLOSE) {
    // MG_INFO(("close triggered, %d", c->id));
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

int main(int argc, const char* argv[]) {
  mg_mgr mgr;
  // ca_cert = mg_file_read(&mg_fs_posix, "certs/ca.crt");
  // server_cert = mg_file_read(&mg_fs_posix, "certs/server.crt");
  // server_key = mg_file_read(&mg_fs_posix, "certs/server.key");
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