// TODO : cycle detection ; drifting rows ; symmetries ; smart stopping ; PRUNING.
#include <bits/stdc++.h>
#include "cadical/src/cadical.hpp"
#include "cqueue/bcq.h"
using namespace std;

#define sz(x) ((int)(x.size()))

#ifdef nohomestuck
class nohomestuckstream { public: ostream& x; nohomestuckstream(ostream& x) : x(x) {}
  template<typename T> const nohomestuckstream& operator<<(const T& v) const { x << v; return *this; }
  const nohomestuckstream& operator<<(const char* s) const {
    for(char c:string(s)) x << ((c=='4'?'A':(c=='1'?'I':(c=='3'?'E':c)))); x.flush(); return *this; }
} nohomestuckcout(cout);
#define cout nohomestuckcout
#define endl "\n"
#endif

int p, width, sym, l4h;
int maxwid, stator;

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

bool Compl3t34bl3(const vector<uint64_t>& state, int phase) {
auto idx = [&](int j) { return ((sym==1) ? min(j, width - j - 1) : j); };
  auto get = [&](int r, int j, int t) {
    if(t == p) t = 0, j = (sym == 2) ? width - j - 1 : j;
    r = r*p+t-phase;
    if(j <= -1 || j >= width) return 0;
    if(r < sz(state)) return +!!(state[r]&(1ull<<j));
    return 0;
  };
  for(int row=state.size(); row<sz(state)+2*p; row++){
    int r=(row+phase)/p, t=(row+phase)%p;
    for(int j=-1; j<=width; j++) {
      vector<int>ar = {get(r-2,j-1,t),get(r-2,j,t),get(r-2,j+1,t),
             get(r-1,j-1,t),get(r-1,j,t),get(r-1,j+1,t),
             get(r,j-1,t),get(r,j,t),get(r,j+1,t),get(r-1,j,t+1)};
      int s = 0;
      for(int i=0;i<10;i++)if(ar[i])s|=(1<<i);
      if(!table[s]) return 0;
    }
  }
  return 1;
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

void genNextRows(vector<uint64_t>& state, int phase, int ahead, uint64_t enforce, uint64_t remember, auto fn) {
  assert(p*2 == sz(state));
  vector<int> inst = {1, 0, -2, 0};
  auto idx = [&](int j) { return ((sym==1) ? min(j, width - j - 1) : j); };
  auto get = [&](int r, int j, int t) {
    if(t == p) t = 0, j = (sym == 2) ? width - j - 1 : j;
    r = r*p+t-phase;
    if(t == -1) r = state.size();
    if(j <= -1 || j >= width) return (2-0);
    if(r < sz(state)) return (2-!!(state[r]&(1ull<<j)));
    return 3 + idx(j) + (int)(r-state.size())*width;
  };
  auto eqify = [&](int i, int j) { for(int x:vector<int>{i,-j,0,-i,j,0}) inst.push_back(x); };
  for(int row=state.size(); row<sz(state)+ahead; row++){
    int r=(row+phase)/p, t=(row+phase)%p;
    for(int j=-1; j<=width; j++) if(enforce & (1ull<<(j+1)))
      trans({get(r-2,j-1,t),get(r-2,j,t),get(r-2,j+1,t),
             get(r-1,j-1,t),get(r-1,j,t),get(r-1,j+1,t),
             get(r,j-1,t),get(r,j,t),get(r,j+1,t),get(r-1,j,t+1)}, inst);
    if(t != p-1) for(int j=0; j<width; j++) if(enforce & (1ull<<(j+1)))
      if(j < stator || width - j <= stator)
        eqify(get(r, j, t), get(r, j, t+1));
  }
  vector<int> crit; {
      set<int> _crit;
      for(int j=0; j<width; j++) if(remember&(1<<j)) _crit.insert(get(0, j, -1));
       crit = vector<int>(_crit.begin(), _crit.end()); }
  vector<uint64_t> bb(state.begin()+1, state.end());
  solve(inst, crit, [&](vector<int> sol){
    uint64_t x = 0;
    for(int j=0; j<width; j++) if(remember&(1<<j)) {
      for(int sdx=0; sdx<crit.size(); sdx++) if(crit[sdx] == get(0, j, -1))
        if(sol[sdx]>0) x|=(1ull<<j);
    }
    fn(x);
  });
}

}; using namespace _logic;

