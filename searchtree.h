#pragma once

#include "globals.h"

namespace _searchtree {
/* u - unqueued; q - queued; d - decomissioned; c - valid completion for one
 * side */
constexpr int TAG_VALID_COMPLETION = 1;
constexpr int TAG_RETURNING_NODE_TREE2 = (1 << 30);
struct node {
  uint64_t row;
  int depth, parent;
  char state;
  int tags;
};
struct searchTree {
  node *a;
  int depths[1000];
  int depthcnt[1000];
  int treeSize = 0, treeAlloc = 16777216;
  searchTree() { a = new node[treeAlloc]; }
  ~searchTree() { delete a; }
  void dumpTree(std::ostream &f) {
    f << "BEGINTREE\n";
    f << treeSize << "\n";
    for (int i = 0; i < treeSize; i++)
      f << a[i].row << ' ' << a[i].parent << ' ' << a[i].state << ' '
        << a[i].tags << '\n';
    f << "ENDTREE\n";
  }
  void loadTree(std::istream &f) {
    for (int i = 0; i < 1000; i++)
      depthcnt[i] = depths[i] = 0;
    std::string buf;
    f >> buf;
    assert(buf == "BEGINTREE");
    f >> treeSize;
    for (int i = 0; i < treeSize; i++) {
      f >> a[i].row >> a[i].parent >> a[i].state >> a[i].tags;
      a[i].depth = 1 + (i ? a[a[i].parent].depth : 0);
    }
    f >> buf;
    assert(buf == "ENDTREE");
    for (int i = 1; i < treeSize; i++)
      if (a[a[i].parent].state == 'd')
        a[i].state = 'd';
    int cnt = 0;
    for (int i = 1; i < treeSize; i++)
      if (a[i].state != 'd' &&
          (a[a[i].parent].state == 'd' || a[a[i].parent].state == 'q'))
        a[i].state = 'd', cnt++;
    if (cnt != 0)
      cout << "[1NFO] " << cnt << " NOD3S 1NV4L1D4T3D" << endl;
    for (int i = 0; i < treeSize; i++) {
      depths[a[i].depth] += (a[i].state == 'q'), depthcnt[a[i].depth]++;
    }
  }
  void flushTree() {}
  int newNode(node s = {0, 0, 0, ' ', 0}) {
    a[treeSize] = s;
    depthcnt[s.depth]++;
    if (s.state == 'q')
      depths[s.depth]++;
    return treeSize++;
  }
  bool shouldSkip(int onx) {
    while (onx != -1) {
      if (a[onx].state == 'd')
        return 1;
      onx = a[onx].parent;
    }
    return 0;
  }
  std::vector<uint64_t> getState(int onx, bool full = 0) {
    std::vector<uint64_t> mat;
    while (onx != -1)
      mat.push_back(a[onx].row), onx = a[onx].parent;
    reverse(mat.begin(), mat.end());
    return std::vector<uint64_t>(full ? mat.begin() : (mat.end() - 2 * p),
                                 mat.end());
  }
  int getWidth(int i) {
    uint64_t bito = 0;
    for (uint64_t x : getState(i))
      bito |= x;
    return 64 - __builtin_clzll(bito) - __builtin_ctzll(bito);
  }
} tree, tree2;

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
  tree2.loadTree(f);
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
  tree2.dumpTree(f);
  f.flush();
}