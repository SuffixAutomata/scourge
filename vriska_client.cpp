#include <thread>

#include "mongoose.h"
#include "cadical/src/cadical.hpp"
#include "cqueue/bcq.h"

#include "logic.h"

namespace _logic {
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

void genNextRows(std::vector<uint64_t> &state, int depth, int ahead, auto fn) {
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
      trans({get(r - 2, j - 1, t), get(r - 2, j, t), get(r - 2, j + 1, t),
              get(r - 1, j - 1, t), get(r - 1, j, t), get(r - 1, j + 1, t),
              get(r, j - 1, t), get(r, j, t), get(r, j + 1, t),
              get(r - 1, j, t + 1)},
            inst);
    if (t != p - 1)
      for (int j = 0; j < width; j++)
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
      _crit.insert(get(0, j, -1));
    crit = std::vector<int>(_crit.begin(), _crit.end());
  }
  std::vector<uint64_t> bb(state.begin() + 1, state.end());
  solve(inst, crit, [&](std::vector<int> sol) {
    uint64_t x = 0;
    for (int j = 0; j < width; j++)
      for (int sdx = 0; sdx < sz(crit); sdx++)
        if (crit[sdx] == get(0, j, -1))
          if (sol[sdx] > 0)
            x |= (1ull << j);
    fn(x);
  });
}
}

int main(int argc, char* argv[]) {
  // args: -h host -i worker id/hostname -t timeout -p threads
}