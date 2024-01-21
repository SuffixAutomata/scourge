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
    cout << "[[OSC1LL4TOR COMPL3T3!!!]]" << endl;
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
  cout << "x = 0, y = 0, rule = B3/S23\n" + rle + '!' << endl;
  if (compl3t3)
    exit(0);
}

struct horseQueue {
  std::set<std::pair<long long, std::pair<int, int>>> internal;
  std::vector<uint64_t> targ;
  std::mt19937 rng;
  bool first = true;
  horseQueue() : rng(720) {}
  int size() { return sz(internal); }
  std::pair<int, int> popTop(int desired) {
    if (desired == -1) {
      std::pair<int, int> val = internal.begin()->second;
      internal.erase(internal.begin());
      return val;
    } else {
      auto it = internal.begin();
      while (it != internal.end() && it->second.first != desired)
        it++;
      if (it == internal.end())
        return popTop(-1);
      std::pair<int, int> val = it->second;
      internal.erase(it);
      return val;
    }
  }
  void push(std::pair<int, int> nx) {
    auto &ref = nx.first ? tree2 : tree;
    std::vector<uint64_t> h = ref.getState(nx.second, 1);
    int wt = -sz(h);
    // for(; (wt<min((int)targ.size(), (int)h.size() - 2 * p)) &&
    //   ((h[wt + 2 * p] >> __builtin_ctzll(overlap)) & 3) == targ[wt]; wt++);
    // if(wt == min((int)targ.size(), (int)h.size() - 2 * p)) wt = 1000 - wt;
    internal.insert({(-wt) * 65536 + rng() % 65536, nx});
  }
  void retarget(std::vector<uint64_t> newtarg) {
    // if(first) { first = false; return; }
    // // TODO: rewrite this. this ineff af
    // std::vector<std::pair<int, int>> nxs;
    // for(auto [a, b] : internal)
    //   nxs.push_back(b);
    // internal.clear();
    // targ = newtarg;
    // for(auto b : nxs) push(b);
  }
} PQ;

void retargetDfs(std::vector<uint64_t> &besttarg,
                 std::vector<uint64_t> &curttarg, int &bw,
                 std::vector<int> t1nodes, std::vector<int> t2nodes,
                 bool absolved, std::vector<int> &sol1, std::vector<int> &lux1,
                 std::vector<int> &sol2, std::vector<int> &lux2) {
  // PRUNE
  if (t1nodes.empty() || t2nodes.empty()) {
    if (!absolved) { // CROW STRIDER
      for (int i : t1nodes)
        tree.a[i].state = 'd';
      for (int i : t2nodes)
        tree2.a[i].state = 'd';
    }
    return;
  }
  // TODO: also try matching a with b, lookahead for combination, etc..
  {
    int i = t1nodes[0], j = t2nodes[0];
    for (int ri : t1nodes)
      if (tree.a[i].tags & TAG_VALID_COMPLETION)
        i = ri;
    for (int rj : t2nodes)
      if (tree2.a[i].tags & TAG_VALID_COMPLETION)
        j = rj;
    std::vector<uint64_t> marge = tree.getState(i, 1);
    std::vector<uint64_t> marge2 = tree2.getState(j, 1);
    for (int x = 0; x < sz(marge); x++)
      marge[x] |= marge2[x];
    emit(marge, (tree.a[i].tags & tree2.a[j].tags) & TAG_VALID_COMPLETION);
  }

  // for(int i:t1nodes) for(int j:t2nodes) {
  //   std::vector<uint64_t> marge = tree.getState(i, 1);
  //   std::vector<uint64_t> marge2 = tree2.getState(j, 1);
  //   for(int x=0; x<(int)marge.size(); x++) marge[x] |= marge2[x];
  //   emit(marge, (tree.a[i].tags & tree2.a[j].tags) & TAG_VALID_COMPLETION);
  // }
  // FIND TARGET
  int a1 = sz(t1nodes), b1 = 0, a2 = sz(t2nodes), b2 = 0;
  for (int i : t1nodes)
    b1 += (tree.a[i].state == 'q');
  for (int i : t2nodes)
    b2 += (tree2.a[i].state == 'q');
  absolved = absolved || (b1 + b2);
  // weight: a1 * b2 + b1 * a2. i know this counts q-q twice!!!!!!!! >:]
  int sw = a1 * b2 + b1 * a2;
  if ((sw > bw && sz(curttarg) >= sz(besttarg)) ||
      (sw && sz(curttarg) > sz(besttarg)))
    besttarg = curttarg, bw = sw;
  std::vector<std::vector<int>> t1x(4), t2x(4);
  for (int i : t1nodes) {
    int f = sol1[i];
    while (f != -1) {
      if (tree.a[f].state != 'd')
        t1x[(tree.a[f].row >> __builtin_ctzll(overlap)) & 3].push_back(f);
      f = lux1[f];
    }
  }
  for (int i : t2nodes) {
    int f = sol2[i];
    while (f != -1) {
      if (tree2.a[f].state != 'd')
        t2x[(tree2.a[f].row >> __builtin_ctzll(overlap)) & 3].push_back(f);
      f = lux2[f];
    }
  }
  // if(besttarg == curttarg) {
  //   cout << "[1NFO] ";
  //   for(int nx=0; nx<4; nx++) cout << t1x[nx].size() << ' ' << t2x[nx].size()
  //   << ' '; cout << endl;
  // }
  for (int nx = 0; nx < 4; nx++) {
    curttarg.push_back(nx);
    retargetDfs(besttarg, curttarg, bw, t1x[nx], t2x[nx], absolved, sol1, lux1,
                sol2, lux2);
    curttarg.pop_back();
  }
}

