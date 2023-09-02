#include <bits/stdc++.h>
using namespace std;

#ifdef __unix__
#include <sys/resource.h>
#include <sys/time.h>
int __SETMEMLIMIT = []() {
  struct rlimit memlimit = {1 << 30, 1 << 30};
  return setrlimit(RLIMIT_AS, &memlimit);
}();

#endif

vector<int> table = []() {
  vector<int> table(1024);
  for (int x = 0; x < 512; x++) {
    int r = abs(__builtin_popcount(x & 495) * 2 + (x >> 4) % 2 - 6) <= 1;
    table[x + r * 512] = 1;
  }
  return table;
}();

struct bddNode {
  int x, v[2];
  bool operator<(const bddNode &o) const {
    return make_pair(x, make_pair(v[0], v[1])) <
           make_pair(o.x, make_pair(o.v[0], o.v[1]));
  }
} A[10000006];
int nodeCount = []() {
  A[0].x = A[1].x = -1;
  return 2;
}();
map<bddNode, int> bddKnown, iteKnown, resKnown, exqKnown;

int newNode(int var, int v0, int v1) {
  if (v0 == v1)
    return v0;
  auto &r = bddKnown[{var, v0, v1}];
  if (r)
    return r;
  A[r = nodeCount] = {var, v0, v1};
  return nodeCount++;
}

int bddITE(int f, int g, int h) {
  if (f < 2 || g == h)
    return (f ? g : h);
  if (g == 1 && h == 0)
    return f;
  auto [it, ex] = iteKnown.try_emplace({f, g, h}, -1);
  if (!ex)
    return it->second;
  int sp = max(A[f].x, max(A[g].x, A[h].x));
  auto q = [&](int i, int v) { return (A[i].x == sp) ? A[i].v[v] : i; };
  return it->second = newNode(sp, bddITE(q(f, 0), q(g, 0), q(h, 0)),
                              bddITE(q(f, 1), q(g, 1), q(h, 1)));
}

int bddRESTRICT(int f, int x, int v) {
  if (A[f].x < x)
    return f;
  if (A[f].x == x)
    return A[f].v[v];
  auto [it, ex] = resKnown.try_emplace({f, x, v}, -1);
  if (!ex)
    return it->second;
  return it->second = newNode(A[f].x, bddRESTRICT(A[f].v[0], x, v),
                              bddRESTRICT(A[f].v[1], x, v));
}

int bddAND(int f, int g) { return bddITE(f, g, 0); }
int bddOR(int f, int g) { return bddITE(f, 1, g); }

int bddEXQUANT(int f, int x) {
  if (A[f].x == x)
    return bddOR(A[f].v[0], A[f].v[1]);
  if (A[f].x < x)
    return f;
  auto [it, ex] = exqKnown.try_emplace({f, x, 0}, -1);
  if (!ex)
    return it->second;
  return it->second = newNode(A[f].x, bddEXQUANT(A[f].v[0], x),
                              bddEXQUANT(A[f].v[1], x));
}

int bddBUILD(vector<int> vars, vector<int> vals) {
  if (vars.size() == 1)
    return newNode(vars[0], vals[0], vals[1]);
  int maxpos = 0;
  for (int i = 0; i < vars.size(); i++)
    if (vars[i] > vars[maxpos])
      maxpos = i;
  vector<vector<int>> v(2);
  vector<int> q;
  for (int i = 0; i < vars.size(); i++)
    if (i != maxpos)
      q.push_back(vars[i]);
  for (int i = 0; i < vals.size(); i++)
    v[!!(i & (1 << maxpos))].push_back(vals[i]);
  return bddITE(newNode(vars[maxpos], 0, 1), bddBUILD(q, v[1]),
                bddBUILD(q, v[0]));
}

int bddSIZE(int f, set<int> &x, bool h = 0) {
  if (x.count(f) || f < 2)
    return x.size();
  if (h)
    cout << f << ' ' << A[f].x << ' ' << A[f].v[0] << ' ' << A[f].v[1] << endl;
  x.insert(f);
  bddSIZE(A[f].v[0], x, h), bddSIZE(A[f].v[1], x, h);
  return x.size();
}
int bddSIZE(int f, bool h = 0) {
  set<int> x;
  return bddSIZE(f, x, h);
}

vector<string> part = {
    "....oo.oo.ooo.........", "...ooo.oo.o.oo........",
    "...o.o.oo...oo........", ".....o......ooo.......",
    "....oo........o.......", "....o.ooo..o..........",
    "........o.oo.o........", "..o.o.........o.......",
    "....oo.oo.o...o.......", ".....o.oo.o....o......",
    ".......ooo.o..........", "........o..o..........",
    "...o..o....oo.o.......", "..ooo.o.oo............",
};

int main() {
  int width = 5, phase = 0;
  int p, rl;
  cin >> p >> rl >> width;
  int k = 1;
  auto get = [&](int r, int j) {
    if ((r < 2 * p) || !(0 <= j && j < width))
      return 0;
    return r * 100000 + ((j + 600) % 3);
  };
  int bdd = 1;
  for (int j = -1; j <= width; j++) {
    for (int row = 2 * p; row < 2 * p + rl; row++) {
      int x = bddBUILD({get(row - 2 * p, j - 1), get(row - 2 * p, j),
                        get(row - 2 * p, j + 1), get(row - p, j - 1),
                        get(row - p, j), get(row - p, j + 1), get(row, j - 1),
                        get(row, j), get(row, j + 1), get(row - p + k, j)},
                       table);
      bdd = bddAND(bdd, x);
      bdd = bddEXQUANT(bdd, get(row - 2 * p, j - 1));
      if (row + p >= 2 * p + rl) {
        bdd = bddEXQUANT(bdd, get(row - p, j - 1));
        bdd = bddEXQUANT(bdd, get(row, j - 1));
      }
      cout << bddSIZE(bdd) << endl;
    }
    cout << bddSIZE(bdd) << ' ' << bdd << endl;
  }
}
