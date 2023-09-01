#include <bits/stdc++.h>
using namespace std;

vector<int> table = [](){
  vector<int> table(1024);
  for(int x=0;x<512;x++){
    int r = abs(__builtin_popcount(x&495) * 2 + (x>>4)%2 - 6) <= 1;
    table[x+r*512]=1;
  }
  return table;
}();

struct bddNode { int x, v[2]; 
  bool operator<(const bddNode& o) const { 
    return make_pair(x, make_pair(v[0],v[1])) < make_pair(o.x, make_pair(o.v[0], o.v[1]));
  }
} A[10000006];
int nodeCount = [](){ A[0].x = A[1].x = -1; return 2; }();
map<bddNode, int> bddKnown, iteKnown, resKnown, exqKnown;

void bddRESET(bool x = 1) { 
  if(x) 
  nodeCount = 2; 
  bddKnown.clear(); iteKnown.clear(); resKnown.clear(); exqKnown.clear(); 
}

int newNode(int var, int v0, int v1) {
  if(v0 == v1) return v0;
  auto& r = bddKnown[{var, v0, v1}];
  if(r) return r;
  A[r = nodeCount] = {var, v0, v1};
  return nodeCount++;
}

int bddITE(int f, int g, int h) {
  if(f < 2 || g == h) return (f ? g : h);
  if(g == 1 && h == 0) return f;
  auto [it, ex] = iteKnown.try_emplace({f, g, h}, -1);
  if(!ex) return it->second;
  int sp = max(A[f].x, max(A[g].x, A[h].x)); 
  auto q = [&](int i, int v){ return (A[i].x == sp) ? A[i].v[v] : i; };
  return it->second = newNode(sp, bddITE(q(f,0), q(g,0), q(h,0)), bddITE(q(f,1), q(g,1), q(h,1)));
}

int bddRESTRICT(int f, int x, int v) {
  if(A[f].x < x) return f;
  auto [it, ex] = resKnown.try_emplace({f, x, v}, -1);
  if(!ex) return it->second;
  if(A[f].x == x) return it->second = bddRESTRICT(A[f].v[v], x, v);
  return it->second = newNode(A[f].x, bddRESTRICT(A[f].v[0], x, v), bddRESTRICT(A[f].v[1], x, v)); 
}

int bddAND(int f, int g) { return bddITE(f, g, 0); }
int bddOR(int f, int g) { return bddITE(f, 1, g); }

int bddEXQUANT(int f, int x) {
  return bddOR(bddRESTRICT(f, x, 0), bddRESTRICT(f, x, 1));

  if(A[f].x == x) return bddOR(A[f].v[0], A[f].v[1]);
  if(A[f].x < x) return f;
  auto [it, ex] = exqKnown.try_emplace({f, x, 0}, -1);
  if(!ex) return it->second;
  return it->second = newNode(A[f].x, bddEXQUANT(A[f].v[0], x), bddEXQUANT(A[f].v[1], x));
}


int bddBUILD(vector<int> vars, vector<int> vals) {
  if(vars.size() == 1) return newNode(vars[0], vals[0], vals[1]);
  vector<vector<int>> v(2); vector<int> q(vars.begin()+1, vars.end());
  for(int i=0; i<vals.size(); i++) v[i&1].push_back(vals[i]);
  return bddITE(newNode(vars[0], 0, 1), bddBUILD(q, v[1]), bddBUILD(q,v[0])); 
  // return newNode(vars[0], bddBUILD(q, v[0]), bddBUILD(q,v[1])); 
}

int bddSIZE(int f, set<int>& x, bool h = 0) {
  if(x.count(f) || f < 2) return x.size();
  if(h) cout << f << ' ' << A[f].x << ' ' << A[f].v[0] << ' ' << A[f].v[1] << endl;
  x.insert(f); bddSIZE(A[f].v[0], x, h), bddSIZE(A[f].v[1], x, h);
  return x.size();
}
int bddSIZE(int f, bool h = 0) { set<int> x; return bddSIZE(f, x, h); }