void prune() {
  BENCHMARK(pruning)
  std::vector<int> sol1(tree.treeSize, -1), lux1(tree.treeSize, -1),
      halted1(tree.treeSize);
  for (int i = 1; i < tree.treeSize; i++)
    lux1[i] = sol1[tree.a[i].parent], sol1[tree.a[i].parent] = i,
    halted1[i] = halted1[tree.a[i].parent] || (tree.a[i].state == 'd');
  int x1 = 0, p1 = 0;
  for (int i = 0; i < tree.treeSize; i++)
    x1 += !halted1[i];
  std::vector<int> sol2(tree2.treeSize, -1), lux2(tree2.treeSize, -1),
      halted2(tree2.treeSize);
  for (int i = 1; i < tree2.treeSize; i++)
    lux2[i] = sol2[tree2.a[i].parent], sol2[tree2.a[i].parent] = i,
    halted2[i] = halted2[tree2.a[i].parent] || (tree2.a[i].state == 'd');
  int x2 = 0, p2 = 0;
  for (int i = 0; i < tree2.treeSize; i++)
    x2 += !halted2[i];
  BENCHMARKEND(pruning)
  BENCHMARK(targfinding)
  std::vector<uint64_t> besttarg,
      curttarg{(uint64_t)((tree2.a[0].row >> __builtin_ctzll(overlap)) & 3)};
  int bw = 0;
  retargetDfs(besttarg, curttarg, bw, {0}, {0}, 0, sol1, lux1, sol2, lux2);
  if (sz(besttarg))
    besttarg.erase(besttarg.begin(), besttarg.begin() + 2 * p);
  for (int i = 1; i < tree.treeSize; i++)
    halted1[i] = halted1[tree.a[i].parent] || (tree.a[i].state == 'd');
  for (int i = 0; i < tree.treeSize; i++)
    p1 += !halted1[i];
  for (int i = 1; i < tree2.treeSize; i++)
    halted2[i] = halted2[tree2.a[i].parent] || (tree2.a[i].state == 'd');
  for (int i = 0; i < tree2.treeSize; i++)
    p2 += !halted2[i];
  BENCHMARKEND(targfinding)
  BENCHMARK(retargeting)
  if (sz(besttarg))
    PQ.retarget(besttarg);
  else
    cout << "[1NFO] F41L3D TO F1ND T4RG3T! >:[" << endl;
  BENCHMARKEND(retargeting)

  cout << "[1NFO] PRUN1NG: " << x1 << " -> " << p1 << "/" << tree.treeSize
       << ", " << x2 << " -> " << p2 << "/" << tree2.treeSize << endl;
  cout << "[1NFO] N3W T4RG3T: ";
  for (int i : besttarg)
    cout << i;
  cout << ' ' << bw << endl;
}

