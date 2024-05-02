#pragma once

#include "globals.h"

#include <vector>

namespace _searchtree {
/* u - unqueued; q - queued; d - decomissioned; c - valid completion for one
 * side */
constexpr int TAG_VALID_COMPLETION = 1;
constexpr int TAG_QUEUED = 2;
constexpr int TAG_RETURNING_NODE_TREE2 = (1 << 30);
struct halfrow { 
  uint64_t v;
  int asc;
}; // 96bits per internal row
struct node {
  uint8_t v; // 8bits (2bits effective)
  short depth; // 16bits
  int tags; // 32bits
  int asc, ch[4]; // 32*5bits
  int n[2]; // 32*2bits
  halfrow* h[2]; // 64*2bits
  node(uint8_t v=0, short depth=0, int tags=0, int asc=0) 
    : v(v), depth(depth), tags(tags), asc(asc) {
    ch[0] = ch[1] = ch[2] = ch[3] = 0;
    n[0] = n[1] = 0;
    h[0] = h[1] = nullptr;
  }
  ~node() {
    // if(h[0] != nullptr || h[1] != nullptr) WARN << h[0] << ' ' << h[1] << '\n';
    if(h[0] != nullptr) delete[] h[0];
    if(h[1] != nullptr) delete[] h[1];
  }
};
struct searchTree {
  node *a;
  int depths[1000];
  int depthcnt[1000];
  int treeSize = 0, treeAlloc = 2097152;
  searchTree() { a = new node[treeAlloc]; }
  ~searchTree() { delete[] a; }
  void dumpTree(std::ostream &f) {
    f << "BEGINTREE\n";
    f << treeSize << "\n";
    for (int i = 0; i < treeSize; i++) {
      f << i << ' ' << (int)(a[i].v) << ' ' << a[i].asc << ' ' << a[i].tags << '\n';
      for(int x = 0; x < 2; x++) {
        f << a[i].n[x];
        for (int j = 0; j < a[i].n[x]; j++)
          f << ' ' << a[i].h[x][j].v << ' ' << a[i].h[x][j].asc;
        f << '\n';
      }
    }
    f << "ENDTREE\n";
  }
  void loadTree(std::istream &f) {
    for (int i = 0; i < 1000; i++)
      depthcnt[i] = depths[i] = 0;
    std::string buf;
    f >> buf;
    assert(buf == "BEGINTREE");
    int cnt[2]; cnt[0] = cnt[1] = 0;
    f >> treeSize;
    for (int i = 0; i < treeSize; i++) {
      int ii, v;
      f >> ii >> v >> a[i].asc >> a[i].tags;
      assert(ii == i);
      a[i].v = v;
      a[i].ch[0] = a[i].ch[1] = a[i].ch[2] = a[i].ch[3] = -1;
      if (i)
        a[a[i].asc].ch[a[i].v] = i;
      for (int x = 0; x < 2; x++) {
        f >> a[i].n[x];
        if (a[i].n[x])
          a[i].h[x] = new halfrow[a[i].n[x]], cnt[x] += a[i].n[x];
        for (int j = 0; j < a[i].n[x]; j++)
          f >> a[i].h[x][j].v >> a[i].h[x][j].asc;
      }
      a[i].depth = 1 + (i ? a[a[i].asc].depth : 0);
    }
    f >> buf;
    assert(buf == "ENDTREE");
    WARN << "LO4D3D " << treeSize << " NOD3S 4ND " << cnt[0] << "+" << cnt[1] << " ROWS\n";
    for (int i = 0; i < treeSize; i++) {
      depths[a[i].depth] += !!(a[i].tags & TAG_QUEUED), depthcnt[a[i].depth]++;
    }
  }
  void flushTree() {}
  int newNode(node s, std::vector<std::vector<halfrow>> bufs) {
    assert(s.asc != -1 || treeSize == 0);
    s.ch[0] = s.ch[1] = s.ch[2] = s.ch[3] = -1;
    a[treeSize] = s;
    for(int x=0; x<2; x++){
      a[treeSize].n[x] = bufs[x].size();
      if (a[treeSize].n[x]) {
        a[treeSize].h[x] = new halfrow[a[treeSize].n[x]];
        std::copy(bufs[x].begin(), bufs[x].end(), a[treeSize].h[x]);
        // WARN << treeSize << ' ' << a[treeSize].h[x][0].v << ' ' << a[treeSize].h[x][0].asc << '\n';
        // WARN << bufs[x][0].v << ' ' << bufs[x][0].asc << '\n';
      }
    }
    if (s.asc != -1 && a[s.asc].ch[s.v] != -1) {
      WARN << "F4T4L: N3W NOD3 " << treeSize << " DUPLIC4T3S KNOWN S3QU3NC3\n";
      assert(a[s.asc].ch[s.v] == -1);
    }
    if (s.asc != -1)
      a[s.asc].ch[s.v] = treeSize;
    depthcnt[s.depth]++;
    if (s.tags & TAG_QUEUED)
      depths[s.depth]++;
    // WARN << a[treeSize].h[0] << ' ' << a[treeSize].h[0][0].v << '\n';
    return treeSize++;
  }
  void filterNode(int onx, std::vector<std::set<int>>& passes) {
    for(int v=0; v<4; v++) assert(a[onx].ch[v] == -1);
    INFO << std::format("node {} (seq='{}'): {}+{} -> ", onx, brief(onx), a[onx].n[0], a[onx].n[1]);
    for(int x=0; x<2; x++){
      halfrow* nh = nullptr;
      if(passes[x].size()) {
        nh = new halfrow[passes[x].size()];
        std::vector<int> idxs(passes[x].begin(), passes[x].end());
        for(int i=0; i<(int)passes[x].size(); i++)
          nh[i] = a[onx].h[x][idxs[i]];
      }
      if(a[onx].h[x] != nullptr) delete[] a[onx].h[x];
      a[onx].n[x] = passes[x].size(), a[onx].h[x] = nh;
    }
    INFO << std::format("{}+{} ½rs\n", a[onx].n[0], a[onx].n[1]);
  }
  std::vector<uint64_t> getState(int onx, int side, int idx, bool full = 0) {
    std::vector<uint64_t> mat;
    while (onx != -1) {
      // WARN << onx << ' ' << side << ' ' << idx << ' ' << a[onx].h[side][idx].v << ' ' << a[onx].h[side][idx].asc << '\n';
      mat.push_back(a[onx].h[side][idx].v), idx = a[onx].h[side][idx].asc, onx = a[onx].asc;
    }
    reverse(mat.begin(), mat.end());
    return std::vector<uint64_t>(full ? mat.begin() : (mat.end() - 2 * p),
                                 mat.end());
  }
  std::string brief(int onx) {
    std::string res;
    while(onx != -1)
      res += a[onx].v + '0', onx = a[onx].asc;
    reverse(res.begin(), res.end());
    return std::string(res.begin() + 2*p, res.end());
  }
  int getWidth(int i, int side, int idx) {
    uint64_t bito = 0;
    for (uint64_t x : getState(i, side, idx))
      bito |= x;
    return 64 - __builtin_clzll(bito) - __builtin_ctzll(bito);
  }
} tree;

}; // namespace _searchtree
using namespace _searchtree;

void loadf(std::istream &f) {
  f >> th >> l4h;
  f >> p >> width >> sym >> stator;
  f >> bthh;
  calculateMasks(bthh);
  int filterrows;
  f >> filterrows;
  filters = std::vector<uint64_t>(filterrows);
  for (int i = 0; i < filterrows; i++)
    f >> filters[i];
  tree.loadTree(f);
}

void dumpf(std::ostream &f) {
  f << th << ' ' << l4h << '\n';
  f << p << ' ' << width << ' ' << sym << ' ' << stator << '\n';
  f << bthh << '\n';
  f << filters.size() << '\n';
  for (auto i : filters)
    f << i << ' ';
  f << '\n';
  tree.dumpTree(f);
  f.flush();
}