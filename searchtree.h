#pragma once

#include "globals.h"

#include <vector>
#include <unordered_map>

namespace _searchtree {
/* u - unqueued; q - queued; d - decomissioned; c - valid completion for one
 * side */
constexpr int TAG_VALID_COMPLETION = 1;
constexpr int TAG_QUEUED = 2;
constexpr int TAG_REMOTE_QUEUED = 1<<2;
// constexpr int TAG_INDETERMINATE_NODEID = 1<<3;
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
  // TODO: contributor stats
  node(uint8_t v=0, short depth=0, int tags=0, int asc=0) 
    : v(v), depth(depth), tags(tags), asc(asc) {
    ch[0] = ch[1] = ch[2] = ch[3] = 0;
    n[0] = n[1] = 0;
    h[0] = h[1] = nullptr;
  }
  ~node() {
    if(h[0] != nullptr) delete[] h[0];
    if(h[1] != nullptr) delete[] h[1];
  }
  uint64_t dump(int id, std::ostream& f) {
    std::vector<std::unordered_map<uint64_t, int>> rowcomp(2);
    std::vector<std::vector<uint64_t>> rowlis(2);
    std::vector<int> maxasc = {1, 1};
    for(int x=0; x<2; x++) for(int j=0; j<n[x]; j++) {
      maxasc[x] = std::max(maxasc[x], h[x][j].asc + 1);
      if(!rowcomp[x].contains(h[x][j].v))
        rowcomp[x][h[x][j].v] = rowlis[x].size(), rowlis[x].push_back(h[x][j].v);
    }
    arithWriteToStream({(uint64_t)id, (uint64_t)v, (uint64_t)std::max(0,asc), (uint64_t)tags, 
                        (uint64_t)rowlis[0].size(), (uint64_t)n[0], (uint64_t)maxasc[0],
                        (uint64_t)rowlis[1].size(), (uint64_t)n[1], (uint64_t)maxasc[1]},
                        {1ull<<32, 1ull<<8, 1ull<<32, 1ull<<32, 1ull<<32, 1ull<<32, 1ull<<32,
                         1ull<<32, 1ull<<32, 1ull<<32}, f);
    std::vector<uint64_t> vals, maxvals;
    uint64_t cksum = id + asc;
    for(int x=0; x<2; x++) {
      for(auto r : rowlis[x])
        vals.push_back(r), maxvals.push_back(1ull<<63);
      for(int j=0; j<n[x]; j++) {
        vals.push_back(rowcomp[x][h[x][j].v]), maxvals.push_back(rowlis[x].size());
        vals.push_back(h[x][j].asc), maxvals.push_back(maxasc[x]);
        cksum = cksum * 9 + h[x][j].asc * 3 + h[x][j].v;
      }
    }
    arithWriteToStream(vals, maxvals, f);
    return cksum;
  }
  uint64_t load(int& id, std::istream& f) {
    std::vector<uint64_t> header;
    arithReadFromStream(header, {1ull<<32, 1ull<<8, 1ull<<32, 1ull<<32, 1ull<<32, 1ull<<32, 1ull<<32,
                         1ull<<32, 1ull<<32, 1ull<<32}, f);
    id = header[0];
    v = header[1], asc = header[2], tags = header[3];
    if(id == 0 && asc == 0)
      asc = -1;
    n[0] = header[5], n[1] = header[8];
    int maxasc[2] = {(int)header[6], (int)header[9]}, rowlisSize[2] = {(int)header[4], (int)header[7]};
    std::vector<uint64_t> vals, maxvals;
    for(int x=0; x<2; x++) {
      for(int i=0; i<rowlisSize[x]; i++)
        vals.push_back(0), maxvals.push_back(1ull<<63);
      for(int i=0; i<n[x]; i++) {
        vals.push_back(0), maxvals.push_back(rowlisSize[x]);
        vals.push_back(0), maxvals.push_back(maxasc[x]);
      }
    }
    arithReadFromStream(vals, maxvals, f);
    std::reverse(vals.begin(), vals.end());
    std::vector<std::vector<uint64_t>> rowlis(2);
    uint64_t cksum = id + asc;
    for(int x=0; x<2; x++) {
      for(int i=0; i<rowlisSize[x]; i++)
        rowlis[x].push_back(vals.back()), vals.pop_back();
      if (h[x] != nullptr)
        delete[] h[x];
      if (n[x])
        h[x] = new halfrow[n[x]];
      for(int i=0; i<n[x]; i++) {
        h[x][i].v = rowlis[x][vals.back()], vals.pop_back();
        h[x][i].asc = vals.back(), vals.pop_back();
        cksum = cksum * 9 + h[x][i].asc * 3 + h[x][i].v;
      }
    }
    return cksum;
  }
};
#ifdef LARGETREE
const int treeAlloc = 2097152*32;
#else
const int treeAlloc = 2097152;
#endif

