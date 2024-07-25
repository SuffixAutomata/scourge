#include "cadical/src/cadical.hpp"
#include "cqueue/bcq.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

using std::max;
using std::min;

#include "debug.h"
#include "globals.h"
#include "logic.h"
#include "searchtree.h"

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

struct A2BUnit { int onx, side0idx, side1idx; };
std::queue<A2BUnit> A2B;
moodycamel::BlockingConcurrentQueue<A2BUnit> _A2B;

struct B2AUnit { bool valid; A2BUnit fa; };
moodycamel::BlockingConcurrentQueue<B2AUnit> B2A;

std::map<int, std::vector<std::set<int>>> validHalfrow;

int hasSolution = 0, skipped = 0, killed = 0;

void betaUniverse() {
  A2BUnit nx;
  while (_A2B.wait_dequeue(nx), nx.onx != -1) {
    int dep = tree.a[nx.onx].depth;
    std::vector<uint64_t> h = tree.getState(nx.onx, 0, nx.side0idx);
    std::vector<uint64_t> h2 = tree.getState(nx.onx, 1, nx.side1idx);
    for(int i=0; i<2*p; i++) h[i] |= h2[i];
    bool ans = existsNextRows(h, dep, l4h, -1, -1);
    B2A.enqueue({ans, nx});
  }
}

void search(int th) {
  STATUS << std::format("{} solving targets queued\n", A2B.size());
  int ran = 0;
  auto maintainQueue = [&] {
    while(qSize < 8 * th && A2B.size()) {
      auto nx = A2B.front(); A2B.pop();
      if(validHalfrow[nx.onx][0].contains(nx.side0idx) && validHalfrow[nx.onx][1].contains(nx.side1idx))
        ran++, skipped++;
      else _A2B.enqueue(nx), qSize++;
    }
  };
  maintainQueue();
  std::vector<std::thread> universes;
  for (int i = 0; i < th; i++)
    universes.emplace_back(betaUniverse);
  B2AUnit x;
  auto report = [&] {
    STATUS << std::format("solved {} implied nodes, killed {}, skipped {} (total {}, remaining {})\n",
                          hasSolution, killed, skipped, ran, qSize + A2B.size());
  };
  while (B2A.wait_dequeue(x), 1) {
    ran++;
    if(x.valid) {
      hasSolution++;
      validHalfrow[x.fa.onx][0].insert(x.fa.side0idx);
      validHalfrow[x.fa.onx][1].insert(x.fa.side1idx);
    } else killed++;
    if(ran % 4096 == 0) report();
    qSize--;
    maintainQueue();
    if (!qSize)
      break;
  }
  for (int i = 0; i < th; i++)
    _A2B.enqueue({-1, -1, -1});
  for (std::thread &x : universes)
    x.join();
  report();
}

int main(int argc, char *argv[]) {
  WARN << primeImplicants.size() << " PR1M3 1MPL1C4NTS\n";
  assert(std::filesystem::exists(argv[1]));
  std::ifstream f(argv[1]);
  loadf(f);
  f.close();
  INFO << "THR34DS, LOOK4H34D: \n";
  std::cin >> th >> l4h;
  int cnt = 0, hrs = 0;
  for(int i=0; i<tree.treeSize; i++)
    cnt += tree.a[i].n[0] * tree.a[i].n[1], hrs += tree.a[i].n[0] + tree.a[i].n[1];
  DEBUG << "total " << cnt << " implied nodes by " << hrs << " ½rs\n";
  cnt = 0, hrs = 0;
  for(int i=0; i<tree.treeSize; i++)
    if(tree.a[i].tags & TAG_QUEUED)
      cnt += tree.a[i].n[0] * tree.a[i].n[1], hrs += tree.a[i].n[0] + tree.a[i].n[1];
  INFO << "active " << cnt << " implied nodes by " << hrs << " ½rs\n";
  for(int i=0; i<tree.treeSize; i++)
    if(tree.a[i].tags & TAG_QUEUED) {
      validHalfrow[i] = std::vector<std::set<int>>(2);
      std::vector<std::pair<int, int>> vp;
      for(int j0=0; j0<tree.a[i].n[0]; j0++)
        for(int j1=0; j1<tree.a[i].n[1]; j1++)
          vp.push_back({j0, j1});
      std::sort(vp.begin(), vp.end(), [&](std::pair<int,int> a, std::pair<int,int> b){
        return min(a.first, a.second) < min(b.first, b.second);
      });
      for(auto [j0, j1] : vp)
        A2B.push({i, j0, j1});
    }
  search(th);
  for(int i=0; i<tree.treeSize; i++)
    if(tree.a[i].tags & TAG_QUEUED)
      tree.filterNode(i, validHalfrow[i]);    
  std::ofstream dump(std::string(argv[1]) + "-new");
  dumpf(dump);
  dump.close();
  

  return 0;
}