namespace _searchtree {
/* u - unqueued; q - queued; d - decomissioned */
struct node { uint64_t row; int depth, shift, parent; char state; };
struct searchTree {
  node* a;
  int depths[1000];
  int depthcnt[1000];
  int treeSize = 0, treeAlloc = 16777216;
  searchTree() { a = new node[treeAlloc]; }
  ~searchTree() { delete a; }
  void dumpTree(string fn) { } // TODO
  void loadTree(string fn) { }
  void flushTree() { }
  int newNode(node s = {0,0,0,0,' '}) {
    a[treeSize] = s; depthcnt[s.depth]++;
    if(s.state == 'q') depths[s.depth]++;
    return treeSize++;
  }
  bool shouldSkip(int onx) {
    while(onx != -1) {
      if(a[onx].state == 'd') return 1;
      onx = a[onx].parent;
    }
    return 0;
  }
  vector<uint64_t> getState(int onx, bool full=0) {
    vector<uint64_t> mat;
    while(onx != -1)
      mat.push_back(a[onx].row), onx = a[onx].parent;
    reverse(mat.begin(), mat.end());
    return vector<uint64_t>(full?mat.begin():(mat.end() - 2*p), mat.end());
  }
  int getWidth(int i) {
    uint64_t bito = 0;
    for(uint64_t x:getState(i)){ bito |= x; }
    return 64-__builtin_clzll(bito)-__builtin_ctzll(bito);
  }
} tree, tree2;

}; using namespace _searchtree;

int bdep = 0;
void emit(vector<uint64_t> mat){
  bool d = Compl3t34bl3(mat, 0);
  if(!d) {
    if(sz(mat) <= bdep) return;
    bdep = mat.size();
  }else
    cout<<"[[OSC1LL4TOR COMPL3T3!!!]]"<<endl;
  std::string rle;
  int cnt = 0, cur = 0;
  auto f = [&](char x) {
    if(x != cur) { if(cnt >= 2) rle+=to_string(cnt); if(cnt >= 1) rle+=(char)cur; cnt=0,cur=x; } cnt++;};
  for(int r=0; r*p<sz(mat); r++){
    if(r) f('$');
    for(int x=r*p; x<sz(mat) && x<(r+1)*p; x++){
      for(int j=0;j<width;j++) f("bo"[!!(mat[x]&(1ull<<j))]);
      f('b');f('b');f('b');
    }}f('!');
  cout << "x = 0, y = 0, rule = B3/S23\n" + rle + '!' << endl;
}

uint64_t enforce =   0b0'000000'111111'111111'000000'0;
uint64_t remember =    0b000001'111111'111111'100000;
uint64_t enforce2=   0b1'111111'000000'000000'111111'1;
uint64_t remember2=    0b111111'100000'000001'111111;
uint64_t overlap = remember & remember2;

bool canMatch(int i, const vector<uint64_t>& pat, searchTree& ref, vector<int>& sol, vector<int>& lux) {
  if(ref.a[i].depth == pat.size()) {
    vector<uint64_t> marge = ref.getState(i, 1);
    for(int x=0; x<pat.size(); x++)
      marge[x] |= pat[x];
    emit(marge);
    return 1;
  }
  if(ref.a[i].state != 'u') 
    return ref.a[i].state == 'q';
  int f = sol[i];
  while(f != -1) {
    if((ref.a[f].row & overlap) == (pat[ref.a[i].depth] & overlap)
      && canMatch(f, pat, ref, sol, lux)) return 1;
    f = lux[f];
  }
  return 0;
}

