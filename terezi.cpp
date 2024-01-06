// TODO : cycle detection ; drifting rows ; symmetries ; smart stopping ; PRUNING.
// PRUNING done. cycle detection IMPOSSIBLE now. drifting rows ALSO PROBABLY IMPOSSIBLE
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

bool Compl3t34bl3(const vector<uint64_t>& state, int phase, uint64_t enforce) {
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
    for(int j=-1; j<=width; j++) if(enforce & (1ull<<(j+1))) {
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
/* u - unqueued; q - queued; d - decomissioned; c - valid completion for one side */
constexpr int TAG_VALID_COMPLETION = 1;
constexpr int TAG_RETURNING_NODE_TREE2 = (1<<30);
struct node { uint64_t row; int depth, parent; char state; int tags; };
struct searchTree {
  node* a;
  int depths[1000];
  int depthcnt[1000];
  int treeSize = 0, treeAlloc = 16777216;
  searchTree() { a = new node[treeAlloc]; }
  ~searchTree() { delete a; }
  void dumpTree(ostream f) {
    f << "BEGINTREE\n";
    f << treeSize << "\n";
    for(int i=0; i<treeSize; i++)
      f << a[i].row << ' ' << a[i].depth << ' ' << a[i].parent << ' ' << a[i].state << ' ' << a[i].tags;
    f << "ENDTREE\n";
  }
  void loadTree(istream f) { 
    for(int i=0; i<1000; i++) depthcnt[i] = depths[i] = 0;
    string buf;
    f >> buf; assert(buf == "BEGINTREE");
    f >> treeSize;
    for(int i=0; i<treeSize; i++)
      f >> a[i].row >> a[i].depth >> a[i].parent >> a[i].state >> a[i].tags;
    f >> buf; assert(buf == "ENDTREE");
    // TODO: delete nodes whose parents are still queued & maintain depths, depthcnt
    for(int i=0; i<treeSize; i++) {
      depths[a[i].depth] += (a[i].state == 'q'), depthcnt[a[i].depth]++;
    }
  }
  void flushTree() { }
  int newNode(node s = {0,0,0,' ',0}) {
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
void emit(vector<uint64_t> mat, bool compl3t3){
  if(!compl3t3) {
    if(mat.size() <= bdep) return;
    bdep = max(bdep, (int)mat.size());
  }
  else
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

uint64_t enforce, remember, enforce2, remember2;
uint64_t overlap = remember & remember2;

struct horseQueue {
  set<pair<long long,pair<int,int>>> internal;
  vector<int> targ;
  mt19937 rng;
  bool first = true;
  horseQueue() : rng(720) { }
  int size() { return internal.size(); }
  pair<int, int> popTop(int desired) {
    if(desired == -1) {
      pair<int,int> val = internal.begin()->second;
      internal.erase(internal.begin());
      return val;
    } else {
      auto it = internal.begin();
      while(it != internal.end() && it->second.first != desired) it++;
      if(it == internal.end()) return popTop(-1);
      pair<int,int> val = it->second;
      internal.erase(it);
      return val;
    }
  }
  void push(pair<int, int> nx) {
    auto& ref = nx.first?tree2:tree;
    vector<uint64_t> h = ref.getState(nx.second, 1);
    int wt = 0;
    for(; wt<min(targ.size(), h.size() - 2 * p) && 
      ((h[wt + 2 * p] >> __builtin_ctzll(overlap)) & 3) == targ[wt]; wt++);
    internal.insert({(-wt) * 65536 + rng() % 65536, nx});
  }
  void retarget(vector<int> newtarg) {
    if(first) { first = false; return; }
    // TODO: rewrite this. this ineff af
    vector<pair<int, int>> nxs;
    for(auto [a, b] : internal)
      nxs.push_back(b);
    internal.clear();
    targ = newtarg;
    for(auto b : nxs) push(b);
  }
} PQ;

#include "debug.h"

bool ORIGINALLY_COMPLETE = 0;

bool canMatch(int i, const vector<uint64_t>& pat, searchTree& ref, vector<int>& sol, vector<int>& lux) {
  if(ref.a[i].depth == pat.size()) {
    vector<uint64_t> marge = ref.getState(i, 1);
    for(int x=0; x<pat.size(); x++)
      marge[x] |= pat[x];
    emit(marge, (ref.a[i].tags & TAG_VALID_COMPLETION) && ORIGINALLY_COMPLETE);
    return 1;
  }
  if(ref.a[i].state != 'u') {
    assert(ref.a[i].state != 'd'); // TODO: *really*?
    return 1;
  }
  int f = sol[i], ok = 0;
  while(f != -1) {
    if((ref.a[f].row & overlap) == (pat[ref.a[i].depth] & overlap))
      ok += canMatch(f, pat, ref, sol, lux);
    f = lux[f];
  }
  return ok;
}

void preprune(int i, bool prune, int& p, vector<int>& sol, vector<int>& lux,
              vector<int>& osol, vector<int>& olux, searchTree& tr, searchTree& otr) {
  if(tr.a[i].state == 'd') return;
  // TODO canMatch is O(n). dp this
  ORIGINALLY_COMPLETE = tr.a[i].tags & TAG_VALID_COMPLETION;
  if(prune && i >= 2*p && !canMatch(0, tr.getState(i, 1), otr, osol, olux)){
    tr.a[i].state = 'd';
    return;
  }
  p++;
  int f = sol[i], ok = 0;
  while(f != -1) {
    preprune(f, prune, p, sol, lux, osol, olux, tr, otr);
    f = lux[f];
  }
}

void retargetDfs(vector<int>& besttarg, vector<int>& curttarg, int& bw, vector<int> t1nodes, vector<int> t2nodes,
                 vector<int>& sol1, vector<int>& lux1, vector<int>& sol2, vector<int>& lux2) {
  if(t1nodes.empty() || t2nodes.empty()) return;
  int a1=t1nodes.size(), b1=0, a2=t2nodes.size(), b2=0;
  for(int i:t1nodes) b1 += (tree.a[i].state == 'q');
  for(int i:t2nodes) b2 += (tree2.a[i].state == 'q');
  // weight: a1 * b2 + b1 * a2. i know this counts q-q twice!!!!!!!! >:]
  int sw = a1 * b2 + b1 * a2;
  if((sw > bw && curttarg.size() >= besttarg.size()) || (sw && curttarg.size() > besttarg.size())) {
    besttarg = curttarg;
    bw = sw;
  }
  vector<vector<int>> t1x(4), t2x(4);
  for(int i:t1nodes) {
    int f = sol1[i];
    while(f != -1) {
      if(tree.a[f].state != 'd') t1x[(tree.a[f].row >>__builtin_ctzll(overlap)) & 3].push_back(f);
      f = lux1[f];
    }
  }
  for(int i:t2nodes) {
    int f = sol2[i];
    while(f != -1) {
      if(tree2.a[f].state != 'd') t2x[(tree2.a[f].row >>__builtin_ctzll(overlap)) & 3].push_back(f);
      f = lux2[f];
    }
  }
  for(int nx = 0; nx < 4; nx++) {
    curttarg.push_back(nx);
    retargetDfs(besttarg, curttarg, bw, t1x[nx], t2x[nx], sol1, lux1, sol2, lux2);
    curttarg.pop_back();
  }
}

void prune() {
  BENCHMARK(pruning)
  int p2 = 0, x2 =0, p1 = 0, x1 = 0;
  vector<int> sol1(tree.treeSize, -1), lux1(tree.treeSize, -1);
  for(int i=1; i<tree.treeSize; i++)
    lux1[i] = sol1[tree.a[i].parent], sol1[tree.a[i].parent] = i;
  vector<int> sol2(tree2.treeSize, -1), lux2(tree2.treeSize, -1);
  for(int i=1; i<tree2.treeSize; i++)
    lux2[i] = sol2[tree2.a[i].parent], sol2[tree2.a[i].parent] = i;
  preprune(0, 0, x2, sol2, lux2, sol1, lux1, tree2, tree);
  preprune(0, 1, p2, sol2, lux2, sol1, lux1, tree2, tree);
  preprune(0, 0, x1, sol1, lux1, sol2, lux2, tree, tree2);
  preprune(0, 1, p1, sol1, lux1, sol2, lux2, tree, tree2);
  BENCHMARKEND(pruning)
  BENCHMARK(targfinding)
  // TODO: this ineff af asw.
  vector<int> besttarg, curttarg{(int)((tree2.a[0].row >>__builtin_ctzll(overlap)) & 3)}; int bw = 0;
  retargetDfs(besttarg, curttarg, bw, {0}, {0}, sol1, lux1, sol2, lux2);
  besttarg.erase(besttarg.begin(), besttarg.begin() + 2 * p);
  BENCHMARKEND(targfinding)
  BENCHMARK(retargeting)
  PQ.retarget(besttarg);
  BENCHMARKEND(retargeting)

  cout<<"[1NFO] PRUN1NG: "<<x1<<" -> "<<p1<<"/"<<tree.treeSize<<", "<<x2<<" -> "<<p2<<"/"<<tree2.treeSize<<endl;
  cout << "[1NFO] N3W T4RG3T: ";
  for(int i:besttarg)cout<<i;
  cout<<endl;
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
          B2A.enqueue({x, dep+1, nx.second, ' ', 0 | (nx.first ? TAG_RETURNING_NODE_TREE2 : 0)});
      });
    }
    B2A.enqueue({0, dep, -nx.second, ' ', 0 | (nx.first ? TAG_RETURNING_NODE_TREE2 : 0)});
  }
}

vector<uint64_t> filters;

void search(int th, int nodelim, int qSize) {
  int solved = 0, onx;
  vector<thread> universes;
  for(int i=0;i<th;i++) universes.emplace_back(betaUniverse);
  node x;
  int reportidx = 0;
  int sdep = bdep;
  auto report = [&] {
    cout << "[1NFO] SOLV3D ";
    cout << solved << "; QU3U3D " << qSize + PQ.size() << "; TOT4L " << tree.treeSize<<'+'<<tree2.treeSize << " NOD3S";
    cout << endl;
    if(reportidx % 8 == 0){
      cout << "[1NFO] D3PTH R34CH3D "<< sdep<<" ; TR33 PROF1L3";
      for(int i=2*p;i<=sdep;i++){
        if(tree.depths[i] == 0 && tree2.depths[i] == 0) cout<<" 0";
        else cout<<' '<<tree.depths[i]<<'+'<<tree2.depths[i];
        if(tree.depths[i] == tree.depthcnt[i] && tree2.depths[i] == tree2.depthcnt[i]);
        else cout<<'/'<<tree.depthcnt[i]<<'+'<<tree2.depthcnt[i];
      }
      cout<<endl;
      prune();
    }
    reportidx++;
  };
  while(B2A.wait_dequeue(x), 1) {
    auto& ref = (x.tags & TAG_RETURNING_NODE_TREE2) ? tree2 : tree;
    if(x.parent < 0) {
      ref.depths[x.depth]--, solved++, --qSize;
      if(ref.a[-x.parent].state == 'q')
        ref.a[-x.parent].state = 'u';
      if(solved % 16 == 0) report();
    } else {
      if((x.depth-1) / p < filters.size() && ((x.row & filters[(x.depth-1)/p]) != x.row)) {
        // cout << "[1NFO] NOD3 1GNOR3D" << endl;
      } else {
        x.state = 'q', onx = ref.newNode(x);
        const vector<uint64_t> st = ref.getState(onx);
        if(Compl3t34bl3(st, ref.a[onx].depth % p, (x.tags & TAG_RETURNING_NODE_TREE2) ? enforce2 : enforce)) {
          cout << "[1NFO] TR33 " << !!(x.tags & TAG_RETURNING_NODE_TREE2) << " NOD3 " << onx << " 1S 4 V4L1D COMPL3T1ON" << endl;
          ref.a[onx].tags |= TAG_VALID_COMPLETION;
        }
        PQ.push({!!(x.tags & TAG_RETURNING_NODE_TREE2), onx});
        sdep = max(sdep, x.depth);
      }
    }
    while(PQ.size() && qSize <= 2*th && nodelim) {
      pair<int,int> best = PQ.popTop((solved % 3) - 1);
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
      cout << "OTH3R SYMM3TR13S T3MPOR4R1LY NOT T3ST3D >:[" << endl;
    int bthh = (sym ? width/4 : width/2);
    cout<<"B1S3CT1ON THR3SHOLD: [SUGG3ST3D " << bthh << "] "<<endl; cin >> bthh;
    // cout<<"NUMB3R OF ROWS 4DV4NC3 FOR 1NN3R TR33: [0 1S OK4Y] "<<endl; cin >> rows_priority;
    for(int j=-1; j<=width; j++) {
      int s = ((sym==1) ? min(j, width - j - 1) : j);
      if(0 <= s && s < bthh + 1) remember2 |= (1ull << j);
      if(bthh - 1 <= s && s < width) remember |= (1ull << j);
      if(s < bthh) enforce2 |= (1ull << (j+1));
      else enforce |= (1ull << (j+1));
    }
    cout<<"F1RST "<<2*p<<" ROWS:"<<endl;
    overlap = remember & remember2;

    int nx = -1, onx = -1;
    for(int i=0;i<2*p;i++){
      uint64_t x=0;
      string s; cin>>s;
      for(int j=0;j<width;j++)if(s[j]=='o')x|=(1ull<<j);
      nx = tree.newNode({x, i+1, onx, 'u', 0});
      nx = tree2.newNode({x, i+1, onx, 'u', 0});
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
    cout << "F1LT3R ROWS: [1F YOU DON'T KNOW WH4T TH1S 1S, 3NT3R 0!] " << endl;
    int filterrows; cin>>filterrows;
    for(int i=0;i<filterrows;i++){
      uint64_t x=0;
      string s; cin>>s;
      for(int j=0;j<width;j++)if(s[j]=='o')x|=(1ull<<j);
      filters.push_back(x);
    }
    search(th, nodelim, qSize);
  } else if(argc > 1 && string(argv[1]) == "distrib") {
    
  }
  return 0;
}