struct searchTree {
  node *a;
  int depths[1000];
  int depthcnt[1000];
  int treeSize = 0;
  searchTree() { a = new node[treeAlloc]; }
  ~searchTree() { delete[] a; }
  uint64_t dumpTree(std::ostream &f) {
    f.write("terezi", 6);
    writeInt(treeSize, f);
    uint64_t cksum = 0;
    for (int i = 0; i < treeSize; i++) 
      cksum ^= a[i].dump(i, f);
    f.write("pyrope", 6);
    return cksum;
  }
  uint64_t loadTree(std::istream &f) {
    for (int i = 0; i < 1000; i++)
      depthcnt[i] = depths[i] = 0;
    std::string buf(6, 0);
    f.read(buf.data(), 6);
    assert(buf == "terezi");
    int cnt[2]; cnt[0] = cnt[1] = 0;
    readInt(treeSize, f);
    uint64_t cksum = 0;
    for (int i = 0; i < treeSize; i++) {
      int ii;
      cksum ^= a[i].load(ii, f);
      assert(ii == i);
      a[i].ch[0] = a[i].ch[1] = a[i].ch[2] = a[i].ch[3] = -1;
      if (i)
        a[a[i].asc].ch[a[i].v] = i;
      for (int x = 0; x < 2; x++)
        if (a[i].n[x])
          cnt[x] += a[i].n[x];
      a[i].depth = 1 + (i ? a[a[i].asc].depth : 0);
    }
    f.read(buf.data(), 6);
    assert(buf == "pyrope");
    WARN << "LO4D3D " << treeSize << " NOD3S 4ND " << cnt[0] << "+" << cnt[1] << " ROWS\n";
    for (int i = 0; i < treeSize; i++) {
      depths[a[i].depth] += !!(a[i].tags & TAG_QUEUED), depthcnt[a[i].depth]++;
    }
    return cksum;
  }

