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

struct A2BUnit { int onx, side, idx; };
moodycamel::BlockingConcurrentQueue<A2BUnit> A2B;

struct B2AUnit { uint64_t row; int response; A2BUnit fa; };
moodycamel::BlockingConcurrentQueue<B2AUnit> B2A;

void betaUniverse() {
  A2BUnit nx;
  while (A2B.wait_dequeue(nx), nx.onx != -1) {
    int dep = tree.a[nx.onx].depth;
    std::vector<uint64_t> h = tree.getState(nx.onx, nx.side, nx.idx);
    genNextRows(h, dep, l4h, nx.side ? enforce2 : enforce,
                nx.side ? remember2 : remember, [&](uint64_t x) {
                  B2A.enqueue({x, dep + 1, nx});
                });
    B2A.enqueue({0, -1, nx});
  }
}

int searchMode; // 0: BFS; 1: parallel DFS
std::pair<int,int> weigh(int onx) { return {(searchMode?1:-1)*tree.a[onx].depth, onx}; }

void search(int th) {
  int vqcnt = 0, solvedNodes = 0, solved = 0, onx, reportidx = 0, its = 0, sdep = bdep;
  std::map<int, std::vector<std::vector<std::vector<halfrow>>>> staging;
  std::map<int, int> remaining;
  std::priority_queue<std::pair<int, int>> toEnq;
  auto enqTreeNode = [&] (int onx) {
    // DEBUG << std::format("queueing node {} (seq='{}')\n", onx, tree.brief(onx));
    vqcnt++;
    for(int x=0; x<2; x++)
      for(int j=0; j<tree.a[onx].n[x]; j++)
        A2B.enqueue({onx, x, j}), qSize++, remaining[onx]++;
    staging[onx] = std::vector<std::vector<std::vector<halfrow>>>(4, std::vector<std::vector<halfrow>>(2));
  };
  auto maintainQueue = [&] {
    while (toEnq.size() && qSize < 8 * th)
      enqTreeNode(toEnq.top().second), toEnq.pop();
  };
  auto emitPartials = [&] (int onx) {
    sdep = max(sdep, (int)tree.a[onx].depth);
    int ixs[2], isComplete[2]; ixs[0] = ixs[1] = isComplete[0] = isComplete[1] = 0;
    for(int x=0; x<2; x++)
      for (int i = 0; i < tree.a[onx].n[x]; i++) {
        const std::vector<uint64_t> st = tree.getState(onx, x, i);
        if (Compl3t34bl3(st, tree.a[onx].depth % p, x ? enforce2 : enforce))
          ixs[x] = i, isComplete[x] = 1;
      }
    std::vector<uint64_t> marge = tree.getState(onx, 0, ixs[0], true);
    std::vector<uint64_t> marge2 = tree.getState(onx, 1, ixs[1], true);
    for (int x = 0; x < sz(marge); x++)
      marge[x] |= marge2[x];
    emit(marge, isComplete[0] && isComplete[1]);
    return onx;
  };
  for (int i = 0; i < tree.treeSize; i++) {
    if (tree.a[i].n[0] == 0 || tree.a[i].n[1] == 0)
      tree.a[i].tags &= ~TAG_QUEUED; // assholes, might as well skip them
    if (tree.a[i].tags & TAG_QUEUED)
      toEnq.push(weigh(i));
  }
  maintainQueue();
  STATUS << vqcnt << " nodes queued (" << qSize << " halfrows)\n";
  std::vector<std::thread> universes;
  for (int i = 0; i < th; i++)
    universes.emplace_back(betaUniverse);
  B2AUnit x;
  auto report = [&] {
    STATUS << "solved " << solvedNodes << " N (" << solved << " ½rs); queued " << vqcnt << " N (" << qSize << " ½rs); total " << tree.treeSize << " nodes\n";
    if (reportidx % 8 == 0) {
      long long cnt = 0, cnt2 = 0;
      for(int i=0; i<tree.treeSize; i++)
        cnt += tree.a[i].n[0] * tree.a[i].n[1], cnt2 += (tree.a[i].tags & TAG_QUEUED) ? tree.a[i].n[0] * tree.a[i].n[1] : 0;
      STATUS << "depth reached " << sdep << "; " << cnt << " (" << cnt2 << " queued) implied nodes; tree profile";
      for (int i = 2 * p; i <= sdep; i++) {
        STATUS << ' ' << tree.depths[i] << '/' << tree.depthcnt[i];
        // TODO: better status output here
      }
      STATUS << '\n';
      if (reportidx % 32 == 0) {
        std::ofstream dump("dump.txt");
        dumpf(dump);
        dump.close();
      }
    }
    reportidx++;
  };
  while (B2A.wait_dequeue(x), 1) {
    int id = x.fa.onx, depth = tree.a[x.fa.onx].depth + 1;
    if (x.response == -1) {
      if(!--remaining[id]) {
        {
          auto& w = staging[id];
          DEBUG << "node " << id << " (seq='" << tree.brief(id) << "') done, producing children: " 
                << sz(w[0][0]) << "-" << sz(w[0][1]) << " " << sz(w[1][0]) << "-" << sz(w[1][1]) << " "
                << sz(w[2][0]) << "-" << sz(w[2][1]) << " " << sz(w[3][0]) << "-" << sz(w[3][1]) << "\n";
          for(int v=0; v<4; v++)
            if(sz(w[v][0]) && sz(w[v][1])) // <- Pruning is reduced to one literal line
              toEnq.push(weigh(emitPartials(tree.newNode({v, depth, TAG_QUEUED, id}, w[v]))));
        }
        vqcnt--, tree.depths[depth-1]--, solvedNodes++;
        tree.a[x.fa.onx].tags &= ~TAG_QUEUED;
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
    loadf(f);
  } else {
    std::string x = "n";
    if (std::filesystem::exists("dump.txt")) {
      WARN << "dump.txt 3XS1STS. CONT1NU3 S34RCH? [Y/N] \n";
      std::cin >> x;
    }
    if (x[0] == 'y' || x[0] == 'Y') {
      std::ifstream f("dump.txt");
      loadf(f);
      f.close();
      INFO << "THR34DS, LOOK4H34D: [" << th << ' ' << l4h << "] \n";
      std::cin >> th >> l4h;
    } else {
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
        int ff = tree.newNode({CENTER_ID(x), // Center
                      i+1, // Depth
                      (i == 2*p-1) ? TAG_QUEUED : 0, // No tags
                      i-1, // Ascendant
                     }, {{{SIDE0_ID(x), 0}}, {{SIDE1_ID(x), 0}}});
        // WARN << tree.a[ff].h[0] << ' ' << tree.a[ff].h[0][0].v << '\n';
      }
      INFO << "4DD1TION4L OPT1ONS? [Y/N] \n";
      std::string options;
      std::cin >> options;
      if (options[0] == 'y' || options[0] == 'Y') {
        INFO << "F1LT3R ROWS: \n";
        int filterrows;
        std::cin >> filterrows;
        for (int i = 0; i < filterrows; i++) {
          uint64_t x = 0;
          std::string s;
          std::cin >> s;
          for (int j = 0; j < width; j++)
            if (s[j] == 'o')
              x |= (1ull << j);
          filters.push_back(x);
        }
      }
    }
  }
  for (std::string i : args)
    if (i.substr(0, 2) == "dp")
      deplim = stoi(i.substr(2));
  if (args.contains("dfs")) searchMode = 1;
  if (args.count("splitdistrib")) {
    // SPL1T S34RCH TO MUTU4LLY 1ND3P3ND3NT S4V3F1L3S
  }
  if (args.count("search")) {
    // WARN << tree.a[13].h[0][0].v << '\n';
    search(th);
    std::ofstream dump("dump.txt");
    dumpf(dump);
    dump.close();
  } else if (args.count("distrib")) {
  }
  return 0;
}