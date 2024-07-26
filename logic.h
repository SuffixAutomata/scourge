#pragma once

#include <cstdint>
#include <iostream>
#include <vector>
#include <ranges>
#include <cassert>
#include <type_traits>
#include <vector>
#include <cmath>
#include <vector>

#define sz(x) ((int)((x).size()))

namespace _logic {
std::vector<int> table = []() -> std::vector<int> {
  std::vector<int> table(1024);
  for (int x = 0; x < 512; x++) {
    int r = abs(__builtin_popcount(x & 495) * 2 + (x >> 4) % 2 - 6) <= 1;
    table[x + r * 512] = 1;
  }
  return table;
}();
std::vector<std::pair<int, int>> primeImplicants = []() {
  std::vector<std::pair<int, int>> ans;
  for (int care = 1; care < 1024; care++) {
    for (int force = care;; force = (force - 1) & care) {
      for (int x = care; x < 1024; x = (x + 1) | care)
        if (table[x ^ force])
          goto no;
      for (auto [a, b] : ans)
        if ((a & care) == a && (a & force) == b)
          goto no;
      ans.push_back({care, force});
    no:;
      if (force == 0)
        break;
    }
  }
  for (int i = 0; i < 1024; i++) {
    bool s = 1;
    for (auto [a, b] : ans)
      s = s && (a & (~(i ^ b)));
    assert(s == table[i]);
  }
  return ans;
}();

void trans(std::vector<int> vars, std::vector<int> &inst) {
  for (auto [a, b] : primeImplicants) {
    for (int x = 0; x < 10; x++)
      if (a & (1 << x))
        inst.push_back((b & (1 << x)) ? vars[x] : -vars[x]);
    inst.push_back(0);
  }
}

bool Compl3t34bl3(const std::vector<uint64_t> &state, int depth, int phase) {
  assert(p * 2 == sz(state));
  // auto idx = [&](int j) { return ((sym==1) ? min(j, width - j - 1) : j); };
  auto get = [&](int r, int j, int t) {
    if (t == p)
      t = 0, j = (sym == 2) ? width - j - 1 : j;
    r = r * p + t - phase;
    int realRow = (r + depth - 2 * p) / p;
    if (j <= -1 && (realRow < (int)leftborder[-j-1].size())) {
      // WARN << j << ' ' << r << ' ' << t << '\n';
      return (leftborder[-j-1][realRow] & (1ull << t)) ? 1 : 0;
    }
    if (j <= -1 || j >= width)
      return 0;
    if (r < p*2)
      return +!!(state[r] & (1ull << j));
    return 0;
  };
  for (int row = p*2; row < p*2 + 2 * p; row++) {
    int r = (row + phase) / p, t = (row + phase) % p;
    for (int j = -1; j <= width; j++) {
      // WARN << row << ' ' << j << '\n';
      std::vector<int> ar = {get(r - 2, j - 1, t), get(r - 2, j, t),
                              get(r - 2, j + 1, t), get(r - 1, j - 1, t),
                              get(r - 1, j, t),     get(r - 1, j + 1, t),
                              get(r, j - 1, t),     get(r, j, t),
                              get(r, j + 1, t),     get(r - 1, j, t + 1)};
      int s = 0;
      for (int i = 0; i < 10; i++)
        if (ar[i])
          s |= (1 << i);
      if (!table[s])
        return 0;
    }
  }
  return 1;
}

}; // namespace _logic
using namespace _logic;