void prune() {
  auto t1 = chrono::high_resolution_clock::now();
  // map<vector<int>, int> idx1;
  // for(int i=2*p;i<tree2.treeSize;i++)if(!tree2.shouldSkip(i)) {
  //   auto h = tree2.getState(i, 1);
  //   vector<int> s; for(int t=2*p; t<h.size(); t++) s.push_back((h[t]>>5)&3);
  //   idx1[s]++;
  // }
  // cout << "[1NFO] ";
  // for(auto& [a,b]:idx1){
  //   for(int i:a)cout<<i;
  //   cout<<" - ";
  //   cout<<b<<' ';
  // }
  // cout<<endl;
  int p2 = 0, x2 =0;
  {
    vector<int> sol1(tree.treeSize, -1), lux1(tree.treeSize, -1);
    for(int i=1; i<tree.treeSize; i++)
      lux1[i] = sol1[tree.a[i].parent], sol1[tree.a[i].parent] = i;
    for(int i=0; i < tree2.treeSize; i++) {
      if(tree2.shouldSkip(i)) continue;
      x2++;
      // cout<<"M4TCH1NG"<<endl;
      if(i>=2*p && !canMatch(0, tree2.getState(i, 1), tree, sol1, lux1)) tree2.a[i].state = 'd';
      // cout<<"DON3 M4TCH1NG"<<endl;
    }
    for(int i=0; i < tree2.treeSize; i++)
      if(!tree2.shouldSkip(i)) p2++;
  }
  int p1 = 0, x1 =0;
  {
    vector<int> sol2(tree2.treeSize, -1), lux2(tree2.treeSize, -1);
    for(int i=1; i<tree2.treeSize; i++)
      lux2[i] = sol2[tree2.a[i].parent], sol2[tree2.a[i].parent] = i;
    for(int i=0; i < tree.treeSize; i++) {
      if(tree.shouldSkip(i)) continue;
      x1++;
      // cout<<"M4TCH1NG"<<endl;
      if(i>=2*p && !canMatch(0, tree.getState(i, 1), tree2, sol2, lux2)) tree.a[i].state = 'd';
      // cout<<"DON3 M4TCH1NG"<<endl;
    }
    for(int i=0; i < tree.treeSize; i++)
      if(!tree.shouldSkip(i)) p1++;
  }
  auto t2 = chrono::high_resolution_clock::now();
  cout<<"[1NFO] PRUN1NG: "<<x1<<" -> "<<p1<<"/"<<tree.treeSize<<", "<<x2<<" -> "<<p2<<"/"<<tree2.treeSize
      <<" 1N "<<(t2-t1).count()/1000000.0<<"ms"<<endl;
}

moodycamel::BlockingConcurrentQueue<pair<int, int>> A2B;
moodycamel::BlockingConcurrentQueue<node> B2A;

void betaUniverse() {
  pair<int,int> nx;
  while(A2B.wait_dequeue(nx), nx.second != -1) {
    auto& ref = nx.first?tree2:tree;
    int dep = ref.a[nx.second].depth;
    if(!ref.shouldSkip(nx.second)){
      vector<uint64_t> h = ref.getState(nx.second);
      genNextRows(h, dep%p, l4h, 
        nx.first ? enforce2 : enforce,
        nx.first ? remember2 : remember, 
        [&](uint64_t x){ 
          B2A.enqueue({x, dep+1, nx.first, nx.second, '0'});
      });
    }
    B2A.enqueue({0, dep, nx.first, -nx.second, '0'});
  }
}

void search(int th, int nodelim, int qSize) {
  int solved = 0, onx;
  vector<thread> universes;
  for(int i=0;i<th;i++) universes.emplace_back(betaUniverse);
  node x;
  int reportidx = 0;
  priority_queue<pair<int,pair<int,int>>> PQ;
  int sdep = bdep;
  auto report = [&] {
    cout << "[1NFO] SOLV3D ";
    cout << solved << "; QU3U3D " << qSize + PQ.size() << "; TOT4L " << tree.treeSize<<'+'<<tree2.treeSize << " NOD3S";
    cout << endl;
    if(reportidx % 2 == 0){
      cout << "[1NFO] D3PTH R34CH3D "<< sdep<<" ; TR33 PROF1L3";
      for(int i=2*p;i<=sdep;i++){
        if(tree.depths[i] == 0 && tree2.depths[i] == 0) cout<<" 0";
        else cout<<' '<<tree.depths[i]<<'+'<<tree2.depths[i];
        if(tree.depths[i] == tree.depthcnt[i] && tree2.depths[i] == tree2.depthcnt[i]);
        else cout<<'/'<<tree.depthcnt[i]<<'+'<<tree2.depthcnt[i];
      }
      cout<<endl;
      // cout<<"ST4RT1NG PRUN1NG"<<endl;
      prune();
      // cout<<"3ND3D PRUN1NG"<<endl;
    }
    reportidx++;
  };
  while(B2A.wait_dequeue(x), 1) {
    auto& ref = x.shift ? tree2 : tree;
    if(x.parent < 0) {
      ref.depths[x.depth]--, solved++, --qSize;
      if(ref.a[-x.parent].state == 'q')
        ref.a[-x.parent].state = 'u';
      if(solved % 16 == 0) report();
    } else {
      // if(x.row == tree[x.parent].row && x.depth == 6) continue;
      x.state = 'q', onx = ref.newNode(x);
      // PR1OR1TY FOR TH3 1NN3R TR33
      PQ.push({(-x.depth + (x.shift == 0 ? 3 : 0)) * 2 + rand() % 2, {x.shift, onx}});
      sdep = max(sdep, x.depth);
      // if(tree.treeSize%256==0) report();
      // emit(ref.getState(onx, 1));
    }
    while(PQ.size() && qSize <= 2*th && nodelim) {
      pair<int,int> best = PQ.top().second; PQ.pop();
      A2B.enqueue(best), qSize++, nodelim--;
    }
    if(x.parent < 0 && !qSize) break;
  }
  for(int i=0;i<th;i++) A2B.enqueue({0, -1});
  for(thread& x:universes) x.join();
  reportidx=0, report();
}

