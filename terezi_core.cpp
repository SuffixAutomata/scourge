// TODO : cycle detection ; drifting rows ; symmetries ; smart stopping ;
// PRUNING. PRUNING done. cycle detection IMPOSSIBLE now. drifting rows ALSO
// PROBABLY IMPOSSIBLE
#include "cadical/src/cadical.hpp"
#include "cqueue/bcq.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

using std::max;
using std::min;

#include "debug.h"
#include "globals.h"
#include "logic.h"
#include "satlogic.h"
#include "searchtree.h"

searchTree* T;

int bdep = 0;
void emit(std::vector<uint64_t> mat, bool compl3t3) {
  if (!compl3t3) {
    if (sz(mat) <= bdep)
      return;
    bdep = max(bdep, sz(mat));
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
  if (compl3t3)
    exit(0);
}

struct A2BUnit { int onx, side, idx; };
moodycamel::BlockingConcurrentQueue<A2BUnit> A2B;

struct B2AUnit { uint64_t row; int response; A2BUnit fa; };
moodycamel::BlockingConcurrentQueue<B2AUnit> B2A;

void betaUniverse() {
  A2BUnit nx;
  while (A2B.wait_dequeue(nx), nx.onx != -1) {
    int dep = T->a[nx.onx].depth;
    std::vector<uint64_t> h = T->getState(nx.onx, nx.side, nx.idx);
    genNextRows(h, dep, l4h, nx.side ? enforce2 : enforce,
                nx.side ? remember2 : remember, [&](uint64_t x) {
                  B2A.enqueue({x, dep + 1, nx});
                });
    B2A.enqueue({0, -1, nx});
  }
}

int searchMode; // 0: BFS; 1: parallel DFS

void search(int th) {
  int vqcnt = 0, solvedNodes = 0, solved = 0, onx, reportidx = 0, its = 0, sdep = bdep;
  std::map<int, std::vector<std::vector<std::vector<halfrow>>>> staging;
  std::map<int, int> remaining;
  std::priority_queue<std::pair<int, int>> toEnq;
  int altQ[2] = {0, 0};
  auto enqTreeNode = [&] (int onx) {
    if(searchMode == 2 && T->a[onx].n[0] + T->a[onx].n[1] >= 41) {
      WARN << "Skipping node " << onx << "( seq='" << T->brief(onx) << "') due to smallOnly: " << T->a[onx].n[0] << "+" << T->a[onx].n[1] << '\n';
      return;
    }
    // DEBUG << std::format("queueing node {} (seq='{}')\n", onx, T->brief(onx));
    vqcnt++;
    for(int x=0; x<2; x++) {
      altQ[x] -= T->a[onx].n[x];
      qSize += T->a[onx].n[x];
      for(int j=0; j<T->a[onx].n[x]; j++)
        A2B.enqueue({onx, x, j}), remaining[onx]++;
    }
    staging[onx] = std::vector<std::vector<std::vector<halfrow>>>(4, std::vector<std::vector<halfrow>>(2));
  };
  auto maintainQueue = [&] {
    while (toEnq.size() && qSize < 8 * th)
      enqTreeNode(toEnq.top().second), toEnq.pop();
  };
  auto pushToHeap = [&] (int onx) {
    sdep = max(sdep, (int)T->a[onx].depth);
    int ixs[2], isComplete[2]; ixs[0] = ixs[1] = isComplete[0] = isComplete[1] = 0;
    for(int x=0; x<2; x++)
      for (int i = 0; i < T->a[onx].n[x]; i++) {
        const std::vector<uint64_t> st = T->getState(onx, x, i);
        if (Compl3t34bl3(st, T->a[onx].depth, T->a[onx].depth % p, x ? enforce2 : enforce)) {
          // WARN << onx << ' ' << x << ' ' << i << '\n';
          // emit(T->getState(onx, x, i, true), 0); bdep = 0;
          ixs[x] = i, isComplete[x] = 1;
          break;
        }
      }
    // WARN << onx << '\n';
    std::vector<uint64_t> marge = T->getState(onx, 0, ixs[0], true);
    std::vector<uint64_t> marge2 = T->getState(onx, 1, ixs[1], true);
    // WARN << sz(marge) << ' ' << sz(marge2) << ' ' << T->a[onx].depth << '\n';
    for (int x = 0; x < sz(marge); x++)
      marge[x] |= marge2[x];
    
    emit(marge, isComplete[0] && isComplete[1]);
    altQ[0] += T->a[onx].n[0], altQ[1] += T->a[onx].n[1];
    toEnq.push({(searchMode?1:-1)*T->a[onx].depth, onx});
  };
  for (int i = 0; i < T->treeSize; i++) {
    if (T->a[i].n[0] == 0 || T->a[i].n[1] == 0)
      T->a[i].tags &= ~TAG_QUEUED; // assholes, might as well skip them
    if ((T->a[i].tags & TAG_QUEUED) && !(T->a[i].tags & TAG_REMOTE_QUEUED))
      pushToHeap(i);
  }
  maintainQueue();
  std::vector<std::thread> universes;
  for (int i = 0; i < th; i++)
    universes.emplace_back(betaUniverse);
  B2AUnit x;
  auto t1 = std::chrono::high_resolution_clock::now();
  std::string dump1 = "dump-odd.txt", dump2 = "dump-even.txt";
  auto report = [&] {
    STATUS << "solved " << solvedNodes << " N (" << solved << " ½rs); ";
    STATUS << "queued " << vqcnt << "+" << toEnq.size() << " N (" << qSize << "+(" << altQ[0] << "+" << altQ[1] << ") ½rs); total " << T->treeSize << " nodes\n";
    if (reportidx % 8 == 0) {
      long long cnt = 0, cnt2 = 0;
      for(int i=0; i<T->treeSize; i++)
        cnt += T->a[i].n[0] * T->a[i].n[1], cnt2 += (T->a[i].tags & TAG_QUEUED) ? T->a[i].n[0] * T->a[i].n[1] : 0;
      STATUS << "depth reached " << sdep << "; " << cnt << " (" << cnt2 << " queued) implied nodes; tree profile";
      for (int i = 2 * p; i <= sdep; i++) {
        STATUS << ' ' << T->depths[i] << '/' << T->depthcnt[i];
        // TODO: better status output here
      }
      STATUS << '\n';
      auto t2 = std::chrono::high_resolution_clock::now();
      std::chrono::nanoseconds diff = t2 - t1;
      if (diff.count() > 600 * 1e9) {
        std::ofstream dump(dump1);
        dumpf(dump, "dumpTree", T);
        dump.close();
        swap(dump1, dump2);
        t1 = t2;
      }
    }
    reportidx++;
  };
  report();
  while (B2A.wait_dequeue(x), 1) {
    int id = x.fa.onx, depth = T->a[x.fa.onx].depth + 1;
    if (x.response == -1) {
      if(!--remaining[id]) {
        {
          auto& w = staging[id];
          // DEBUG << "node " << id << " (seq='" << T->brief(id) << "') done, producing children: " 
          //       << sz(w[0][0]) << "-" << sz(w[0][1]) << " " << sz(w[1][0]) << "-" << sz(w[1][1]) << " "
          //       << sz(w[2][0]) << "-" << sz(w[2][1]) << " " << sz(w[3][0]) << "-" << sz(w[3][1]) << "\n";
          for(int v=0; v<4; v++)
            if(sz(w[v][0]) && sz(w[v][1])) // <- Pruning is reduced to one literal line
              pushToHeap(T->newNode({v, depth, TAG_QUEUED, id}, w[v]));
        }
        vqcnt--, T->depths[depth-1]--, solvedNodes++;
        T->a[x.fa.onx].tags &= ~TAG_QUEUED;
        staging.erase(staging.find(id));
      }
      ++solved, --qSize;
      maintainQueue();
    } else {
      if ((depth - 1) / p < sz(filters) && ((x.row & filters[(depth - 1) / p]) != x.row)) {
        WARN << x.row << ' ' << filters[(depth - 1) / p] << '\n';
        assert(0);
        // cout << "[1NFO] NOD3 1GNOR3D" << endl;
      } else
        staging[id][CENTER_ID(x.row)][x.fa.side].push_back({x.row, x.fa.idx});
    }
    its++;
    if (its % 8192 == 0)
      report();
    if (x.response == -1 && !qSize)
      break;
  }
  for (int i = 0; i < th; i++)
    A2B.enqueue({-1, -1, -1});
  for (std::thread &x : universes)
    x.join();
  reportidx = 0, report();
}

int main(int argc, char *argv[]) {
  WARN << primeImplicants.size() << " PR1M3 1MPL1C4NTS\n";
  std::set<std::string> args;
  for (int i = 1; i < argc; i++)
    args.insert(argv[i]);
  if (std::filesystem::exists("dump.txt") && args.count("continue")) {
    std::ifstream f("dump.txt");
    T = loadf(f, "loadTree");
  } else {
    std::string x = "n";
    if (std::filesystem::exists("dump.txt")) {
      WARN << "dump.txt 3XS1STS. CONT1NU3 S34RCH? [Y/N] \n";
      std::cin >> x;
    }
    if (x[0] == 'y' || x[0] == 'Y') {
      std::ifstream f("dump.txt");
      T = loadf(f, "dumpTree");
      f.close();
      INFO << "THR34DS, LOOK4H34D: [" << th << ' ' << l4h << "] \n";
      std::cin >> th >> l4h;
    } else {
      T = new searchTree;
      INFO << "THR34DS, LOOK4H34D: \n";
      std::cin >> th >> l4h;
      INFO << "P3R1OD, W1DTH, SYMM3TRY, ST4TOR W1DTH: \n";
      std::cin >> p >> width >> sym >> stator;
      if (sym != 0 && sym != 1)
        INFO << "OTH3R SYMM3TR13S T3MPOR4R1LY NOT T3ST3D >:[\n";
      bthh = (sym ? width / 4 : width / 2);
      INFO << "B1S3CT1ON THR3SHOLD: [SUGG3ST3D " << bthh << "] \n";
      std::cin >> bthh;
      calculateMasks(bthh);
      INFO << "F1RST " << 2 * p << " ROWS:\n";
      for (int i = 0; i < 2 * p; i++) {
        uint64_t x = 0;
        std::string s;
        std::cin >> s;
        for (int j = 0; j < width; j++)
          if (s[j] == 'o')
            x |= (1ull << j);
        int ff = T->newNode({CENTER_ID(x), // Center
                      i+1, // Depth
                      (i == 2*p-1) ? TAG_QUEUED : 0, // No tags
                      i-1, // Ascendant
                     }, {{{SIDE0_ID(x), 0}}, {{SIDE1_ID(x), 0}}});
        // WARN << T->a[ff].h[0] << ' ' << T->a[ff].h[0][0].v << '\n';
      }
      INFO << "4DD1TION4L OPT1ONS? [Y/N] \n";
      std::string options;
      std::cin >> options;
      if (options[0] == 'y' || options[0] == 'Y') {
        INFO << "F1LT3R ROWS: \n";
        int filterrows;
        std::cin >> filterrows;
        filters = std::vector<uint64_t>(filterrows);
        for (int i = 0; i < filterrows; i++) {
          std::string t; std::cin >> t;
          assert(t.size() == (size_t)width);
          for (int j = 0; j < width; j++) if (t[j] == 'o') filters[i] |= (1ull << j);
        }
        INFO << "L3FT COLUMNS: \n";
        for(int s=0; s<2; s++){
          int cnt; std::cin >> cnt;
          leftborder[s] = std::vector<uint64_t>(cnt);
          for(int i=0; i<cnt; i++){
            std::string t; std::cin >> t;
            assert(t.size() == (size_t)p);
            for(int j=0; j<p; j++) if(t[j] == 'o') leftborder[s][i] |= (1ull << j);
          }
        }
        INFO << "4LT 1N1TROWS: \n";
        int eis; std::cin >> eis;
        assert(eis == 0 || eis == 2*p);
        exInitrow = std::vector<uint64_t>(eis);
        for (int i = 0; i < eis; i++) {
          std::string t; std::cin >> t;
          assert(t.size() == (size_t)width);
          for (int j = 0; j < width; j++) if (t[j] == 'o') exInitrow[i] |= (1ull << j);
        }
      }
    }
  }
  for (std::string i : args)
    if (i.substr(0, 2) == "dp")
      deplim = stoi(i.substr(2));
  if (args.contains("dfs")) searchMode = 1;
  if (args.contains("smallOnly")) {
    WARN << "Only running small nodes (L+R < 41).\n";
    searchMode = 2;
  }
  if (args.count("splitdistrib")) {
    // SPL1T S34RCH TO MUTU4LLY 1ND3P3ND3NT S4V3F1L3S
  }
  if (args.count("search")) {
    // WARN << T->a[13].h[0][0].v << '\n';
    search(th);
    std::ofstream dump("dump.txt");
    dumpf(dump, "dumpTree", T);
    dump.close();
  } else if (args.count("distrib")) {
    for(int i=0; i<T->treeSize; i++) {
      if(!(T->a[i].tags & TAG_QUEUED)) continue;
      if((T->a[i].tags & TAG_REMOTE_QUEUED)) continue;
      std::string pth = "tasks/";
      pth += std::to_string(i/1000);
      if(!std::filesystem::exists(pth))
        std::filesystem::create_directory(pth);
      std::ofstream dump_(pth+"/"+std::to_string(i)+".txt");
      dumpf(dump_, "dumpWorkunit", T, i);
      dump_.close();
    }
    std::ofstream dump("dump.txt");
    dumpf(dump, "dumpTree", T);
    dump.close();
  }
  delete T;
  return 0;
}