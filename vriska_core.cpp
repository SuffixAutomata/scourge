#include "cadical/src/cadical.hpp"

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

using std::max;
using std::min;

#include "debug.h"
#include "globals.h"
#include "logic.h"
#include "searchtree.h"

struct A2BUnit { int onx, side, idx; };
std::queue<A2BUnit> searchQueue;
std::map<int, std::vector<std::vector<std::vector<halfrow>>>> staging;
std::map<int, int> remaining;

struct B2AUnit { uint64_t row; int response; A2BUnit fa; };

int enqTreeNode(int onx) {
  for(int x=0; x<2; x++)
    for(int j=0; j<tree.a[onx].n[x]; j++)
      searchQueue.push({onx, x, j}), remaining[onx]++;
  staging[onx] = std::vector<std::vector<std::vector<halfrow>>>(4, std::vector<std::vector<halfrow>>(2));
  return onx;
}

int main(int argc, char* argv[]) {
  auto t1 = std::chrono::high_resolution_clock::now();
  assert(argc == 3);
  std::ifstream fin(argv[1], std::ios::binary);
  loadf(fin, "loadWorkunit");
  for (int i = 0; i < tree.treeSize; i++) {
    if (tree.a[i].n[0] == 0 || tree.a[i].n[1] == 0)
      tree.a[i].tags &= ~TAG_QUEUED;
    if (tree.a[i].tags & TAG_QUEUED)
      enqTreeNode(i);
  }
  assert(staging.size() == 1);
  std::queue<int> toEnq;
  auto process = [&](B2AUnit x) {
    int id = x.fa.onx, depth = tree.a[x.fa.onx].depth + 1;
    if (x.response == -1) {
      if(!--remaining[id]) {
        {
          auto& w = staging[id];
          // DEBUG << "node " << id << " (seq='" << tree.brief(id) << "') done, producing children: " 
          //       << sz(w[0][0]) << "-" << sz(w[0][1]) << " " << sz(w[1][0]) << "-" << sz(w[1][1]) << " "
          //       << sz(w[2][0]) << "-" << sz(w[2][1]) << " " << sz(w[3][0]) << "-" << sz(w[3][1]) << "\n";
          for(int v=0; v<4; v++)
            if(sz(w[v][0]) && sz(w[v][1])) // <- Pruning is reduced to one literal line
              toEnq.push(tree.newNode({v, depth, TAG_QUEUED, id}, w[v]));
        }
        tree.a[x.fa.onx].tags &= ~TAG_QUEUED;
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
  while (searchQueue.size()) {
    nx = searchQueue.front();
    searchQueue.pop();
    int dep = tree.a[nx.onx].depth;
    std::vector<uint64_t> h = tree.getState(nx.onx, nx.side, nx.idx);
    genNextRows(h, dep, l4h, nx.side ? enforce2 : enforce,
                nx.side ? remember2 : remember, [&](uint64_t x) {
                  process({x, dep + 1, nx});
                });
    process({0, -1, nx});
    it++;
    if(it % 1024 == 0)
      t2 = std::chrono::high_resolution_clock::now();
    while(!searchQueue.size() && toEnq.size() && ((t2 - t1).count() < 1 * 3600 * 1e9)) {
      enqTreeNode(toEnq.front());
      toEnq.pop();
    }
  }
  std::ofstream fx(argv[2]);
  dumpf(fx, "dumpWorkunitResponse");
  fx.close();
  t2 = std::chrono::high_resolution_clock::now();
  std::cerr << "success-done " << (t2-t1);
  // std::cerr << tree.treeSize << std::endl;
  // std::cerr << toEnq.size() << std::endl;
  return 0;
}