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

int th, nodelim = -1, deplim = 1000, l4h;
int p, width, sym;
int maxwid, stator, bthh;
int qSize = 0;

uint64_t enforce, remember, enforce2, remember2;
uint64_t overlap = remember & remember2;

vector<uint64_t> filters;

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
// auto idx = [&](int j) { return ((sym==1) ? min(j, width - j - 1) : j); };
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

void genNextRows(vector<uint64_t>& state, int depth, int ahead, uint64_t enforce, uint64_t remember, auto fn) {
  assert(p*2 == sz(state));
  int phase = depth % p;
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
  for(int row=0; row<sz(state)+ahead; row++){
    int r=(row+phase)/p, t=(row+phase)%p;
    if(((row+depth-2*p)/p) < (int)filters.size())
      for(int j=0; j<width; j++)
        if(!(filters[(row+depth-2*p)/p] & (1ull<<j)))
          eqify(get(r, j, t), 2-0);
  }
  vector<int> crit; {
      set<int> _crit;
      for(int j=0; j<width; j++) if(remember&(1<<j)) _crit.insert(get(0, j, -1));
       crit = vector<int>(_crit.begin(), _crit.end()); }
  vector<uint64_t> bb(state.begin()+1, state.end());
  solve(inst, crit, [&](vector<int> sol){
    uint64_t x = 0;
    for(int j=0; j<width; j++) if(remember&(1<<j)) {
      for(int sdx=0; sdx<(int)crit.size(); sdx++) if(crit[sdx] == get(0, j, -1))
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
  void dumpTree(ostream& f) {
    f << "BEGINTREE\n";
    f << treeSize << "\n";
    for(int i=0; i<treeSize; i++)
      f << a[i].row << ' ' << a[i].parent << ' ' << a[i].state << ' ' << a[i].tags << '\n';
    f << "ENDTREE\n";
  }
  void loadTree(istream& f) { 
    for(int i=0; i<1000; i++) depthcnt[i] = depths[i] = 0;
    string buf;
    f >> buf; assert(buf == "BEGINTREE");
    f >> treeSize;
    for(int i=0; i<treeSize; i++) {
      f >> a[i].row >> a[i].parent >> a[i].state >> a[i].tags;
      a[i].depth = 1 + (i ? a[a[i].parent].depth : 0);
    }
    f >> buf; assert(buf == "ENDTREE");
    for(int i=1; i<treeSize; i++)
      if(a[a[i].parent].state == 'd')
        a[i].state = 'd';
    int cnt = 0;
    for(int i=1; i<treeSize; i++)
      if(a[i].state != 'd' && (a[a[i].parent].state == 'd' || a[a[i].parent].state == 'q'))
        a[i].state = 'd', cnt++;
    if(cnt != 0) std::cout << "[1NFO] " << cnt << " NOD3S 1NV4L1D4T3D" << std::endl;
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

#include "debug.h"

int bdep = 0;
void emit(vector<uint64_t> mat, bool compl3t3){
  if(!compl3t3) {
    if((int)mat.size() <= bdep) return;
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

struct horseQueue {
  set<pair<long long,pair<int,int>>> internal;
  vector<uint64_t> targ;
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
    for(; (wt<min((int)targ.size(), (int)h.size() - 2 * p)) && 
      ((h[wt + 2 * p] >> __builtin_ctzll(overlap)) & 3) == targ[wt]; wt++);
    if(wt == min((int)targ.size(), (int)h.size() - 2 * p)) wt = 1000 - wt;
    internal.insert({(-wt) * 65536 + rng() % 65536, nx});
  }
  void retarget(vector<uint64_t> newtarg) {
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



void retargetDfs(vector<uint64_t>& besttarg, vector<uint64_t>& curttarg, int& bw, vector<int> t1nodes, vector<int> t2nodes, bool absolved,
                 vector<int>& sol1, vector<int>& lux1, vector<int>& sol2, vector<int>& lux2) {
  // PRUNE
  if(t1nodes.empty() || t2nodes.empty()) {
    if(!absolved) { // CROW STRIDER
      for(int i:t1nodes) tree.a[i].state = 'd';
      for(int i:t2nodes) tree2.a[i].state = 'd';
    }
    return;
  }
  // TODO: also try matching a with b, lookahead for combination, etc..
  {
    int i = t1nodes[0], j = t2nodes[0];
    for(int ri:t1nodes) if(tree.a[i].tags & TAG_VALID_COMPLETION) i = ri;
    for(int rj:t2nodes) if(tree2.a[i].tags & TAG_VALID_COMPLETION) j = rj;
    vector<uint64_t> marge = tree.getState(i, 1);
    vector<uint64_t> marge2 = tree2.getState(j, 1);
    for(int x=0; x<(int)marge.size(); x++) marge[x] |= marge2[x];
    emit(marge, (tree.a[i].tags & tree2.a[j].tags) & TAG_VALID_COMPLETION);
  }
  
  // for(int i:t1nodes) for(int j:t2nodes) {
  //   vector<uint64_t> marge = tree.getState(i, 1);
  //   vector<uint64_t> marge2 = tree2.getState(j, 1);
  //   for(int x=0; x<(int)marge.size(); x++) marge[x] |= marge2[x];
  //   emit(marge, (tree.a[i].tags & tree2.a[j].tags) & TAG_VALID_COMPLETION);
  // }
  // FIND TARGET
  int a1=t1nodes.size(), b1=0, a2=t2nodes.size(), b2=0;
  for(int i:t1nodes) b1 += (tree.a[i].state == 'q');
  for(int i:t2nodes) b2 += (tree2.a[i].state == 'q');
  absolved = absolved || (b1 + b2);
  // weight: a1 * b2 + b1 * a2. i know this counts q-q twice!!!!!!!! >:]
  int sw = a1 * b2 + b1 * a2;
  if((sw > bw && curttarg.size() >= besttarg.size()) || (sw && curttarg.size() > besttarg.size()))
    besttarg = curttarg, bw = sw;
  vector<vector<int>> t1x(4), t2x(4);
  for(int i:t1nodes) {
    int f = sol1[i];
    while(f != -1) {
      if(tree.a[f].state != 'd') t1x[(tree.a[f].row >>__builtin_ctzll(overlap)) & 3].push_back(f);
      f = lux1[f]; }
  }
  for(int i:t2nodes) {
    int f = sol2[i];
    while(f != -1) {
      if(tree2.a[f].state != 'd') t2x[(tree2.a[f].row >>__builtin_ctzll(overlap)) & 3].push_back(f);
      f = lux2[f]; }
  }
  if(besttarg == curttarg) {
    cout << "[1NFO] ";
    for(int nx=0; nx<4; nx++) cout << t1x[nx].size() << ' ' << t2x[nx].size() << ' ';
    cout << endl;
  }
  for(int nx = 0; nx < 4; nx++) {
    curttarg.push_back(nx);
    retargetDfs(besttarg, curttarg, bw, t1x[nx], t2x[nx], absolved, sol1, lux1, sol2, lux2);
    curttarg.pop_back();
  }
}

void prune() {
  BENCHMARK(pruning)
  vector<int> sol1(tree.treeSize, -1), lux1(tree.treeSize, -1), halted1(tree.treeSize);
  for(int i=1; i<tree.treeSize; i++)
    lux1[i] = sol1[tree.a[i].parent], sol1[tree.a[i].parent] = i, 
    halted1[i] = halted1[tree.a[i].parent] || (tree.a[i].state == 'd');
  int x1 = 0, p1 = 0; for(int i=0; i<tree.treeSize; i++) x1 += !halted1[i];
  vector<int> sol2(tree2.treeSize, -1), lux2(tree2.treeSize, -1), halted2(tree2.treeSize);
  for(int i=1; i<tree2.treeSize; i++)
    lux2[i] = sol2[tree2.a[i].parent], sol2[tree2.a[i].parent] = i,
    halted2[i] = halted2[tree2.a[i].parent] || (tree2.a[i].state == 'd');
  int x2 = 0, p2 = 0; for(int i=0; i<tree2.treeSize; i++) x2 += !halted2[i];
  BENCHMARKEND(pruning)
  BENCHMARK(targfinding)
  vector<uint64_t> besttarg, curttarg{(uint64_t)((tree2.a[0].row >>__builtin_ctzll(overlap)) & 3)}; int bw = 0;
  retargetDfs(besttarg, curttarg, bw, {0}, {0}, 0, sol1, lux1, sol2, lux2);
  if(besttarg.size())
    besttarg.erase(besttarg.begin(), besttarg.begin() + 2 * p);
  for(int i=1; i<tree.treeSize; i++)
    halted1[i] = halted1[tree.a[i].parent] || (tree.a[i].state == 'd');
  for(int i=0; i<tree.treeSize; i++) p1 += !halted1[i];
  for(int i=1; i<tree2.treeSize; i++)
    halted2[i] = halted2[tree2.a[i].parent] || (tree2.a[i].state == 'd');
  for(int i=0; i<tree2.treeSize; i++) p2 += !halted2[i];
  BENCHMARKEND(targfinding)
  BENCHMARK(retargeting)
  if(besttarg.size())
    PQ.retarget(besttarg);
  else
    cout << "[1NFO] F41L3D TO F1ND T4RG3T! >:[" << endl;
  BENCHMARKEND(retargeting)

  cout<<"[1NFO] PRUN1NG: "<<x1<<" -> "<<p1<<"/"<<tree.treeSize<<", "<<x2<<" -> "<<p2<<"/"<<tree2.treeSize<<endl;
  cout << "[1NFO] N3W T4RG3T: ";
  for(int i:besttarg)cout<<i;
  cout<<' ' << bw << endl;
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
      genNextRows(h, dep, l4h, 
        nx.first ? enforce2 : enforce,
        nx.first ? remember2 : remember, 
        [&](uint64_t x){ 
          B2A.enqueue({x, dep+1, nx.second, ' ', 0 | (nx.first ? TAG_RETURNING_NODE_TREE2 : 0)});
      });
    }
    B2A.enqueue({0, dep, -nx.second, ' ', 0 | (nx.first ? TAG_RETURNING_NODE_TREE2 : 0)});
  }
}

void search(int th) {
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
    if(reportidx % 1 == 0){
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
      if(solved % (64*64) == 0) report();
    } else {
      if((x.depth-1) / p < (int)filters.size() && ((x.row & filters[(x.depth-1)/p]) != x.row)) {
        cout << x.row << ' ' << filters[(x.depth-1)/p] << endl;
        assert(0);
        // cout << "[1NFO] NOD3 1GNOR3D" << endl;
      } else {
        x.state = 'q', onx = ref.newNode(x);
        const vector<uint64_t> st = ref.getState(onx);
        if(Compl3t34bl3(st, ref.a[onx].depth % p, (x.tags & TAG_RETURNING_NODE_TREE2) ? enforce2 : enforce)) {
          cout << "[1NFO] TR33 " << !!(x.tags & TAG_RETURNING_NODE_TREE2) << " NOD3 " << onx << " 1S 4 V4L1D COMPL3T1ON" << endl;
          ref.a[onx].tags |= TAG_VALID_COMPLETION;
        }
        PQ.push({!!(x.tags & TAG_RETURNING_NODE_TREE2), onx});
        // if(x.depth < deplim) {
        //   A2B.enqueue({!!(x.tags & TAG_RETURNING_NODE_TREE2), onx}), qSize++;
        // }
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

void calculateMasks(int bthh) {
  enforce = remember = enforce2 = remember2 = 0;
  for(int j=-1; j<=width; j++) {
    int s = ((sym==1) ? min(j, width - j - 1) : j);
    if(0 <= s && s < bthh + 1) remember2 |= (1ull << j);
    if(bthh - 1 <= s && s < width) remember |= (1ull << j);
    if(s < bthh) enforce2 |= (1ull << (j+1));
    else enforce |= (1ull << (j+1));
  }
  overlap = remember & remember2;
}

void loadf(istream& f) {
  f >> th >> l4h;
  f >> p >> width >> sym >> stator;
  f >> bthh; calculateMasks(bthh);
  int filterrows;
  f >> filterrows; filters = vector<uint64_t>(filterrows); 
  for(int i=0; i<filterrows; i++) f >> filters[i];
  tree.loadTree(f);
  tree2.loadTree(f);
}

void dumpf(ostream& f) {
  f << th << ' ' << l4h << '\n';
  f << p << ' ' << width << ' ' << sym << ' ' << stator << '\n';
  f << bthh << '\n'; 
  f << filters.size() << '\n';
  for(auto i:filters) f << i << ' ';
  f << '\n';
  tree.dumpTree(f);
  tree2.dumpTree(f);
  f.flush();
}

int main(int argc, char* argv[]) {
  cout<<primeImplicants.size()<<" PR1M3 1MPL1C4NTS"<<endl;
  set<string> args; for(int i=1; i<argc; i++) args.insert(argv[i]);
  if(filesystem::exists("dump.txt") && args.count("continue")) {
    ifstream f("dump.txt");
    loadf(f);
  } else {
    string x = "n"; 
    if(filesystem::exists("dump.txt")) {
      cout << "dump.txt 3XS1STS. CONT1NU3 S34RCH? [Y/N] " << endl;
      cin >> x;
    }
    if(x[0] == 'y' || x[0] == 'Y') {
      ifstream f("dump.txt");
      loadf(f);
      cout<<"THR34DS, LOOK4H34D: ["<<th<<' '<<l4h<<"] "<<endl;
      cin>>th>>nodelim>>l4h;
    } else {
      cout<<"THR34DS, LOOK4H34D: "<<endl; cin>>th>>l4h;
      cout<<"P3R1OD, W1DTH, SYMM3TRY, ST4TOR W1DTH: "<<endl; cin>>p>>width>>sym>>stator;
      if(sym != 0 && sym != 1)
        cout << "OTH3R SYMM3TR13S T3MPOR4R1LY NOT T3ST3D >:[" << endl;
      bthh = (sym ? width/4 : width/2);
      cout<<"B1S3CT1ON THR3SHOLD: [SUGG3ST3D " << bthh << "] "<<endl; cin >> bthh;
      calculateMasks(bthh);
      cout<<"F1RST "<<2*p<<" ROWS:"<<endl;
      for(int i=0;i<2*p;i++){
        uint64_t x=0;
        string s; cin>>s;
        for(int j=0;j<width;j++)if(s[j]=='o')x|=(1ull<<j);
        tree.newNode({x, i+1, i-1, (i == 2*p-1)?'q':'u', 0});
        tree2.newNode({x, i+1, i-1, (i == 2*p-1)?'q':'u', 0});
      }
      cout << "4DD1TION4L OPT1ONS? [Y/N] " << endl;
      string options; cin >> options;
      if(options[0] == 'y' || options[0] == 'Y') {
        cout << "F1LT3R ROWS: " << endl;
        int filterrows; cin>>filterrows;
        for(int i=0;i<filterrows;i++){
          uint64_t x=0;
          string s; cin>>s;
          for(int j=0;j<width;j++)if(s[j]=='o')x|=(1ull<<j);
          filters.push_back(x);
        }
        cout << "NOD3 L1M1T, D3PTH L1M1T: " << endl;
        cin >> nodelim >> deplim;
      }
    }
  }
  for(string i:args) if(i.substr(0, 2) == "dp") deplim = stoi(i.substr(2));
  if(args.count("splitdistrib")) {
    // SPL1T S34RCH TO MUTU4LLY 1ND3P3ND3NT S4V3F1L3S
  }
  if(args.count("search"))  {
    for(int i=0; i<tree.treeSize; i++)
      if(tree.a[i].state == 'q')
        A2B.enqueue({0, i}), qSize++;
    for(int i=0; i<tree2.treeSize; i++)
      if(tree2.a[i].state == 'q')
        A2B.enqueue({1, i}), qSize++;
    cout << qSize << endl;
    search(th);
    ofstream dump("dump.txt");
    dumpf(dump);
    dump.close();
  } else if(args.count("distrib")) {
    
  }
  return 0;
}