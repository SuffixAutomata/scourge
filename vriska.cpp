#include <bits/stdc++.h>
#include "cadical/src/cadical.hpp"
using namespace std;

#define sz(x) ((int)(x.size()))

int p, width, sym, l4h;

namespace _logic {
vector<int> table = []() -> vector<int> { 
  vector<int> table(1024);
  for(int x=0;x<512;x++){
    int r = abs(__builtin_popcount(x&495) * 2 + (x>>4)%2 - 6) <= 1;
    table[x+r*512]=1;
  }
  return table;
}();
vector<pair<int,int>> primeImplicants = []() -> vector<pair<int,int>> {
  vector<pair<int,int>> ans;
  for(int care=1;care<1024;care++){
    for(int force=care;;force=(force-1)&care){
      for(int x=care;x<1024;x=(x+1)|care)if(table[x^force])goto no;
      for(auto [a,b]:ans) if((a&care)==a && (a&force)==b) goto no;
      ans.push_back({care, force});
      no:;
      if(force == 0) break;
    }
  }
  for(int i=0;i<1024;i++){
    bool s = 1;
    for(auto [a,b]:ans)
      s = s && (a&(~(i^b)));
    assert(s == table[i]);
  }
  return ans;
}();

void trans(vector<int> vars, vector<int>& inst) {
  for(auto [a,b]:primeImplicants){
    for(int x=0;x<10;x++)if(a&(1<<x)){
      inst.push_back((b&(1<<x))?vars[x]:-vars[x]);
    }
    inst.push_back(0);
  }
}

void solve(vector<int>& inst, vector<int> crit, auto fn) {
  CaDiCaL::Solver* solver = new CaDiCaL::Solver;
  solver->set("quiet", 1);
  for(int i:inst) solver->add(i);
  int res;
  while((res = solver->solve()) != 20) {
    assert(res == 10);
    vector<int> resp(crit.size());
    for(int i=0;i<sz(crit);i++)
      resp[i] = solver->val(crit[i]);
    for(int i=0;i<sz(crit);i++)
      solver->add(-resp[i]);
    fn(resp), solver->add(0);
  }
  delete solver;
}

void genNextRows(vector<uint64_t>& state, int phase, int ahead, auto fn) {
  assert(p*2 == sz(state));
  vector<int> inst = {1, 0, -2, 0};
  auto idx = [&](int j) { return (sym ? min(j, width - j - 1) : j); };
  auto get = [&](int r, int j, int t) {
    r = r*p+t-phase;
    if(j <= -1 || j >= width) return (2-0);
    if(r < sz(state)) return (2-!!(state[r]&(1ull<<j)));
    return 3 + idx(j) + (int)(r-state.size())*width;
  };
  for(int row=state.size(); row<sz(state)+ahead; row++){
    int r=(row+phase)/p, t=(row+phase)%p;
    for(int j=-1; j<=width; j++)
      trans({get(r-2,j-1,t),get(r-2,j,t),get(r-2,j+1,t),
             get(r-1,j-1,t),get(r-1,j,t),get(r-1,j+1,t),
             get(r,j-1,t),get(r,j,t),get(r,j+1,t),get(r-1,j,(t+1)%p)}, inst);
  }
  vector<int> crit(max(idx(width/2),idx(width-1))+1); iota(crit.begin(), crit.end(), 3);
  vector<uint64_t> bb(state.begin()+1, state.end());
  solve(inst, crit, [&](vector<int> sol){
    uint64_t x = 0;
    for(int j=0; j<width; j++) if(sol[idx(j)]>0) x|=(1ull<<j);
    fn(x);
  });
}

}; using namespace _logic;

int main(int argc, char* argv[]) {
  auto t1 = chrono::high_resolution_clock::now();
  assert(argc == 5);
  ifstream fin(argv[1]);
  int l = stoi(argv[3]), r = stoi(argv[4]);
  fin >> p >> width >> sym >> l4h;
  int i, ph;
  ostringstream fout;
  while(fin >> i >> ph) {
    vector<uint64_t> h(2*p);
    for(int x=0;x<2*p;x++) fin >> h[x];
    vector<uint64_t> nxt;
    if(l <= i && i <= r) {
      genNextRows(h, ph, l4h, [&](uint64_t t){nxt.push_back(t);});
      fout<<i;
      for(uint64_t s:nxt)fout<<' '<<s;
      fout<<" -\n";
    }
  }
  auto t2 = chrono::high_resolution_clock::now();
  fout << (t2-t1);
  ofstream fx(argv[2]);
  fx<<fout.str()<<endl; fx.flush(); fx.close();
  return 0;
}