int main(int argc, char* argv[]) {
  cout<<primeImplicants.size()<<" PR1M3 1MPL1C4NTS"<<endl;
  if(argc > 1 && string(argv[1]) == "search")  {
    int th, nodelim, qSize = 0;
    cout<<"THR34DS, NOD3 L1M1T, LOOK4H34D: "<<endl; cin>>th>>nodelim>>l4h;
    cout<<"P3R1OD, W1DTH, SYMM3TRY, ST4TOR W1DTH: "<<endl; cin>>p>>width>>sym>>stator;
    if(sym != 0 && sym != 1)
      cout << "OTH3R SYMM3TR1S T3MPOR4R1LY NOT T3ST3D >:[" << endl;
    int bthh = (sym ? width/4 : width/2);
    cout<<"B1S3CT1ON THR3SHOLD: [SUGG3ST3D " << bthh << "] "<<endl; cin >> bthh;
    for(int j=-1; j<=width; j++) {
      int s = ((sym==1) ? min(j, width - j - 1) : j);
      if(0 <= s && s < bthh + 1) remember2 |= (1ull << j);
      if(bthh - 1 <= s && s < width) remember |= (1ull << j);
      if(s < bthh) enforce2 |= (1ull << (j+1));
      else enforce |= (1ull << (j+1));
    }
    // cout<<enforce<<' '<<enforce2<<' '<<remember<<' '<<remember2<<endl;
    cout<<"F1RST "<<2*p<<" ROWS:"<<endl;

    // enforce =   0b0'000000'111111'111111'000000'0;
    // remember =    0b000001'111111'111111'100000;
    // enforce2=   0b1'111111'000000'000000'111111'1;
    // remember2=    0b111111'100000'000001'111111;
    // cout<<enforce<<' '<<enforce2<<' '<<remember<<' '<<remember2<<endl;

    overlap = remember & remember2;

    int nx = -1, onx = -1;
    for(int i=0;i<2*p;i++){
      uint64_t x=0;
      string s; cin>>s;
      for(int j=0;j<width;j++)if(s[j]=='o')x|=(1ull<<j);
      nx = tree.newNode({x, i+1, 0, onx, 'u'});
      nx = tree2.newNode({x, i+1, 0, onx, 'u'});
      onx = nx;
    }
    tree.a[onx].state = 'q';
    tree2.a[onx].state = 'q';
    for(int i=0; i<tree.treeSize; i++) {
      bdep = max(bdep, tree.a[i].depth);
      if(tree.a[i].state == 'q')
        A2B.enqueue({0, i}), tree.depths[tree.a[i].depth]++, qSize++;
    }
    for(int i=0; i<tree2.treeSize; i++) {
      bdep = max(bdep, tree2.a[i].depth);
      if(tree2.a[i].state == 'q')
        A2B.enqueue({1, i}), tree2.depths[tree2.a[i].depth]++, qSize++;
    }
    search(th, nodelim, qSize);
  } else if(argc > 1 && string(argv[1]) == "distrib") {
    
  }
  return 0;
}