map<bddNode, int> attemptPredict(int p, int r, int width, map<bddNode, int> ans) { 
  map<bddNode, int> oknown = ans;
  int phase = 0;
  for(int row=0; row<2*p+r; row++) {
    int r=(row+phase)/p, t=(row+phase)%p;
    for(int j = 0; j < 3; j++)
      ans.try_emplace({r, j, t}, j + r * 1000 + t * 1000000);
  }
  auto get = [&](int _r, int j, int t) {
    return ans[{_r,j,t}];
  };
  // auto ev = [&]() {
    bddRESET();
    int cbdd = 1, j = 1;
    for(int row=2*p; row<2*p+r; row++){
      int r=(row+phase)/p, t=(row+phase)%p;
      // for(int j=0; j<width; j++)
        // trans(
      int x =  bddBUILD(
          {get(r-2,j-1,t),get(r-2,j,t),get(r-2,j+1,t),
              get(r-1,j-1,t),get(r-1,j,t),get(r-1,j+1,t),
              get(r,j-1,t),get(r,j,t),get(r,j+1,t),get(r-1,j,(t+1)%p)}, table);
      cbdd = bddAND(cbdd, x);
      for(int row = 0; row<2*p; row++) {
        int r=(row+phase)/p, t=(row+phase)%p;
        cbdd = bddRESTRICT(cbdd, get(r, j-1, t), 0);
        cbdd = bddRESTRICT(cbdd, get(r, j, t), 0);
        cbdd = bddRESTRICT(cbdd, get(r, j+1, t), 0);
      }
    }
    // return bddSIZE(cbdd);
  int best = bddSIZE(cbdd);
  // };
  // int best = ev();
  for(int _=0;_<2;_++) for(auto& [a,b]: ans) for(auto& [c,d] : ans) {
    if((a.x*p + a.v[0]) < 2 * p) continue;
    if((c.x*p + c.v[0]) < 2 * p) continue;
    // if(oknown.count(a) && oknown.count(c)) continue;
    int x0 = bddRESTRICT(cbdd, b, 0), x1 = bddRESTRICT(cbdd, b, 1);
    int x00 = bddRESTRICT(x0, d, 0), x01 = bddRESTRICT(x0, d, 1);
    int x10 = bddRESTRICT(x1, d, 0), x11 = bddRESTRICT(x1, d, 1);
    int cbddAlt = bddITE(newNode(b, 0, 1), bddITE(newNode(d, 0, 1), x00, x10), bddITE(newNode(d, 0, 1), x01, x11));
    int re = bddSIZE(cbddAlt);
    swap(b, d);
    // int re = ev();
    if(re < best)
      cout << re << endl, best = re, cbdd=cbddAlt;
    else swap(b,d);
  }
  cout<< r<<" done: "<<best<<endl;
  return ans;
}

vector<string> part = {
"....oo.oo.ooo.........",
"...ooo.oo.o.oo........",
"...o.o.oo...oo........",
".....o......ooo.......",
"....oo........o.......",
"....o.ooo..o..........",
"........o.oo.o........",
"..o.o.........o.......",
"....oo.oo.o...o.......",
".....o.oo.o....o......",
".......ooo.o..........",
"........o..o..........",
"...o..o....oo.o.......",
"..ooo.o.oo............",};

int main() {
  int width = 5, phase = 0;;
  int p, rl;
  cin >> p >> rl >> width;
  int k = 1;

  auto get = [&](int r, int j) {
    if(r < 2*p) return 0;
    if(!(0 <= j && j < width)) return 0;
    return ((j+600)%3) * 100000 + r;
    // return _r + j * 1000 + t * 1000000;
    // return _r + j * 1000000 + t * 1000;
    // return _r * 1000 + j + t * 1000000;
    // return _r * 1000 + j * 1000000 + t;
    // return _r * 1000000 + j + t * 1000;
    // return _r * 1000000 + j * 1000 + t;
  };

  // map<bddNode, int> heuristicMap;
  // for(int s=8; s<=r;s++)
  // heuristicMap = attemptPredict(p, s, width, heuristicMap);
  // for(auto& [a,b]:heuristicMap) cout<<a.x<<' '<<a.v[0]<<' '<<a.v[1]<<' '<<b<<endl;
  // return 0;

  int bdd = 1;
  for(int j = -1; j <= width; j++ ){
    for(int row=2*p; row<2*p+rl; row++){
      // for(int j=0; j<width; j++)
        // trans(
      int x =  bddBUILD(
          {get(row-2*p,j-1),get(row-2*p,j),get(row-2*p,j+1),
              get(row-p,j-1),get(row-p,j),get(row-p,j+1),
              get(row,j-1),get(row,j),get(row,j+1),get(row-p+k,j)}, table);
      // for(int row = 0; row<2*p; row++) {
      //   int r=(row+phase)/p, t=(row+phase)%p;
      //   for(int v=j-1;v<=j+1;v++)
      //     x = bddRESTRICT(x, get(r, v, t),  0);
      //     // bdd = bddRESTRICT(bdd, get(r, v, t), (0 <= v && v < width) ? (part[row][v] == 'o') : 0);
      // }
      bdd = bddAND(bdd, x);
      bdd = bddEXQUANT(bdd, get(row-2*p, j-1));
      if(row + p >= 2*p+rl) {
        bdd = bddEXQUANT(bdd, get(row-p, j-1));
        bdd = bddEXQUANT(bdd, get(row, j-1));
      }
      cout << bddSIZE(bdd)<<endl;
    }
    bdd = bddEXQUANT(bdd, get(2*p+rl-1, j));
    bdd = bddEXQUANT(bdd, get(2*p+rl-1, j+1));
    bdd = bddEXQUANT(bdd, get(2*p+rl-2, j));
    bdd = bddEXQUANT(bdd, get(2*p+rl-2, j+1));
    // bddRESET(true);
    // for(int row = 0; row<2*p+r; row++) {
    //   int r=(row+phase)/p, t=(row+phase)%p;
    //   bdd = bddEXQUANT(bdd, get(r, j-1, t));
    // }  
    cout<<bddSIZE(bdd)<<' ' <<bdd<<endl;
  }
}
