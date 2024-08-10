// TODO: why do nodes swallow & forget about their work units?

#include "mongoose.h"

#include <bits/stdc++.h>

#include "cqueue/bcq.h"

const int worker_timeout_duration = 20000; // ms
const int worker_ping_rate = 10000; // ms
const int maxProcessedWUperSec = 1000000;

#include "logic.h"
#include "searchtree.h"
#include "cache.h"

// State, for admin
std::string banner = "Welcome to Scourge v2.3\n";
std::string longgestPartial;
std::string oscillatorComplete;

Cache pendingOutboundCache;
moodycamel::BlockingConcurrentQueue<int> pendingOutbound;

Cache pendingInboundCache;
struct pendingInboundMessage {
  // state: c - completed, u - unfinished, s - just sent
  int id; unsigned long cid; char state; std::string contributor;
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

void emit(std::vector<uint64_t> mat, bool compl3t3) {
  std::stringstream fxx;
  static int bdep = 0;
  if (!compl3t3) {
    if (sz(mat) <= bdep)
      return;
    bdep = std::max(bdep, sz(mat));
  } else
    INFO << "[[OSC1LL4TOR COMPL3T3!!!]]\n";
  std::string rle;
  int cnt = 0, cur = 0;
  auto f = [&](char x) {
    if (x != cur) {
      (cnt >= 2) ? rle += std::to_string(cnt) : "";
      (cnt >= 1) ? rle += (char)cur : "";
      cnt = 0, cur = x;
    }
    cnt++;
  };
  for (int r = 0; r * p < sz(mat); r++) {
    r ? f('$') : void();
    for (int x = r * p; x < sz(mat) && x < (r + 1) * p; x++) {
      for (int j = 0; j < width; j++)
        f("bo"[!!(mat[x] & (1ull << j))]);
      f('b'), f('b'), f('b');
    }
  }
  f('!');
  (compl3t3 ? INFO : STATUS) << "x = 0, y = 0, rule = B3/S23\n" + rle + '!' << '\n';
  fxx << "<pre style='white-space: pre-wrap; word-break:break-all'><code>";
  fxx << "x = 0, y = 0, rule = B3/S23\n" + rle + "!</code></pre>" << std::endl;
  if(compl3t3) {
    oscillatorComplete += fxx.str();
    std::ofstream fx("osc-complete");
    fx<<oscillatorComplete<<std::endl; fx.flush(); fx.close();
  }
  else longgestPartial = fxx.str();
  adminConsoleHandler_queue.enqueue({1, 0, fxx.str()});
}

struct workerInfo {
  uint64_t contime, uptime, latency;
  uint64_t lastmsg;
  std::set<int> attachedWorkunits;
};
std::map<unsigned long, workerInfo> workerConnections;
std::mutex workerConnections_mutex;
std::mutex searchtree_mutex;

void workunitHandler() {
  // every T seconds clear out B2A and regenerate A2B node
  std::string autosave1 = "autosave-odd.txt", autosave2 = "autosave-even.txt";
  pendingInboundMessage nx;
  int idx = 0;
  while(1) {
    std::this_thread::sleep_for(1 * std::chrono::seconds(1));
    // process pendingInbound but process at most 1000 to ensure that pendingOutbound can be refreshed
    // only acquire the lock for a shot amount of time
    std::vector<int> addPendingOutbound;
    std::vector<pendingInboundMessage> to_process(maxProcessedWUperSec);
    to_process.resize(pendingInbound.try_dequeue_bulk(to_process.begin(), maxProcessedWUperSec));
    { const std::lock_guard<std::mutex> lock(searchtree_mutex);
      const std::lock_guard<std::mutex> lock2(workerConnections_mutex);
      for(auto& x:to_process) {
        if(!(tree.a[x.id].tags & TAG_QUEUED)) continue;
        if(x.state == 'c') {
          // tree[x.id].state = 'd'; // do not update, loadWorkUnitResponse already does that
          std::istringstream b(pendingInboundCache.get(x.id), std::ios::binary);
          pendingInboundCache.erase(x.id);
          int oldTreeSize = tree.treeSize;
          loadf(b, "loadWorkunitResponse");
          for(int idx=oldTreeSize; idx<tree.treeSize; idx++) {
            // tree.a[idx].tags |= TAG_QUEUED; // No need either, dumpf does it
            // if(checkduplicate(onx))
            //   tree[onx].state = '2';
            std::ostringstream c(std::ios::binary);
            dumpf(c, "dumpWorkunit");
            pendingOutboundCache.set(idx, c.str());
            addPendingOutbound.push_back(idx);
            // emit(onx, Compl3t34bl3(getState(onx), tree[onx].depth, tree[onx].depth%p)); // TODO
          }
          // if(!contributorIDs.contains(x.contributor)) {
          //   contributorIDs[x.contributor] = contributors.size();
          //   contributors.push_back(x.contributor);
          // }
          // tree[x.id].contrib = contributorIDs[x.contributor];
        } else if(x.state == 'u') {
          addPendingOutbound.push_back(x.id);
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
          else addPendingOutbound.push_back(x.id);
        }
      }
    }
    pendingOutbound.enqueue_bulk(addPendingOutbound.begin(), addPendingOutbound.size());
    if((++idx) % 256 == 0) {
      const std::lock_guard<std::mutex> lock(searchtree_mutex);
      const std::lock_guard<std::mutex> lock2(workerConnections_mutex);
      std::ostringstream res;
      auto now_tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
      res << std::put_time(std::gmtime(&now_tt), "%c %Z") << " conn#: " << workerConnections.size();
      res << " q#: "<<pendingOutbound.size_approx() << " pending#: "<<pendingInbound.size_approx();
      adminConsoleHandler_queue.enqueue({1, 0, res.str()});
      if(idx % 4096 == 0) {
        std::ofstream dump(autosave1, std::ios::binary);
        dumpf(dump, "dumpTree");
        dump.flush(); dump.close();
        std::ostringstream res2; res2 << "autosaved to "<<autosave1;
        swap(autosave1, autosave2);
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
      auto it =  adminConnections.find(nx.conn_id);
      if(it == adminConnections.end())
        continue; // bizarre
      mgr = adminConnections[nx.conn_id];
    }
    std::istringstream s(nx.message);
    std::string com; s>>com;
    if(com == "help") {
      woker(mgr, nx.conn_id, {1, "commands: remoteclose connstats init loadsave dumpsave treestats"});
    } else if(com == "remoteclose") {
      woker(mgr, nx.conn_id, {3, "bye"});
    } else if(com == "connstats") {
      std::ostringstream f;
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
      std::ostringstream f; f << _f.rdbuf();
      woker(mgr, nx.conn_id, {1, f.str()});
    } else if(com == "actual-init") {
//       const std::lock_guard<std::mutex> lock(searchtree_mutex);
//       std::string options;
//       if(treeSize != 0) {
//         woker(mgr, nx.conn_id, {1, "failed, tree is already initialized"});
//         goto fail;
//       }
//       // TODO: assumes stator = 0
//       s >> p >> width >> sym >> l4h;
//       if(sym != 0 && sym != 1) {
//         woker(mgr, nx.conn_id, {1, "failed, unsupported symmetry"});
//         continue;
//       }
//       for (int i = 0; i < 2 * p; i++) {
//         uint64_t x = 0; std::string row;
//         s >> row;
//         if(sz(row) != width)  {
//           woker(mgr, nx.conn_id, {1, "failed, bad row length"});
//           goto fail;
//         }
//         for (int j = 0; j < width; j++)
//           if (row[j] == 'o')
//             x |= (1ull << j);
// // struct node { uint64_t row; int depth, shift, parent, contrib; char state; };
//         tree[newNode()]={x, short(i+1), 0, i-1, 0, 'd'};
//       } s>>options;
//       if (options[0] == 'y' || options[0] == 'Y') {
//         int filterrows; s>>filterrows;
//         for (int i = 0; i < filterrows; i++) {
//           uint64_t x = 0; std::string filter; s >> filter;
//           if(sz(filter) != width) {
//             woker(mgr, nx.conn_id, {1, "failed, bad filter length, must be w"});
//             goto fail;
//           }
//           for (int j = 0; j < width; j++)
//             if (filter[j] == 'o')
//               x |= (1ull << j);
//           filters.push_back(x);
//         }
//         for(int idx=0; idx<2; idx++){
//           int cnt; s>>cnt;
//           leftborder[idx] = std::vector<uint64_t>(cnt);
//           for(int i=0; i<cnt; i++){
//             std::string t; s>>t;
//             if(sz(t) != p)  {
//               woker(mgr, nx.conn_id, {1, "failed, bad fix length, must be p"});
//               goto fail;
//             }
//             for(int j=0; j<p; j++) if(t[j] == 'o') leftborder[idx][i] |= (1ull << j);
//           }
//         }
//         exInitrow = std::vector<uint64_t>(2*p);
//         for(int i=0; i<2*p; i++) {
//           uint64_t x = 0; std::string row;
//           s >> row;
//           if(sz(row) != width)  {
//             woker(mgr, nx.conn_id, {1, "failed, bad row length"});
//             goto fail;
//           }
//           for (int j = 0; j < width; j++)
//             if (row[j] == 'o')
//               x |= (1ull << j);
//           exInitrow[i] = x;
//         }
//       }
//       assert(treeSize == 2*p);
//       tree[2*p-1].state = 'q';
//       pendingOutbound.enqueue(genPOM(2*p-1));
//       if(s.fail()) {
//         treeSize = 0;
//         woker(mgr, nx.conn_id, {1, "unknown parsing failure"});
//         goto fail;
//       }
//       woker(mgr, nx.conn_id, {1, "successfully parsed input:\n<pre><code>" + nx.message.substr(12)+"</code></pre>"});
//       fail:;
    } else if (com == "loadsave") {
      const std::lock_guard<std::mutex> lock(searchtree_mutex);
      if(tree.treeSize != 0) {
        woker(mgr, nx.conn_id, {1, "failed, tree is already initialized"});
      } else {
        std::string fn; s>>fn;
        std::ifstream dump(fn, std::ios::binary);
        loadf(dump, "loadTree");
        int cnt = 0;
        for(int i=0; i<tree.treeSize; i++)
          if(tree.a[i].tags & TAG_QUEUED) {
            cnt++;
            // TODO: init queue state
            // pendingOutbound.enqueue(genPOM(i));
          }
        woker(mgr, nx.conn_id, {1, "loaded " + std::to_string(tree.treeSize) + " nodes from "+fn+
        " and queued "+std::to_string(cnt)+" nodes"});
      }
    } else if(com == "dumpsave") {
      const std::lock_guard<std::mutex> lock(searchtree_mutex);
      std::string fn; s>>fn;
      std::ofstream dump(fn, std::ios::binary);
      dumpf(dump, "loadTree");
      // dumpTree(fn);
      // // TODO: NE node process here
      woker(mgr, nx.conn_id, {1, "saved " + std::to_string(tree.treeSize) + " nodes to "+fn});
    } else if(com == "treestats") {
      woker(mgr, nx.conn_id, {1, "crunching tree stats..."});
      // const std::lock_guard<std::mutex> lock(searchtree_mutex);
      // std::ostringstream fx;
      // int maxdep = 0, dup = 0;
      // std::vector<std::pair<int, std::string>> conIdx;
      // for(auto s:contributors) conIdx.push_back({0, s});
      // for(int i=0; i<treeSize; i++) {
      //   maxdep = std::max(maxdep, (int)tree[i].depth);
      //   dup += (tree[i].state == '2');
      // }
      // std::vector<int> ongoing(maxdep+1), total(maxdep+1);
      // for(int i=0;i<treeSize; i++) {
      //   total[tree[i].depth]++;
      //   if(tree[i].state != 'd' && tree[i].state != '2') ongoing[tree[i].depth]++;
      //   else if(i>=2*p) conIdx[tree[i].contrib].first++;
      // }
      // fx << treeSize << " nodes, " << dup << " duplicates<br>";
      // fx << "max depth "<<maxdep <<"<br>";
      // fx << "profile";
      // for(int i=0; i<=maxdep; i++){
      //   if(ongoing[i])fx<<' '<<ongoing[i]<<'/'<<total[i];
      //   else fx<<' '<<total[i];
      // }
      // fx<<"<br>contributors<br>";
      // std::sort(conIdx.begin(), conIdx.end()); std::reverse(conIdx.begin(), conIdx.end());
      // for(auto& [cnt, id]:conIdx)fx<<id<<": "<<cnt<<" nodes<br>";
      // fx<<longgestPartial;
      // if(oscillatorComplete.size())fx<<oscillatorComplete;
      // woker(mgr, nx.conn_id, {1, fx.str()});
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
  int flag = 0; // 1 - loop over everyone
  unsigned long conn;
  unsigned long mil;
};

moodycamel::BlockingConcurrentQueue<workerhandlerMessage> workerHandler_queue;
void postWorkerDisconnect(unsigned long conn) {
  // for(int i:workerConnections[conn].attachedWorkunits) // this is done with a lock on it anyways
  //   pendingInbound.enqueue({i, conn, 'u', "", {}});
}
void workerHandler() {
  MG_INFO(("Worker handler started running"));
  workerhandlerMessage nx;
  // Every 30s, go through all of the worker connections; kick the ones that haven't sent anything in 60s
  while (workerHandler_queue.wait_dequeue(nx), nx.flag != 2) {
    if(nx.flag == 0) {
      const std::lock_guard<std::mutex> lock(workerConnections_mutex);
      auto& pp = workerConnections[nx.conn];
      if(!pp.contime) pp.contime = nx.mil;
      pp.lastmsg = nx.mil;
      pp.uptime = nx.mil - pp.contime;
    }
    if(nx.flag == 1) {
      const std::lock_guard<std::mutex> lock(workerConnections_mutex);
      std::vector<unsigned long> toKick;
      for(auto& [conn, stat] : workerConnections) 
        if(stat.contime && stat.lastmsg + worker_timeout_duration < nx.mil) {
          std::cerr << "kicked " << conn << '\n';
          toKick.push_back(conn);
        }
      for(auto conn : toKick) {
        auto it = workerConnections.find(conn);
        if(it != workerConnections.end()) {
          postWorkerDisconnect(conn);
          workerConnections.erase(it);
        }
      }
      continue;
    }
  }
  // halt all connections
  { const std::lock_guard<std::mutex> lock(workerConnections_mutex);
    for(auto & [conn, _] : workerConnections)
      postWorkerDisconnect(conn);
    workerConnections.clear();
  }
}

std::thread workerhandlerthread(workerHandler);

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
    } else if(mg_match(hm->uri, mg_str("/getconfig"), NULL)) {
      // GET
      // v2: merge with /getwork
      std::ostringstream res;
      /* TODO */
      mg_http_reply(c, 200, "Content-Type: text/raw\n", "%s", res.str().c_str());
    } else if(mg_match(hm->uri, mg_str("/keepalive"), NULL)) {
      // POST request, should contain three parameters: websocket id & ephemerality & amount
      // returns amount * [workunit identifier]
      std::istringstream co(_mg_str_to_stdstring(hm->body));
      unsigned long wsid=0;
      co >> wsid;
      workerHandler_queue.enqueue({0, wsid, mg_millis()});
      if(co.fail())
        mg_http_reply(c, 500, "Content-Type: text/raw\n", "");
      else
        mg_http_reply(c, 200, "Content-Type: text/raw\n", "OK");
    } else if(mg_match(hm->uri, mg_str("/getwork"), NULL)) {
      // POST request, should contain three parameters: websocket id & ephemerality & amount
      // returns amount * [workunit identifier]
      std::istringstream co(_mg_str_to_stdstring(hm->body));
      std::ostringstream res(std::ios::binary);
      unsigned long wsid=0;
      co >> wsid;
      int ephemeral=0;
      co >> ephemeral;
      if(!ephemeral) {
        // TODO. not yet implemented
        writeInt(0, res);
      } else {
        int amnt=0; co >> amnt;
        if(!(0 <= amnt && amnt <= 10000) || co.fail()) {
          std::cerr << "getwork failed: " << _mg_str_to_stdstring(hm->body) << std::endl;
          mg_http_reply(c, 200, "Content-Type: text/raw\n", "-1");
          return;
        }
        // TODO: dequeue until halfrows > amnt
        std::vector<int> nodes(amnt);
        nodes.resize(pendingOutbound.try_dequeue_bulk(nodes.begin(), amnt));
        // int cnt = std::min(amnt, (int)pendingOutbound.size());
        writeInt(nodes.size(), res);
        for(auto& pom:nodes) {
          pendingInbound.enqueue({pom, wsid, 's', "", {}}); // TOFIX
          writeString(pendingInboundCache.get(pom), res);
        }
      }
      workerHandler_queue.enqueue({0, wsid, mg_millis()});
      // std::cerr << res.str().size() << '\n';
      mg_http_reply(c, 200, "Content-Type: text/binary\n", "%.*s", (int)res.str().size(), res.str().data());
      // TODO update for binary, send 501 for no more work temporarily
      // if(co.fail())
      //   mg_http_reply(c, 200, "Content-Type: text/raw\n", "-1");
      // else
      //   
    } else if(mg_match(hm->uri, mg_str("/returnwork"), NULL)) {
      // POST request, should contain amount, websocket connection id
      // then amount times the following: workunit id, status - 0 or 1
      // if status is 1, further contain contributor id and all children
      std::istringstream co(_mg_str_to_stdstring(hm->body), std::ios::binary);
      int amnt;
      readInt(amnt, co);
      unsigned long wsid;
      readInt(wsid, co);
      for(int i=0; i<amnt; i++) {
        int id=0, status=0;
        readInt(id, co);
        readInt(status, co);
        if(status == 0)
          pendingInbound.enqueue({id, wsid, 'u', "", {}}); // tofix
        else {
          std::string v;
          readString(v, co);
          if(co.fail()) goto fail;
          pendingInboundCache.set(id, v);
          // std::string cid; co >> cid;
          // int ch; co >> ch;
        //   std::vector<uint64_t> rows(ch);
          // for(int i=0; i<ch; i++) co >> rows[i];
          pendingInbound.enqueue({id, wsid, 'c', cid, rows}); // tofix
        }
      }
      workerHandler_queue.enqueue({0, wsid, mg_millis()});
      if(co.fail()) goto fail;
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
  } else if(ev == MG_EV_OPEN) {
    // MG_INFO(("open triggered"));
  } else if(ev == MG_EV_CLOSE) {
    // MG_INFO(("close triggered, %d", c->id));
  } else if(ev == MG_EV_WS_CTL) {
    mg_ws_message* wm = (mg_ws_message*) ev_data;
    uint32_t s = (wm->flags) & 0xF0;
    MG_INFO(("\033[1;1;31mwebsocket control triggered, %.*s, %x \033[0m", wm->data.len, wm->data.buf, s));
    if(s == 0x80) {
      // if(c->data[1] == 'W') {
      //   haltWorkerConnection(c->id);
      //   workerHandler_queue.enqueue({4, c->id, ""});
      // }
      // else 
      if(c->data[1] == 'A')
        adminConsoleHandler_queue.enqueue({4, c->id, ""});
    }
  } else if(ev == MG_EV_WS_MSG) {
    mg_ws_message* wm = (mg_ws_message*) ev_data;
    // if(c->data[1] == 'W') {
    //   workerHandler_queue.enqueue({0, c->id, _mg_str_to_stdstring(wm->data), mg_millis()});
    // } else 
    if(c->data[1] == 'A') {
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
    if((current = mg_millis()) > lastping + worker_ping_rate) {
      lastping = current;
      workerHandler_queue.enqueue({1, 0, lastping});
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