moodycamel::BlockingConcurrentQueue<std::pair<int, int>> A2B;
moodycamel::BlockingConcurrentQueue<node> B2A;

void betaUniverse() {
  std::pair<int, int> nx;
  while (A2B.wait_dequeue(nx), nx.second != -1) {
    auto &ref = nx.first ? tree2 : tree;
    int dep = ref.a[nx.second].depth;
    if (!ref.shouldSkip(nx.second)) {
      std::vector<uint64_t> h = ref.getState(nx.second);
      genNextRows(h, dep, l4h, nx.first ? enforce2 : enforce,
                  nx.first ? remember2 : remember, [&](uint64_t x) {
                    B2A.enqueue(
                        {x, dep + 1, nx.second, ' ',
                         0 | (nx.first ? TAG_RETURNING_NODE_TREE2 : 0)});
                  });
    }
    B2A.enqueue({0, dep, -nx.second, ' ',
                 0 | (nx.first ? TAG_RETURNING_NODE_TREE2 : 0)});
  }
}

void search(int th) {
  int solved = 0, onx;
  std::vector<std::thread> universes;
  for (int i = 0; i < th; i++)
    universes.emplace_back(betaUniverse);
  node x;
  int reportidx = 0;
  int sdep = bdep;
  auto report = [&] {
    cout << "[1NFO] SOLV3D " << solved << "; QU3U3D " << qSize + PQ.size()
         << "; TOT4L " << tree.treeSize << '+' << tree2.treeSize << " NOD3S"
         << endl;
    if (reportidx % 8 == 0) {
      cout << "[1NFO] D3PTH R34CH3D " << sdep << " ; TR33 PROF1L3";
      for (int i = 2 * p; i <= sdep; i++) {
        if (tree.depths[i] == 0 && tree2.depths[i] == 0)
          cout << " 0";
        else
          cout << ' ' << tree.depths[i] << '+' << tree2.depths[i];
        if (tree.depths[i] == tree.depthcnt[i] &&
            tree2.depths[i] == tree2.depthcnt[i])
          ;
        else
          cout << '/' << tree.depthcnt[i] << '+' << tree2.depthcnt[i];
      }
      cout << endl;
    }
    if (reportidx % 1 == 0) {
      prune();
    }
    reportidx++;
  };
  while (B2A.wait_dequeue(x), 1) {
    auto &ref = (x.tags & TAG_RETURNING_NODE_TREE2) ? tree2 : tree;
    if (x.parent < 0) {
      ref.depths[x.depth]--, solved++, --qSize;
      if (ref.a[-x.parent].state == 'q')
        ref.a[-x.parent].state = 'u';
      if (solved % (64 * 64) == 0)
        report();
    } else {
      if ((x.depth - 1) / p < sz(filters) &&
          ((x.row & filters[(x.depth - 1) / p]) != x.row)) {
        cout << x.row << ' ' << filters[(x.depth - 1) / p] << endl;
        assert(0);
        // cout << "[1NFO] NOD3 1GNOR3D" << endl;
      } else {
        x.state = 'q', onx = ref.newNode(x);
        const std::vector<uint64_t> st = ref.getState(onx);
        if (Compl3t34bl3(st, ref.a[onx].depth % p,
                         (x.tags & TAG_RETURNING_NODE_TREE2) ? enforce2
                                                             : enforce)) {
          if (!(x.tags & TAG_RETURNING_NODE_TREE2))
            cout << "[1NFO] TR33 " << !!(x.tags & TAG_RETURNING_NODE_TREE2)
                 << " NOD3 " << onx << " 1S 4 V4L1D COMPL3T1ON" << endl;
          ref.a[onx].tags |= TAG_VALID_COMPLETION;
        }
        PQ.push({!!(x.tags & TAG_RETURNING_NODE_TREE2), onx});
        // if(x.depth < deplim) {
        //   A2B.enqueue({!!(x.tags & TAG_RETURNING_NODE_TREE2), onx}), qSize++;
        // }
        sdep = max(sdep, x.depth);
      }
    }
    while (PQ.size() && qSize <= 2 * th && nodelim) {
      std::pair<int, int> best = PQ.popTop((solved % 3) - 1);
      A2B.enqueue(best), qSize++, nodelim--;
    }
    if (x.parent < 0 && !qSize)
      break;
  }
  for (int i = 0; i < th; i++)
    A2B.enqueue({0, -1});
  for (std::thread &x : universes)
    x.join();
  reportidx = 0, report();
}

