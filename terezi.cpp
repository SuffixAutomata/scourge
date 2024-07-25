#include "mongoose.h"

#include <bits/stdc++.h>

#include "cqueue/bcq.h"

int p, width, sym, l4h;
int maxwid, stator;

namespace _searchtree {
int depths[1000], total[1000];
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

void flushTree(node* dest = tree) { assert(0); }

int newNode() {
  if(treeSize == treeAlloc)
    treeAlloc *= 2, flushTree(new node[treeAlloc]);
  // if((treeSize - time(0)) % 8192 == 0) flushTree();
  tree[treeSize] = {0,0,0,0,'0'};
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
bool adminConsoleHandler_running = false;
void adminConsoleHandler() {
  assert(!adminConsoleHandler_running);
  adminConsoleHandler_running = true;
  // MG_INFO(("Admin console handler started running"));
  adminhandlerMessage nx;
  while (adminConsoleHandler_queue.wait_dequeue(nx), nx.flag != 2) {
    MG_INFO(("handling %d - %s", nx.conn_id, nx.message.c_str()));
    if(nx.flag == 1) {
      const std::lock_guard<std::mutex> lock(adminConnections_mutex);
      std::cerr << "broadcasting to "<<adminConnections.size()<<" hosts\n";
      for(auto& [conn, mgr] : adminConnections){
        wakeupCall* resp = new wakeupCall {1, nx.message};
        mg_wakeup(mgr, conn, &resp, sizeof(resp));
      }
      continue;
    }
    if(nx.flag == 4) {
      // MG_INFO(("\033[1;1;31m## connection close handler triggered ## \033[0m"));
      const std::lock_guard<std::mutex> lock(adminConnections_mutex);
      adminConnections.erase(adminConnections.find(nx.conn_id));
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
      wakeupCall* resp = new wakeupCall {1, "Loading dump.txt"};
      mg_wakeup(mgr, nx.conn_id, &resp, sizeof(resp));
    } else if(com == "bcast") {
      wakeupCall* resp = new wakeupCall {1, "Broadcasting..."};
      mg_wakeup(mgr, nx.conn_id, &resp, sizeof(resp));
      adminConsoleHandler_queue.enqueue({1, 0, nx.message});
    } else if(nx.message == "remoteclose") {
      wakeupCall* resp = new wakeupCall {3, "bye"};
      mg_wakeup(mgr, nx.conn_id, &resp, sizeof(resp));
    } else {
      wakeupCall* resp = new wakeupCall {1, "huh?"};
      mg_wakeup(mgr, nx.conn_id, &resp, sizeof(resp));
    }
  }
  // halt all connections
  {
    const std::lock_guard<std::mutex> lock(adminConnections_mutex);
    // hmm does this trigger nx flag 4 ...?
    adminConnections.clear();
  }
  adminConsoleHandler_running = false;
}

struct workerhandlerMessage {
  int flag = 0; // 1 - broadcast, 2 - halt all connections, 4 - connection closed, 8 - ping time
  unsigned long conn_id;  // Parent connection ID
  std::string message;
};

std::map<unsigned long, mg_mgr*> workerConnections;
std::mutex workerConnections_mutex;
moodycamel::BlockingConcurrentQueue<workerhandlerMessage> workerHandler_queue;
bool workerHandler_running = false;
void workerHandler() {
  assert(!workerHandler_running);
  workerHandler_running = true;
  // MG_INFO(("Admin console handler started running"));
  workerhandlerMessage nx;
  while (workerHandler_queue.wait_dequeue(nx), nx.flag != 2) {
    MG_INFO(("handling %d - %s", nx.conn_id, nx.message.c_str()));
    if(nx.flag == 1) {
      const std::lock_guard<std::mutex> lock(workerConnections_mutex);
      std::cerr << "broadcasting to "<<workerConnections.size()<<" hosts\n";
      for(auto& [conn, mgr] : workerConnections){
        wakeupCall* resp = new wakeupCall {1, nx.message};
        mg_wakeup(mgr, conn, &resp, sizeof(resp));
      }
      continue;
    }
    if(nx.flag == 4) {
      // MG_INFO(("\033[1;1;31m## connection close handler triggered ## \033[0m"));
      const std::lock_guard<std::mutex> lock(workerConnections_mutex);
      workerConnections.erase(workerConnections.find(nx.conn_id));
      continue;
    }
    if(nx.flag == 8) {
      const std::lock_guard<std::mutex> lock(workerConnections_mutex);
      // disconnect hosts that did not respond to last ping...
      std::cerr << "pinging "<<workerConnections.size()<<" hosts\n";
      for(auto& [conn, mgr] : workerConnections){
        wakeupCall* resp = new wakeupCall {1, "ping"};
        mg_wakeup(mgr, conn, &resp, sizeof(resp));
      }
      continue;
    }
    // normal
    mg_mgr* mgr;
    {
      const std::lock_guard<std::mutex> lock(workerConnections_mutex);
      mgr = workerConnections[nx.conn_id];
    }
    std::stringstream s(nx.message);
    std::string com; s>>com;
    if(com == "disconnect") {
      wakeupCall* resp = new wakeupCall {3, "bye"};
      mg_wakeup(mgr, nx.conn_id, &resp, sizeof(resp));
      // release all workunits...
    } else {
      wakeupCall* resp = new wakeupCall {1, "huh?"};
      mg_wakeup(mgr, nx.conn_id, &resp, sizeof(resp));
    }
  }
  // halt all connections
  {
    const std::lock_guard<std::mutex> lock(workerConnections_mutex);
    // ...
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
      mg_http_serve_opts opts = {.mime_types = "html=text/html"};
      mg_http_serve_file(c, hm, "admin.html", &opts);
    } else if(mg_match(hm->uri, mg_str("/admin-websocket"), NULL)) {
      mg_ws_upgrade(c, hm, NULL);
      c->data[0] = 'W'; // websocket
      c->data[1] = 'A'; // admin
      {
        const std::lock_guard<std::mutex> lock(adminConnections_mutex);
        adminConnections[c->id] = c->mgr;
      }
      if(adminConsoleHandler_running == false) {
        std::thread t(adminConsoleHandler); t.detach();
      }
    }
    /* connected units end*/
    else if (mg_match(hm->uri, mg_str("/worker-websocket"), NULL)) {
      mg_ws_upgrade(c, hm, NULL);
      c->data[0] = 'W'; // websocket
      c->data[1] = 'W'; // worker
    }
    else if(mg_match(hm->uri, mg_str("/getwork"), NULL)) {
      // load from dynamic work queue...
    } else if(mg_match(hm->uri, mg_str("/returnwork"), NULL)) {
      // handle abandoning mechanism, etc...
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
    // what's this?
    if(s == 0x80) {
      adminConsoleHandler_queue.enqueue({4, c->id, ""});
    }
  } else if(ev == MG_EV_WS_MSG) {
    mg_ws_message* wm = (mg_ws_message*) ev_data;
    if(c->data[1] == 'W') {
      auto sv = mg_str("hey hi!!");
      mg_ws_send(c, sv.buf, sv.len, WEBSOCKET_OP_TEXT);
      // if(c->data[1] == '0') {
      //   // First message, initialization, should be of the form
      //   // [contributor name]&&[ephemeral - 0/1]&&[...]
      //   struct mg_str caps[4];
      //   int ephemeral;
      //   if (mg_match(wm->data, mg_str("#&&#&&#"), caps) &&
      //       mg_str_to_num(caps[1], 10, &ephemeral, sizeof(ephemeral)) &&
      //       (0 <= ephemeral && ephemeral <= 1)) {
      //     // establish connection
      //     c->data[1] = '1';
      //     // caps[0] holds `foo`, caps[1] holds `bar`.
      //   } else {
      //     // close connection
      //   }
      // }
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