  // uint64_t loadWorkUnit(std::ifstream &f) {
  //   for (int i = 0; i < 1000; i++)
  //     depthcnt[i] = depths[i] = 0;
  //   std::string buf(6, 0);
  //   f.read(buf.data(), 6);
  //   assert(buf == "terezi");
  //   readInt(wu_onx, f);
  //   readInt(treeSize, f);
  //   assert(treeSize == 2 * p);
  //   std::vector<int> depthShit(2*p);
  //   for(int i=0; i<2*p; i++) readInt(depthShit[i], f);
  //   uint64_t cksum = 0;
  //   for (int i = 0; i < treeSize; i++) {
  //     int ii;
  //     cksum ^= a[i].load(ii, f);
  //     partialLoadOriginalNodeid[i] = ii;
  //     partialLoadNewNodeid[ii] = i;
  //     a[i].ch[0] = a[i].ch[1] = a[i].ch[2] = a[i].ch[3] = -1;
  //     if (i) {
  //       assert(partialLoadNewNodeid[a[i].asc] == i-1);
  //       a[i].asc = i-1;
  //       a[a[i].asc].ch[a[i].v] = i;
  //     } else a[i].asc = -1;
  //     a[i].depth = depthShit[i];
  //   }
  //   f.read(buf.data(), 6);
  //   assert(buf == "pyrope");
  //   assert(partialLoadOriginalNodeid[2*p-1] == wu_onx);
  //   return cksum;
  // }
  uint64_t dumpWorkUnit(std::ostream &f, int onx) {
    f.write("terezi", 6);
    writeInt(onx, f);
    writeInt(2*p, f);
    assert(a[onx].tags & TAG_QUEUED);
    assert(!(a[onx].tags & TAG_REMOTE_QUEUED));
    a[onx].tags |= TAG_REMOTE_QUEUED;
    std::vector<int> td;
    for(int i=0; i<2*p; i++) td.push_back(onx), onx = a[onx].asc;
    std::reverse(td.begin(), td.end());
    for(int i=0; i<2*p; i++) writeInt(a[td[i]].depth, f);
    uint64_t cksum = 0;
    for (int i = 0; i < 2*p; i++) 
      cksum ^= a[td[i]].dump(td[i], f);
    f.write("pyrope", 6);
    return cksum;
  }
  // uint64_t dumpWorkUnitResponse(std::ofstream &f) {
  //   f.write("terezi", 6);
  //   writeInt(wu_onx, f);
  //   writeInt(treeSize, f);
  //   uint64_t cksum = 0;
  //   for (int i = 2*p; i < treeSize; i++) 
  //     cksum ^= a[i].dump(i, f);
  //   f.write("pyrope", 6);
  //   return cksum;
  // }
  uint64_t loadWorkUnitResponse(std::istream &f) {
    std::string buf(6, 0);
    f.read(buf.data(), 6);
    assert(buf == "terezi");
    readInt(wu_onx, f);
    assert(a[wu_onx].tags & TAG_QUEUED);
    assert((a[wu_onx].tags & TAG_REMOTE_QUEUED)); // TOCHECK
    a[wu_onx].tags ^= TAG_QUEUED;
    a[wu_onx].tags ^= TAG_REMOTE_QUEUED; // TOCHECK
    int xTreeSize;
    readInt(xTreeSize, f);
    uint64_t cksum = 0;
    for (int i = treeSize; i < treeSize + xTreeSize - 2*p; i++) {
      int ii;
      cksum ^= a[i].load(ii, f); assert(ii == i + 2*p - treeSize);
      if(a[i].asc < 2 * p) {
        assert(a[i].asc == 2*p-1);
        a[i].asc = wu_onx;
      } else {
        a[i].asc = a[i].asc + treeSize - 2*p;
      }
      a[i].ch[0] = a[i].ch[1] = a[i].ch[2] = a[i].ch[3] = -1;
      a[a[i].asc].ch[a[i].v] = i;
      a[i].depth = 1 + (i ? a[a[i].asc].depth : 0);
    }
    f.read(buf.data(), 6);
    assert(buf == "pyrope");
    treeSize += xTreeSize - 2*p;
    return cksum;
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
    return treeSize++;
  }
  void filterNode(int onx, std::vector<std::set<int>>& passes) {
    for(int v=0; v<4; v++) assert(a[onx].ch[v] == -1);
    INFO << "node " << onx << " (seq='" << brief(onx) << "'): " << a[onx].n[0] << "+" << a[onx].n[1] << " -> ";
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
    INFO << a[onx].n[0] << "+" << a[onx].n[1] << " ½rs\n";
  }
  std::vector<uint64_t> getState(int onx, int side, int idx, bool full = 0) {
    std::vector<uint64_t> mat;
    while (full ? onx != -1 : (mat.size() != 2 * p))
      mat.push_back(a[onx].h[side][idx].v), idx = a[onx].h[side][idx].asc, onx = a[onx].asc;
    reverse(mat.begin(), mat.end());
    return mat;
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

void loadf(std::istream &f, std::string mode) {
  // BENCHMARK(load)
  readInt(th, f), readInt(l4h, f);
  readInt(p, f), readInt(width, f), readInt(sym, f), readInt(stator, f);
  readInt(bthh, f);
  calculateMasks(bthh);
  int filterrows;
  readInt(filterrows, f);
  arithReadFromStream(filters, std::vector<uint64_t>(filterrows, 1<<width), f);
  filters = std::vector<uint64_t>(filterrows);
  for (int i = 0; i < filterrows; i++)
    readInt(filters[i], f);
  for(int s=0; s<2; s++) {
    int sz; readInt(sz, f);
    arithReadFromStream(leftborder[s], std::vector<uint64_t>(sz, 1<<p), f);
  }
  int eis; readInt(eis, f);
  arithReadFromStream(exInitrow, std::vector<uint64_t>(eis, 1<<p), f);
  uint64_t cksum;
  if(mode == "loadTree") cksum = tree.loadTree(f);
  else if(mode == "loadWorkunit") assert(0); //cksum = tree.loadWorkUnit(f);
  else if(mode == "loadWorkunitResponse") cksum = tree.loadWorkUnitResponse(f);
  else assert(0);
  uint64_t expected_cksum; readInt(expected_cksum, f);
  INFO << "Loaded search tree; checksum " << std::hex << cksum << std::dec << "\n";
  if (cksum != expected_cksum)
    throw std::runtime_error("Checksum mismatch: expected " + std::to_string(expected_cksum) + ", got " + std::to_string(cksum));
  // BENCHMARKEND(load)
}

void dumpf(std::ostream &f, std::string mode, int onx = -1) {
  // BENCHMARK(dump)
  writeInt(th, f); writeInt(l4h, f);
  writeInt(p, f); writeInt(width, f); writeInt(sym, f); writeInt(stator, f);
  writeInt(bthh, f);
  writeInt(filters.size(), f);
  for (auto i : filters)
    writeInt(i, f);
  for(int s=0; s<2; s++) {
    writeInt(leftborder[s].size(), f);
    arithWriteToStream(leftborder[s], std::vector<uint64_t>(leftborder[s].size(), 1<<p), f);
  }
  writeInt(exInitrow.size(), f);
  arithWriteToStream(exInitrow, std::vector<uint64_t>(exInitrow.size(), 1<<p), f);
  uint64_t cksum;
  if(mode == "dumpTree") cksum = tree.dumpTree(f);
  else if(mode == "dumpWorkunit") cksum = tree.dumpWorkUnit(f, onx);
  else if(mode == "dumpWorkunitResponse") assert(0); //cksum = tree.dumpWorkUnitResponse(f);
  else assert(0);
  writeInt(cksum, f);
  f.flush();
  // BENCHMARKEND(dump)
  INFO << "Saved search tree; checksum " << std::hex << cksum << std::dec << "\n";
}