int main(int argc, char *argv[]) {
  cout << primeImplicants.size() << " PR1M3 1MPL1C4NTS" << endl;
  std::set<std::string> args;
  for (int i = 1; i < argc; i++)
    args.insert(argv[i]);
  if (std::filesystem::exists("dump.txt") && args.count("continue")) {
    std::ifstream f("dump.txt");
    loadf(f);
  } else {
    std::string x = "n";
    if (std::filesystem::exists("dump.txt")) {
      cout << "dump.txt 3XS1STS. CONT1NU3 S34RCH? [Y/N] " << endl;
      std::cin >> x;
    }
    if (x[0] == 'y' || x[0] == 'Y') {
      std::ifstream f("dump.txt");
      loadf(f);
      cout << "THR34DS, LOOK4H34D: [" << th << ' ' << l4h << "] " << endl;
      std::cin >> th >> nodelim >> l4h;
    } else {
      cout << "THR34DS, LOOK4H34D: " << endl;
      std::cin >> th >> l4h;
      cout << "P3R1OD, W1DTH, SYMM3TRY, ST4TOR W1DTH: " << endl;
      std::cin >> p >> width >> sym >> stator;
      if (sym != 0 && sym != 1)
        cout << "OTH3R SYMM3TR13S T3MPOR4R1LY NOT T3ST3D >:[" << endl;
      bthh = (sym ? width / 4 : width / 2);
      cout << "B1S3CT1ON THR3SHOLD: [SUGG3ST3D " << bthh << "] " << endl;
      std::cin >> bthh;
      calculateMasks(bthh);
      cout << "F1RST " << 2 * p << " ROWS:" << endl;
      for (int i = 0; i < 2 * p; i++) {
        uint64_t x = 0;
        std::string s;
        std::cin >> s;
        for (int j = 0; j < width; j++)
          if (s[j] == 'o')
            x |= (1ull << j);
        tree.newNode({x, i + 1, i - 1, (i == 2 * p - 1) ? 'q' : 'u', 0});
        tree2.newNode({x, i + 1, i - 1, (i == 2 * p - 1) ? 'q' : 'u', 0});
      }
      cout << "4DD1TION4L OPT1ONS? [Y/N] " << endl;
      std::string options;
      std::cin >> options;
      if (options[0] == 'y' || options[0] == 'Y') {
        cout << "F1LT3R ROWS: " << endl;
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
        cout << "NOD3 L1M1T, D3PTH L1M1T: " << endl;
        std::cin >> nodelim >> deplim;
      }
    }
  }
  for (std::string i : args)
    if (i.substr(0, 2) == "dp")
      deplim = stoi(i.substr(2));
  if (args.count("splitdistrib")) {
    // SPL1T S34RCH TO MUTU4LLY 1ND3P3ND3NT S4V3F1L3S
  }
  if (args.count("search")) {
    for (int i = 0; i < tree.treeSize; i++)
      if (tree.a[i].state == 'q')
        A2B.enqueue({0, i}), qSize++;
    for (int i = 0; i < tree2.treeSize; i++)
      if (tree2.a[i].state == 'q')
        A2B.enqueue({1, i}), qSize++;
    cout << qSize << endl;
    search(th);
    std::ofstream dump("dump.txt");
    dumpf(dump);
    dump.close();
  } else if (args.count("distrib")) {
  }
  return 0;
}