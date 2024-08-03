#pragma once

#include <cassert>
#include <cmath>
#include <vector>

#include "globals.h"

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

bool Compl3t34bl3(const std::vector<uint64_t> &state, int depth, int phase,
                  uint64_t enforce) {
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
    if (r < sz(state))
      return +!!(state[r] & (1ull << j));
    return 0;
  };
  for (int row = sz(state); row < sz(state) + 2 * p; row++) {
    int r = (row + phase) / p, t = (row + phase) % p;
    for (int j = -1; j <= width; j++)
      if (enforce & (1ull << (j + 1))) {
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

void solve(std::vector<int> &inst, std::vector<int> crit, auto fn, bool term1=false) {
  CaDiCaL::Solver *solver = new CaDiCaL::Solver;
  solver->set("quiet", 1);
  for (int i : inst)
    solver->add(i);
  int res;
  while ((res = solver->solve()) != 20) {
    assert(res == 10);
    std::vector<int> resp(sz(crit));
    for (int i = 0; i < sz(crit); i++)
      resp[i] = solver->val(crit[i]);
    for (int i = 0; i < sz(crit); i++)
      solver->add(-resp[i]);
    fn(resp), solver->add(0);
    if(term1) break;
  }
  delete solver;
}

void genNextRows(std::vector<uint64_t> &state, int depth, int ahead,
                 uint64_t enforce, uint64_t remember, auto fn) {
  assert(p * 2 == sz(state));
  int phase = depth % p;
  std::vector<int> inst = {1, 0, -2, 0};
  auto idx = [&](int j) { return ((sym == 1) ? min(j, width - j - 1) : j); };
  auto get = [&](int r, int j, int t) {
    if (t == p)
      t = 0, j = (sym == 2) ? width - j - 1 : j;
    r = r * p + t - phase;
    int realRow = (r + depth - 2 * p) / p;
    if (j <= -1 && (realRow < (int)leftborder[-j-1].size())) {
      // WARN << j << ' ' << r << ' ' << t << '\n';
      return (leftborder[-j-1][realRow] & (1ull << t)) ? 1 : 2;
    }
    if (t == -1)
      r = sz(state);
    if (j <= -1 || j >= width)
      return (2 - 0);
    if (r < sz(state))
      return (2 - !!(state[r] & (1ull << j)));
    return 3 + idx(j) + (r - sz(state)) * width;
  };
  auto eqify = [&](int i, int j) {
    for (int x : std::vector<int>{i, -j, 0, -i, j, 0})
      inst.push_back(x);
  };
  for (int row = sz(state); row < sz(state) + ahead; row++) {
    int r = (row + phase) / p, t = (row + phase) % p;
    for (int j = -1; j <= width; j++)
      if (enforce & (1ull << (j + 1)))
        trans({get(r - 2, j - 1, t), get(r - 2, j, t), get(r - 2, j + 1, t),
               get(r - 1, j - 1, t), get(r - 1, j, t), get(r - 1, j + 1, t),
               get(r, j - 1, t), get(r, j, t), get(r, j + 1, t),
               get(r - 1, j, t + 1)},
              inst);
    if (t != p - 1)
      for (int j = 0; j < width; j++)
        if (enforce & (1ull << (j + 1)))
          if (width - j <= stator)
            eqify(get(r, j, t), get(r, j, t + 1));
  }
  for (int row = 0; row < sz(state) + ahead; row++) {
    int r = (row + phase) / p, t = (row + phase) % p;
    if (((row + depth - 2 * p) / p) < sz(filters))
      for (int j = 0; j < width; j++)
        if (!(filters[(row + depth - 2 * p) / p] & (1ull << j)))
          eqify(get(r, j, t), 2 - 0);
  }
  std::vector<int> crit;
  {
    std::set<int> _crit;
    for (int j = 0; j < width; j++)
      if (remember & (1 << j))
        _crit.insert(get(0, j, -1));
    crit = std::vector<int>(_crit.begin(), _crit.end());
  }
  std::vector<uint64_t> bb(state.begin() + 1, state.end());
  solve(inst, crit, [&](std::vector<int> sol) {
    uint64_t x = 0;
    for (int j = 0; j < width; j++)
      if (remember & (1 << j)) {
        for (int sdx = 0; sdx < sz(crit); sdx++)
          if (crit[sdx] == get(0, j, -1))
            if (sol[sdx] > 0)
              x |= (1ull << j);
      }
    fn(x);
  });
}

bool existsNextRows(std::vector<uint64_t> &state, int depth, int ahead,
                    uint64_t enforce, uint64_t remember) {
  assert(p * 2 == sz(state));
  int phase = depth % p;
  std::vector<int> inst = {1, 0, -2, 0};
  auto idx = [&](int j) { return ((sym == 1) ? min(j, width - j - 1) : j); };
  auto get = [&](int r, int j, int t) {
    if (t == p)
      t = 0, j = (sym == 2) ? width - j - 1 : j;
    r = r * p + t - phase;
    int realRow = (r + depth - 2 * p) / p;
    if (j <= -1 && (realRow < (int)leftborder[-j-1].size())) {
      // WARN << j << ' ' << r << ' ' << t << '\n';
      return (leftborder[-j-1][realRow] & (1ull << t)) ? 1 : 2;
    }
    if (t == -1)
      r = sz(state);
    if (j <= -1 || j >= width)
      return (2 - 0);
    if (r < sz(state))
      return (2 - !!(state[r] & (1ull << j)));
    return 3 + idx(j) + (r - sz(state)) * width;
  };
  auto eqify = [&](int i, int j) {
    for (int x : std::vector<int>{i, -j, 0, -i, j, 0})
      inst.push_back(x);
  };
  for (int row = sz(state); row < sz(state) + ahead; row++) {
    int r = (row + phase) / p, t = (row + phase) % p;
    for (int j = -1; j <= width; j++)
      if (enforce & (1ull << (j + 1)))
        trans({get(r - 2, j - 1, t), get(r - 2, j, t), get(r - 2, j + 1, t),
               get(r - 1, j - 1, t), get(r - 1, j, t), get(r - 1, j + 1, t),
               get(r, j - 1, t), get(r, j, t), get(r, j + 1, t),
               get(r - 1, j, t + 1)},
              inst);
    if (t != p - 1)
      for (int j = 0; j < width; j++)
        if (enforce & (1ull << (j + 1)))
          if (j < stator || width - j <= stator)
            eqify(get(r, j, t), get(r, j, t + 1));
  }
  for (int row = 0; row < sz(state) + ahead; row++) {
    int r = (row + phase) / p, t = (row + phase) % p;
    if (((row + depth - 2 * p) / p) < sz(filters))
      for (int j = 0; j < width; j++)
        if (!(filters[(row + depth - 2 * p) / p] & (1ull << j)))
          eqify(get(r, j, t), 2 - 0);
  }
  std::vector<int> crit;
  {
    std::set<int> _crit;
    for (int j = 0; j < width; j++)
      if (remember & (1 << j))
        _crit.insert(get(0, j, -1));
    crit = std::vector<int>(_crit.begin(), _crit.end());
  }
  std::vector<uint64_t> bb(state.begin() + 1, state.end());
  bool hasSol = 0;
  solve(inst, crit, [&](std::vector<int> sol) {
    hasSol = 1;
  }, true);
  return hasSol;
}

}; // namespace _logic
using namespace _logic;

void calculateMasks(int bthh) {
  enforce = remember = enforce2 = remember2 = 0;
  for (int j = -1; j <= width; j++) {
    int s = ((sym == 1) ? min(j, width - j - 1) : j);
    if (0 <= s && s < bthh + 1)
      remember2 |= (1ull << j);
    if (bthh - 1 <= s && s < width)
      remember |= (1ull << j);
    if (s < bthh)
      enforce2 |= (1ull << (j + 1));
    else
      enforce |= (1ull << (j + 1));
  }
  overlap = remember & remember2;
  overlap_ctz = __builtin_ctzll(overlap);
}