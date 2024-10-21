#pragma once

#include <cassert>
#include <cmath>
#include <vector>
#include <cstdint>
#include <mutex>

#include "globals.h"

namespace _logic {

struct cacheKey {
  uint64_t enforce; // 1shifted
  uint64_t remember;
  int row; // -1: generic
  int useless; // number of bits to shit a subset of remember so that it uniquely determines it up to symmetry
  // e.g. 0b00111000011100 -> useless = 9
  int wid; // width of nonuseless part i.e. 3 for above
  uint64_t re2; // other remember
  const bool operator< (const cacheKey& o) const {
    return std::make_tuple(enforce, row) <
           std::make_tuple(o.enforce, o.row);
  }
};

std::mutex cacheMutex;

uint64_t** getCache(const cacheKey& k) {
  static std::map<cacheKey, uint64_t**> builtCaches;
  static thread_local std::map<cacheKey, uint64_t**> localBuiltCaches;
  if(localBuiltCaches.contains(k))
    return localBuiltCaches[k];
  const std::lock_guard<std::mutex> lock(cacheMutex);
  if(!builtCaches.contains(k)) {
    uint64_t** v = new uint64_t*[1ull<<(k.wid*2)];
    for(size_t i=0; i<(1ull<<(k.wid*2)); i++)
      v[i] = nullptr;
    localBuiltCaches[k] = v;
    return builtCaches[k] = v;
  }
  localBuiltCaches[k] = builtCaches[k];
  return builtCaches[k];
}

// solutions x to ev(r1, r2, x) = r3, r1 r2 r3 as prescribed by remember, under enforce conditions
/*
more precisely:
r1 r2 x r3 all subsets of remember
x situated at row (>= 2*p)
enforce a shifted subset of {-1 ... wid} describes columns where (r1 r2 x) -> r3 must be satisfied
*/
// in any case elements of p always satisfy [r1 r2 el -> r3] under r2 constraints under enforce constraints

std::mutex cacheConstructionMutex;

uint64_t* constructCacheIndex(const cacheKey& k, uint64_t r1, uint64_t r2) {
  // depth = k.row (ie, depth of el, r1 is depth - 2p)
  int depth = k.row;
  int phase = depth % p;
  int row = p * 2;
  int r = (row + phase) / p, t = (row + phase) % p;
  int C = (r - 1) * p + (t + 1) % p - phase;
  // DEBUG << k.remember << ' ' << r1 << ' ' << r2 << '\n';
  assert(((r1 & k.remember) == r1) && ((r2 & k.remember) == r2));
  std::map<uint64_t, std::vector<uint64_t>> evx; int evxCnt = 0;
  bool alt = false;
  auto get = [&](int row, uint64_t xv, int j) {
    int realRow = (row + depth - 2 * p) / p;
    if (j <= -1 && (realRow < (int)leftborder[-j-1].size()))
      return (leftborder[-j-1][realRow] & (1ull << ((row + depth) % p))) ? 1 : 0;
    if (j <= -1 || j >= width)
      return 0;
    return +!!(((alt && realRow < 2) ? exInitrow[row + depth - 2 * p] : xv) & (1ull << j));
  };
  for (uint64_t x = k.remember; ; x = (x-1) & k.remember) {
    // Check: from (r1, r2, x) what can be determined about r3?
    uint64_t det = 0, r3 = 0;
    if(depth / p < filters.size() && ((x & filters[depth / p]) != x)) goto fail;
    // INFO << x << ' ';
    for (int j = -1; j <= width; j++)
      if (k.enforce & (1ull << (j+1))) {
        if(j == -1 || j == width) {
          std::vector<int> ar = {get(0, r1, j - 1), get(0, r1, j), get(0, r1, j + 1),
                                 get(p, r2, j - 1), get(p, r2, j), get(p, r2, j+1),
                                 get(2*p, x, j - 1), get(2*p, x, j), get(2*p, x, j+1), get(C, 0, j)};
          int s = 0;
          for (int i = 0; i < 10; i++) if (ar[i]) s |= (1 << i);
          if(!table[s]) goto fail;
        } else {
          std::vector<int> ar = {get(0, r1, j - 1), get(0, r1, j), get(0, r1, j + 1),
                                 get(p, r2, j - 1), get(p, r2, j), get(p, r2, j+1),
                                 get(2*p, x, j - 1), get(2*p, x, j), get(2*p, x, j+1)};
          int s = 0;
          for (int i = 0; i < 9; i++) if (ar[i]) s |= (1 << i);
          int cell = table[s + 512]; // 512 = check if result is on
          det |= (1ull << j), r3 |= ((1ull*cell)<<j);
        }
        if (exInitrow.size()) {
          alt = true;
          std::vector<int> a2 = {get(0, r1, j - 1), get(0, r1, j), get(0, r1, j + 1),
                                 get(p, r2, j - 1), get(p, r2, j), get(p, r2, j+1),
                                 get(2*p, x, j - 1), get(2*p, x, j), get(2*p, x, j+1), get(C, r3, j)};
          alt = false;
          int s = 0;
          for (int i = 0; i < 10; i++) if (a2[i]) s |= (1 << i);
          if (!table[s]) {
            // WARN << x << ' ';
            goto fail;
          }
        }
      }
    assert((det & k.remember) == det); // det is submask of k.remember
    assert(__builtin_popcountll(k.remember ^ det) <= 2); // todo: adjust for sym
    for (uint64_t alt = k.remember ^ det; ; alt = (alt - 1) & (k.remember ^ det)) {
      evx[(r3 | alt) >> k.useless].push_back(x), evxCnt++;
      // DEBUG << x << ' ';
      if (alt == 0) break;
    }
    fail:;
    if(x == 0) break;
  }
  // DEBUG << '\n';
  uint64_t* res = new uint64_t[(1ull << k.wid) + 1 + evxCnt];
  // res format: first [2^wid+1] elements describe positions of candidate rows, evxCnt describes actual candidate rows
  res[0] = (1ull << k.wid) + 1;
  int sh = __builtin_ctzll(k.remember & k.re2);
  for(uint64_t v = 0; v < (1ull << k.wid); v++) {
    res[v + 1] = res[v] + evx[v].size();
    std::copy(evx[v].begin(), evx[v].end(), res + res[v]);
    uint64_t sign = 0;
    for(uint64_t j : evx[v]) {
      int ctr = (j & k.re2) >> sh;
      assert(ctr < 4);
      sign |= (1 << ctr);
    }
    res[v] |= (sign << 32);
  }

  // ...
  // DEBUG << std::to_string(evxCnt)+'\n';
  return res;
}

// int altlogic_brq = 0;

inline std::tuple<uint64_t*, int, uint32_t> qryLookup(const cacheKey& k, uint64_t** rows, uint64_t r1, uint64_t r2, uint64_t r3) {
  // DEBUG << r1 << ' ' << r2 << ' ' << r3 << ' ' << k.remember << '\n';
  r1 &= k.remember;
  r2 &= k.remember;
  r3 &= k.remember;
  // DEBUG << r1 << ' ' << r2 << ' ' << r3 << ' ' << k.remember << ' ' << (r2 & k.remember) << '\n';
  uint64_t*& i = rows[((r1 >> k.useless) << k.wid) | (r2 >> k.useless)];
  if(i == nullptr) {
    uint64_t* row = constructCacheIndex(k, r1, r2);
    {
      const std::lock_guard<std::mutex> lock(cacheConstructionMutex);
      // altlogic_brq++;
      i = row;
    }
  }
  r3 >>= k.useless;
  return {i + ((uint32_t)i[r3]), (int)(((uint32_t)i[r3 + 1]) - ((uint32_t)i[r3])), (uint32_t)(i[r3]>>32)};
}

bool _lookahead(const uint64_t* state, int depth, uint64_t enf_A, uint64_t enf_B, uint64_t rem_A, uint64_t rem_B) {
  // static uint64_t lhcnt = 0;
  // lhcnt++;
  // if(lhcnt % 2048 == 0) WARN << lhcnt << '\n';
  int phase = depth % p;
  int row = p * 2;
  int r = (row + phase) / p, t = (row + phase) % p;
  int C = (r - 1) * p + (t + 1) % p - phase;
  auto gck = [&](int depth) {
    return std::make_pair(cacheKey{enf_A, rem_A, depth, __builtin_ctzll(rem_A), __builtin_popcountll(rem_A), rem_B},
                          cacheKey{enf_B, rem_B, depth, __builtin_ctzll(rem_B), __builtin_popcountll(rem_B), rem_A});
  };
  auto gckc = [&](const cacheKey& k) { return getCache(k); };
  const auto & [ck1_A, ck1_B] = gck(depth);
  auto ckc1_A = gckc(ck1_A), ckc1_B = gckc(ck1_B);
  auto [h1_A, hn1_A, hs1_A] = qryLookup(ck1_A, ckc1_A, state[0], state[p], state[C]);
  auto [h1_B, hn1_B, hs1_B] = qryLookup(ck1_B, ckc1_B, state[0], state[p], state[C]);
  if (!(hs1_A & hs1_B)) return false;
  if(phase >= 3) {
    // static double avg = 0.0; static int cnt = 0;
    // int innermost = 0;
    // auto t1 = std::chrono::high_resolution_clock::now();
    const auto & [ck2_A, ck2_B] = gck(depth + p - 1);
    auto ckc2_A = gckc(ck2_A), ckc2_B = gckc(ck2_B);
    const auto & [ck3_A, ck3_B] = gck(depth + p - 2);
    auto ckc3_A = gckc(ck3_A), ckc3_B = gckc(ck3_B);
    const auto & [ck4_A, ck4_B] = gck(depth + p - 3);
    auto ckc4_A = gckc(ck4_A), ckc4_B = gckc(ck4_B);
    const auto & [ck5_A, ck5_B] = gck(depth + 2 * p - 2);
    auto ckc5_A = gckc(ck5_A), ckc5_B = gckc(ck5_B);
    const auto & [ck6_A, ck6_B] = gck(depth + 2 * p - 3);
    auto ckc6_A = gckc(ck6_A), ckc6_B = gckc(ck6_B);
    const auto & [ck7_A, ck7_B] = gck(depth + 3 * p - 3);
    auto ckc7_A = gckc(ck7_A), ckc7_B = gckc(ck7_B);
    auto [h3_A, hn3_A, hs3_A] = qryLookup(ck3_A, ckc3_A, state[p - 2], state[2 * p - 2], state[2 * p - 1]);
    auto [h3_B, hn3_B, hs3_B] = qryLookup(ck3_B, ckc3_B, state[p - 2], state[2 * p - 2], state[2 * p - 1]);
    auto [h4_A, hn4_A, hs4_A] = qryLookup(ck4_A, ckc4_A, state[p - 3], state[2 * p - 3], state[2 * p - 2]);
    auto [h4_B, hn4_B, hs4_B] = qryLookup(ck4_B, ckc4_B, state[p - 3], state[2 * p - 3], state[2 * p - 2]);
    if(!(hs3_A & hs3_B) || !(hs4_A & hs4_B)) return false;
    std::vector<uint64_t> r4s;
    for (int i4 = 0; i4 < hn4_A; i4++)
      for (int j4 = 0; j4 < hn4_B; j4++)
        if ((h4_A[i4] & rem_B) == (h4_B[j4] & rem_A))
          r4s.push_back(h4_A[i4] | h4_B[j4]);
    if(!hn1_A || !hn1_B || !hn3_A || !hn3_B || !r4s.size()) return false;
    for (int i = 0; i < hn1_A; i++)
      for (int j = 0; j < hn1_B; j++)
        if ((h1_A[i] & rem_B) == (h1_B[j] & rem_A)) {
          // return true;
          uint64_t r1 = h1_A[i] | h1_B[j];
          auto [h2_A, hn2_A, hs2_A] = qryLookup(ck2_A, ckc2_A, state[p - 1], state[2 * p - 1], r1);
          auto [h2_B, hn2_B, hs2_B] = qryLookup(ck2_B, ckc2_B, state[p - 1], state[2 * p - 1], r1);
          if (!(hs2_A & hs2_B)) continue;
          for (int i2 = 0; i2 < hn2_A; i2++)
            for (int j2 = 0; j2 < hn2_B; j2++)
              if ((h2_A[i2] & rem_B) == (h2_B[j2] & rem_A)) {
                uint64_t r2 = h2_A[i2] | h2_B[j2];
                for (int i3 = 0; i3 < hn3_A; i3++)
                  for (int j3 = 0; j3 < hn3_B; j3++)
                    if ((h3_A[i3] & rem_B) == (h3_B[j3] & rem_A)) {
                      uint64_t r3 = h3_A[i3] | h3_B[j3];
                      auto [h5_A, hn5_A, hs5_A] = qryLookup(ck5_A, ckc5_A, state[2 * p - 2], r3, r2);
                      auto [h5_B, hn5_B, hs5_B] = qryLookup(ck5_B, ckc5_B, state[2 * p - 2], r3, r2);
                      if (!(hs5_A & hs5_B)) continue;
                      for (int i5 = 0; i5 < hn5_A; i5++)
                        for (int j5 = 0; j5 < hn5_B; j5++)
                          if ((h5_A[i5] & rem_B) == (h5_B[j5] & rem_A)) {
                            uint64_t r5 = h5_A[i5] | h5_B[j5];
                            for (uint64_t r4 : r4s) {
                              auto [h6_A, hn6_A, hs6_A] = qryLookup(ck6_A, ckc6_A, state[2 * p - 3], r4, r3);
                              auto [h6_B, hn6_B, hs6_B] = qryLookup(ck6_B, ckc6_B, state[2 * p - 3], r4, r3);
                              if (!(hs6_A & hs6_B)) continue;
                              for (int i6 = 0; i6 < hn6_A; i6++)
                                for (int j6 = 0; j6 < hn6_B; j6++)
                                  if ((h6_A[i6] & rem_B) == (h6_B[j6] & rem_A)) {
                                    uint64_t r6 = h6_A[i6] | h6_B[j6];
                                    auto [h7_A, hn7_A, hs7_A] = qryLookup(ck7_A, ckc7_A, r4, r6, r5);
                                    auto [h7_B, hn7_B, hs7_B] = qryLookup(ck7_B, ckc7_B, r4, r6, r5);
                                    if (hs7_A & hs7_B)
                                      return true;
                                  }
                            }
                          }
                    }
              }
        }
    // auto t2 = std::chrono::high_resolution_clock::now();
    // // avg = avg * 0.995 + (t2 - t1).count() * 0.005;
    // avg = avg * 0.995 + innermost * 0.005;
    // cnt++;
    // if(cnt % 1024 == 0) WARN << avg << '\n';
  } else if (phase == 2) {
    const auto & [ck2_A, ck2_B] = gck(depth + p - 1);
    auto ckc2_A = gckc(ck2_A), ckc2_B = gckc(ck2_B);
    const auto & [ck3_A, ck3_B] = gck(depth + p - 2);
    auto ckc3_A = gckc(ck3_A), ckc3_B = gckc(ck3_B);
    const auto & [ck4_A, ck4_B] = gck(depth + 2 * p - 2);
    auto ckc4_A = gckc(ck4_A), ckc4_B = gckc(ck4_B);
    auto [h3_A, hn3_A, hs3_A] = qryLookup(ck3_A, ckc3_A, state[p - 2], state[2 * p - 2], state[2 * p - 1]);
    auto [h3_B, hn3_B, hs3_B] = qryLookup(ck3_B, ckc3_B, state[p - 2], state[2 * p - 2], state[2 * p - 1]);
    if(!hn1_A || !hn1_B || !(hs3_A & hs3_B)) return false;
    for (int i = 0; i < hn1_A; i++)
      for (int j = 0; j < hn1_B; j++)
        if ((h1_A[i] & rem_B) == (h1_B[j] & rem_A)) {
          uint64_t r1 = h1_A[i] | h1_B[j];
          auto [h2_A, hn2_A, hs2_A] = qryLookup(ck2_A, ckc2_A, state[p - 1], state[2 * p - 1], r1);
          auto [h2_B, hn2_B, hs2_B] = qryLookup(ck2_B, ckc2_B, state[p - 1], state[2 * p - 1], r1);
          if(!(hs2_A & hs2_B)) continue;
          for (int i2 = 0; i2 < hn2_A; i2++)
            for (int j2 = 0; j2 < hn2_B; j2++)
              if ((h2_A[i2] & rem_B) == (h2_B[j2] & rem_A)) {
                uint64_t r2 = h2_A[i2] | h2_B[j2];
                for (int i3 = 0; i3 < hn3_A; i3++)
                  for (int j3 = 0; j3 < hn3_B; j3++)
                    if ((h3_A[i3] & rem_B) == (h3_B[j3] & rem_A)) {
                      uint64_t r3 = h3_A[i3] | h3_B[j3];
                      auto [h4_A, hn4_A, hs4_A] = qryLookup(ck4_A, ckc4_A, state[2 * p - 2], r3, r2);
                      auto [h4_B, hn4_B, hs4_B] = qryLookup(ck4_B, ckc4_B, state[2 * p - 2], r3, r2);
                      if (hs4_A & hs4_B) return true;
                    }
              }
        }
  } else if (phase == 1) {
    const auto & [ck2_A, ck2_B] = gck(depth + p - 2);
    auto ckc2_A = gckc(ck2_A), ckc2_B = gckc(ck2_B);
    const auto & [ck3_A, ck3_B] = gck(depth + p - 1);
    auto ckc3_A = gckc(ck3_A), ckc3_B = gckc(ck3_B);
    const auto & [ck4_A, ck4_B] = gck(depth + 2 * p - 2);
    auto ckc4_A = gckc(ck4_A), ckc4_B = gckc(ck4_B);
    const auto & [ck5_A, ck5_B] = gck(depth + 3 * p - 2);
    auto ckc5_A = gckc(ck5_A), ckc5_B = gckc(ck5_B);
    auto [h2_A, hn2_A, hs2_A] = qryLookup(ck2_A, ckc2_A, state[p - 2], state[2 * p - 2], state[p - 1]);
    auto [h2_B, hn2_B, hs2_B] = qryLookup(ck2_B, ckc2_B, state[p - 2], state[2 * p - 2], state[p - 1]);
    if(!hn1_A || !hn1_B || !(hs2_A & hs2_B)) return false;
    for (int i = 0; i < hn1_A; i++)
      for (int j = 0; j < hn1_B; j++)
        if ((h1_A[i] & rem_B) == (h1_B[j] & rem_A)) {
          // return true;
          uint64_t r1 = h1_A[i] | h1_B[j];
          for (int i2 = 0; i2 < hn2_A; i2++)
            for (int j2 = 0; j2 < hn2_B; j2++)
              if ((h2_A[i2] & rem_B) == (h2_B[j2] & rem_A)) {
                uint64_t r2 = h2_A[i2] | h2_B[j2];
                auto [h3_A, hn3_A, hs3_A] = qryLookup(ck3_A, ckc3_A, state[p - 1], state[2 * p - 1], r1);
                auto [h3_B, hn3_B, hs3_B] = qryLookup(ck3_B, ckc3_B, state[p - 1], state[2 * p - 1], r1);
                if (!(hs3_A & hs3_B)) continue;
                for (int i3 = 0; i3 < hn3_A; i3++)
                  for (int j3 = 0; j3 < hn3_B; j3++)
                    if ((h3_A[i3] & rem_B) == (h3_B[j3] & rem_A)) {
                      uint64_t r3 = h3_A[i3] | h3_B[j3];
                      auto [h4_A, hn4_A, hs4_A] = qryLookup(ck4_A, ckc4_A, state[2 * p - 2], r2, state[2 * p - 1]);
                      auto [h4_B, hn4_B, hs4_B] = qryLookup(ck4_B, ckc4_B, state[2 * p - 2], r2, state[2 * p - 1]);
                      if (!(hs4_A & hs4_B)) continue;
                      for (int i4 = 0; i4 < hn4_A; i4++)
                        for (int j4 = 0; j4 < hn4_B; j4++)
                          if ((h4_A[i4] & rem_B) == (h4_B[j4] & rem_A)) {
                            uint64_t r4 = h4_A[i4] | h4_B[j4];
                            auto [h5_A, hn5_A, hs5_A] = qryLookup(ck5_A, ckc5_A, r2, r4, r3);
                            auto [h5_B, hn5_B, hs5_B] = qryLookup(ck5_B, ckc5_B, r2, r4, r3);
                            if (hs5_A & hs5_B) return true;
                          }
                    }
              }
        }
  } else {
    assert(phase == 0);
    const auto & [ck2_A, ck2_B] = gck(depth + p - 1);
    auto ckc2_A = gckc(ck2_A), ckc2_B = gckc(ck2_B);
    const auto & [ck3_A, ck3_B] = gck(depth + p - 2);
    auto ckc3_A = gckc(ck3_A), ckc3_B = gckc(ck3_B);
    const auto & [ck4_A, ck4_B] = gck(depth + 2 * p - 1);
    auto ckc4_A = gckc(ck4_A), ckc4_B = gckc(ck4_B);
    const auto & [ck5_A, ck5_B] = gck(depth + 2 * p - 2);
    auto ckc5_A = gckc(ck5_A), ckc5_B = gckc(ck5_B);
    const auto & [ck6_A, ck6_B] = gck(depth + 3 * p - 2);
    auto ckc6_A = gckc(ck6_A), ckc6_B = gckc(ck6_B);
    auto [h2_A, hn2_A, hs2_A] = qryLookup(ck2_A, ckc2_A, state[p - 1], state[2 * p - 1], state[p]);
    auto [h2_B, hn2_B, hs2_B] = qryLookup(ck2_B, ckc2_B, state[p - 1], state[2 * p - 1], state[p]);
    auto [h3_A, hn3_A, hs3_A] = qryLookup(ck3_A, ckc3_A, state[p - 2], state[2 * p - 2], state[2 * p - 1]);
    auto [h3_B, hn3_B, hs3_B] = qryLookup(ck3_B, ckc3_B, state[p - 2], state[2 * p - 2], state[2 * p - 1]);
    if(!(hs2_A & hs2_B) || !(hs3_A & hs3_B)) return false;
    for (int i = 0; i < hn1_A; i++)
      for (int j = 0; j < hn1_B; j++)
        if ((h1_A[i] & rem_B) == (h1_B[j] & rem_A)) {
          // return true;
          uint64_t r1 = h1_A[i] | h1_B[j];
          for (int i2 = 0; i2 < hn2_A; i2++)
            for (int j2 = 0; j2 < hn2_B; j2++)
              if ((h2_A[i2] & rem_B) == (h2_B[j2] & rem_A)) {
                uint64_t r2 = h2_A[i2] | h2_B[j2];
                for (int i3 = 0; i3 < hn3_A; i3++)
                  for (int j3 = 0; j3 < hn3_B; j3++)
                    if ((h3_A[i3] & rem_B) == (h3_B[j3] & rem_A)) {
                      uint64_t r3 = h3_A[i3] | h3_B[j3];
                      auto [h4_A, hn4_A, hs4_A] = qryLookup(ck4_A, ckc4_A, state[2 * p - 1], r2, r1);
                      auto [h4_B, hn4_B, hs4_B] = qryLookup(ck4_B, ckc4_B, state[2 * p - 1], r2, r1);
                      if (!(hs4_A & hs4_B)) continue;
                      for (int i4 = 0; i4 < hn4_A; i4++)
                        for (int j4 = 0; j4 < hn4_B; j4++)
                          if ((h4_A[i4] & rem_B) == (h4_B[j4] & rem_A)) {
                            uint64_t r4 = h4_A[i4] | h4_B[j4];
                            auto [h5_A, hn5_A, hs5_A] = qryLookup(ck5_A, ckc5_A, state[2 * p - 2], r3, r2);
                            auto [h5_B, hn5_B, hs5_B] = qryLookup(ck5_B, ckc5_B, state[2 * p - 2], r3, r2);
                            if (!(hs5_A & hs5_B)) continue;
                            for (int i5 = 0; i5 < hn5_A; i5++)
                              for (int j5 = 0; j5 < hn5_B; j5++)
                                if ((h5_A[i5] & rem_B) == (h5_B[j5] & rem_A)) {
                                  uint64_t r5 = h5_A[i5] | h5_B[j5];
                                  auto [h6_A, hn6_A, hs6_A] = qryLookup(ck6_A, ckc6_A, r3, r5, r4);
                                  auto [h6_B, hn6_B, hs6_B] = qryLookup(ck6_B, ckc6_B, r3, r5, r4);
                                  if (hs6_A & hs6_B) return true;
                                }
                          }
                    }
              }
        }
  }
  return false;
}

void genNextRows(std::vector<uint64_t> &state, int depth, int ahead,
                 uint64_t enforce, uint64_t remember, auto fn) {
  // ahead = 1 test
  assert(ahead == 1);
  assert(p * 2 == sz(state));
  int phase = depth % p;
  int row = p * 2;
  int r = (row + phase) / p, t = (row + phase) % p;
  int C = (r - 1) * p + (t + 1) % p - phase;
  // split enforce and remember about half-half
  uint64_t enf_A = 0, enf_B = 0;
  uint64_t rem_A = 0, rem_B = 0;
  // TOFIX adjust for symmetry
  int targ = __builtin_popcountll(enforce), cnt = 0;
  
  for (int j = -1; j <= width; j++)
    if (enforce & (1ull << (j+1))) {
      cnt += 2;
      if(cnt < targ) enf_A |= (1ull << (j+1));
      else enf_B |= (1ull << (j+1));
    }
  for (int j = 0; j < width; j++)
    if (remember & (1ull << j)) {
      if (enf_A & (7ull << j)) rem_A |= (1ull << j);
      if (enf_B & (7ull << j)) rem_B |= (1ull << j);
    }
  auto gck = [&](int depth) {
    return std::make_pair(cacheKey{enf_A, rem_A, depth, __builtin_ctzll(rem_A), __builtin_popcountll(rem_A), rem_B},
                          cacheKey{enf_B, rem_B, depth, __builtin_ctzll(rem_B), __builtin_popcountll(rem_B), rem_A});
  };
  auto gckc = [&](const cacheKey& k) { return getCache(k); };
  const auto & [ck1_A, ck1_B] = gck(depth);
  auto ckc1_A = gckc(ck1_A), ckc1_B = gckc(ck1_B);
  auto [h1_A, hn1_A, hs1_A] = qryLookup(ck1_A, ckc1_A, state[0], state[p], state[C]);
  auto [h1_B, hn1_B, hs1_B] = qryLookup(ck1_B, ckc1_B, state[0], state[p], state[C]);
  if (!(hs1_A & hs1_B)) return;
  for (int i = 0; i < hn1_A; i++)
      for (int j = 0; j < hn1_B; j++)
        if ((h1_A[i] & rem_B) == (h1_B[j] & rem_A)) {
          state.push_back(h1_A[i] | h1_B[j]);
          if(_lookahead(state.data() + (state.size() - 2 * p), depth + 1, enf_A, enf_B, rem_A, rem_B))
            fn(state[2 * p]);
          state.pop_back();
        }
}
}; // namespace _logic
using namespace _logic;
