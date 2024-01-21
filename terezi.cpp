// TODO : cycle detection ; drifting rows ; symmetries ; smart stopping ; PRUNING.
#include <bits/stdc++.h>
#include "cadical/src/cadical.hpp"
#include "cqueue/bcq.h"
using namespace std;

#define sz(x) ((int)(x.size()))

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

void genNextRows(vector<uint64_t>& state, int phase, int ahead, auto fn) {
  assert(p*2 == sz(state));
  vector<int> inst = {1, 0, -2, 0};
  auto idx = [&](int j) { return ((sym==1) ? min(j, width - j - 1) : j); };
  auto get = [&](int r, int j, int t) {
    if(t == p) t = 0, j = (sym == 2) ? width - j - 1 : j;
    r = r*p+t-phase;
    if(j <= -1 || j >= width) return (2-0);
    if(r < sz(state)) return (2-!!(state[r]&(1ull<<j)));
    return 3 + idx(j) + (int)(r-state.size())*width;
  };
  auto eqify = [&](int i, int j) { for(int x:vector<int>{i,-j,0,-i,j,0}) inst.push_back(x); };
  for(int row=state.size(); row<sz(state)+ahead; row++){
    int r=(row+phase)/p, t=(row+phase)%p;
    for(int j=-1; j<=width; j++)
      trans({get(r-2,j-1,t),get(r-2,j,t),get(r-2,j+1,t),
             get(r-1,j-1,t),get(r-1,j,t),get(r-1,j+1,t),
             get(r,j-1,t),get(r,j,t),get(r,j+1,t),get(r-1,j,t+1)}, inst);
    if(t != p-1) for(int j=0; j<width; j++)
      if(j < stator || width - j <= stator)
        eqify(get(r, j, t), get(r, j, t+1));
  }
  vector<int> crit(max(idx(width/2),idx(width-1))+1); iota(crit.begin(), crit.end(), 3);
  solve(inst, crit, [&](vector<int> sol){
    uint64_t x = 0;
    for(int j=0; j<width; j++) if(sol[idx(j)]>0) x|=(1ull<<j);
    fn(x);
  });
}

void genNextRows_dualEncode(vector<vector<uint64_t>>& state, int phase, int ahead, auto fn) {
  assert(p*2 == sz(state[0]));
  int cntx = state.size();
  vector<int> inst = {1, 0, -2, 0};
  auto idx = [&](int j) { return ((sym==1) ? min(j, width - j - 1) : j); };
  auto get = [&](int r, int j, int t) {
    if(t == p) t = 0, j = (sym == 2) ? width - j - 1 : j;
    r = r*p+t-phase;
    if(j <= -1 || j >= width) return (2-0);
    // if(r < sz(state)) return (2-!!(state[r]&(1ull<<j)));
    // return 3 + idx(j) + (int)(r-state.size())*width;
    return 3 + idx(j) + (int)(r)*width;
  };
  auto imply = [&](int i, int j) { for(int x:vector<int>{-i,j,0}) inst.push_back(x); };
  auto eqify = [&](int i, int j) { imply(i, j), imply(-i, -j); };
  for(int row=state[0].size(); row<sz(state[0])+ahead; row++){
    int r=(row+phase)/p, t=(row+phase)%p;
    for(int j=-1; j<=width; j++)
      trans({get(r-2,j-1,t),get(r-2,j,t),get(r-2,j+1,t),
             get(r-1,j-1,t),get(r-1,j,t),get(r-1,j+1,t),
             get(r,j-1,t),get(r,j,t),get(r,j+1,t),get(r-1,j,t+1)}, inst);
    if(t != p-1) for(int j=0; j<width; j++)
      if(j < stator || width - j <= stator)
        eqify(get(r, j, t), get(r, j, t+1));
  }
  vector<int> altvars(cntx); iota(altvars.begin(), altvars.end(), 3 + (sz(state[0])+ahead)*width);
  for(int i=0; i<cntx;i++)
    for(int row=0; row<sz(state[0]); row++){
      int r=(row+phase)/p, t=(row+phase)%p;
      for(int j=0; j<width; j++)
        imply(altvars[i], (state[i][row]&(1ull<<j)) ? get(r,j,t) : -get(r,j,t));
    }
  for(int i:altvars) inst.push_back(i); 
  inst.push_back(0);
  for(int i:altvars) for(int j: altvars) if(i != j) imply(i, -j);
  vector<int> crit(max(idx(width/2),idx(width-1))+1); iota(crit.begin(), crit.end(), 3+2*p*width);
  crit.insert(crit.end(), altvars.begin(), altvars.end());
  solve(inst, crit, [&](vector<int> sol){
    uint64_t x = 0;
    for(int j=0; j<width; j++) if(sol[idx(j)]>0) x|=(1ull<<j);
    int idx = -1;
    for(int i=0; i<cntx; i++) if(sol[sol.size()-cntx+i]>0) {
      assert(idx == -1);
      idx = i;
    }
    assert(idx != -1);
    fn(idx, x);
  });
}

}; using namespace _logic;

namespace _searchtree {
int depths[1000], total[1000];
struct node { uint64_t row; int depth, shift, parent; char state; };
node* tree = new node[16777216];
int treeSize = 0, treeAlloc = 16777216;

void dumpTree(string fn) {
  ofstream fx(fn);
  fx<<p<<'.'<<width<<'.'<<sym<<'.'<<stator<<'.'<<"period.width.sym.stator"<<'\n';  
  fx<<treeSize<<'\n';
  for(int i=0;i<treeSize;i++)fx<<tree[i].row<<' '<<tree[i].shift<<' '<<tree[i].parent<<' '<<tree[i].state<<'\n';
  fx.flush(); fx.close();
}

void loadTree(string fn) {
  ifstream fx(fn);
  string s; fx>>s; for(char&c:s) if(c=='.') c=' ';
  istringstream gx(s); vector<string> keys; map<string, string> altkeys; 
  string key; while(gx >> key) keys.push_back(key);
  for(int i=0; i<keys.size()/2; i++) altkeys[keys[i+keys.size()/2]] = keys[i];
  p = stoi(altkeys["period"]); width = stoi(altkeys["width"]); sym = stoi(altkeys["sym"]);
  if(altkeys.count("stator")) stator = stoi(altkeys["stator"]);
  fx>>treeSize;
  for(int i=0;i<treeSize;i++){
    fx>>tree[i].row>>tree[i].shift>>tree[i].parent>>tree[i].state;
    tree[i].depth = 1 + (i ? tree[tree[i].parent].depth : 0);
  }
}

void flushTree(node* dest = tree) { assert(0); }

int newNode() {
  if(treeSize == treeAlloc)
    treeAlloc *= 2, flushTree(new node[treeAlloc]);
  // if((treeSize - time(0)) % 8192 == 0) flushTree();
  tree[treeSize] = {0,0,0,0,'0'};
  return treeSize++;
}

vector<uint64_t> getState(int onx) {
  vector<uint64_t> mat;
  while(onx != -1)
    mat.push_back(tree[onx].row), onx = tree[onx].parent;
  reverse(mat.begin(), mat.end());
  return vector<uint64_t>(mat.end() - 2*p, mat.end());
}

int getWidth(int i) {
  uint64_t bito = 0;
  for(uint64_t x:getState(i)){ bito |= x; }
  return 64-__builtin_clzll(bito)-__builtin_ctzll(bito);
}
}; using namespace _searchtree;

int bdep = 0;
void emit(int state, bool d = true){
  vector<uint64_t> mat;
  while(state != -1)
    mat.push_back(tree[state].row), state = tree[state].parent;
  reverse(mat.begin(), mat.end());
  if(!d) {
    if(sz(mat) <= bdep) return;
    bdep = mat.size();
  }else
    cout<<"[[OSC1LL4TOR COMPL3T3!!!]]"<<endl;
  std::string rle;
  int cnt = 0, cur = 0;
  auto f = [&](char x) {
    if(x != cur) {
      if(cnt >= 2) rle += to_string(cnt);
      if(cnt >= 1) rle += (char)cur;
      cnt = 0, cur = x;
    }
    cnt++;
  };
  for(int r=0; r*p<sz(mat); r++){
    if(r) f('$');
    for(int x=r*p; x<sz(mat) && x<(r+1)*p; x++){
      for(int j=0;j<width;j++) f("bo"[!!(mat[x]&(1ull<<j))]);
      f('b');f('b');f('b');
    }
  }
  f('!'); 
  cout << "x = 0, y = 0, rule = B3/S23\n" + rle + '!' << endl;
  if(d) exit(0);
}

moodycamel::BlockingConcurrentQueue<vector<int>> A2B;
moodycamel::BlockingConcurrentQueue<node> B2A;

void betaUniverse() {
  vector<int> nxs;
  // todo: this can't terminate then, might deadlock
  while(A2B.wait_dequeue(nxs), nxs[0] != -1){
    vector<vector<uint64_t>> h;
    for(int i:nxs) h.push_back(getState(i));
    int dep = tree[nxs[0]].depth;
    genNextRows_dualEncode(h, dep%p, l4h, [&](int idx, uint64_t x){ 
      B2A.enqueue({x, dep+1, 0, nxs[idx], '0'});
    });
    for(int nx:nxs)
      B2A.enqueue({0, dep, 0, -nx, '0'});
    nxs.clear();
  }
}

void search(int th, int nodelim, int qSize) {
  int solved = 0, onx;
  vector<thread> universes;
  for(int i=0;i<th;i++) universes.emplace_back(betaUniverse);
  node x;
  int reportidx = 0, nsolved = 0;
  priority_queue<pair<int,int>> PQ;
  auto report = [&] {
    cout << "[1NFO] SOLV3D ";
    cout << solved << " (" << solved-nsolved << "); QU3U3D " << qSize + PQ.size() << "; TOT4L " << treeSize << " NOD3S";
    cout << endl;
    nsolved=solved;
    if(reportidx % 8 == 0){
      cout << "[1NFO] D3PTH R34CH3D "<< bdep<<" ; TR33 PROF1L3 ";
      for(int i=2*p;i<=bdep;i++){
        cout<<' '<<depths[i];
        if(depths[i] != total[i])cout<<'/'<<total[i];
      }
      cout<<endl;
    }
    reportidx++;
  };
  auto t1 = chrono::high_resolution_clock::now();
  while(B2A.wait_dequeue(x), 1) {
    if(x.parent < 0) {
      depths[x.depth]--, solved++, tree[-x.parent].state = 'u';
      --qSize;
    } else {
      // if(x.row == tree[x.parent].row && x.depth == 6) continue;
      x.state = 'q', tree[onx = newNode()] = x;
      depths[x.depth]++, total[x.depth]++;;
      PQ.push({-x.depth, onx});
      emit(onx, Compl3t34bl3(getState(onx), x.depth%p));
    }
    vector<int> blk; int tdep = -1;
    if(qSize <= 6*th) while(PQ.size() && qSize <= 6*th+10 && nodelim) {
      int best = PQ.top().second, dep = PQ.top().first; PQ.pop();
      if(blk.size() && ((blk.size() == 5) || (tdep != dep))) A2B.enqueue(blk), blk.clear();
      blk.push_back(best), tdep = dep, qSize++, nodelim--;
    }
    if(blk.size()) A2B.enqueue(blk);
    if(!qSize) break;
    auto t2 = chrono::high_resolution_clock::now();
    auto d = t2 - t1;
    if((t2-t1) > 10s) {
      t1 = t2;
      report();
    }
  }
  for(int i=0;i<th;i++) A2B.enqueue({-1});
  for(thread& x:universes) x.join();
  reportidx = 0;
  report();
}

int main(int argc, char* argv[]) {
  cout<<primeImplicants.size()<<" PR1M3 1MPL1C4NTS"<<endl;
  if(argc > 1 && string(argv[1]) == "search")  {
    int th, nodelim, qSize = 0;
    cout<<"THR34DS, NOD3 L1M1T, LOOK4H34D: "<<endl; cin>>th>>nodelim>>l4h;
    if(filesystem::exists("dump.txt"))
      loadTree("dump.txt");
    else {
      cout<<"P3R1OD, W1DTH, SYMM3TRY, ST4TOR W1DTH: "<<endl; cin>>p>>width>>sym>>stator;
      cout<<"F1RST "<<2*p<<" ROWS:"<<endl;
      int nx = -1, onx = -1;
      for(int i=0;i<2*p;i++){
        uint64_t x=0;
        string s; cin>>s;
        for(int j=0;j<width;j++)if(s[j]=='o')x|=(1ull<<j);
        tree[nx = newNode()] = {x, i+1, 0, onx, 'u'}, onx = nx;
      }
      tree[onx].state = 'q';
    }
    for(int i=0; i<treeSize; i++) {
      total[tree[i].depth]++, bdep = max(bdep, tree[i].depth);
      // if(tree[i].state == 'q' && tree[i].depth <= deplim){
      if(tree[i].state == 'q'){
        A2B.enqueue({i}), depths[tree[i].depth]++, qSize++;
      }
    }
    search(th, nodelim, qSize);
  } else if(argc > 1 && string(argv[1]) == "distrib") {
    bool xall = (argc > 2);
    assert(filesystem::exists("dump.txt"));
    loadTree("dump.txt");
    // vector<vector<uint64_t>> r(treeSize);
    // for(int i=1;i<treeSize;i++) r[tree[i].parent].push_back(tree[i].row);
    // for(int i=0;i<treeSize;i++){
    //   cout<<i<<endl;
    //   set<uint64_t> f(r[i].begin(), r[i].end());
    //   assert(f.size() == r[i].size());
    //   if(tree[i].state == 'q') assert(f.size() == 0);
    // }
    // return 0;
    cout<<"LOOK4H34D: "<<endl; cin>>l4h;
    ifstream fin("pending.txt");
    int nx, onx, solved = 0, solutions = 0;
    int cnt = 0;
    while(filesystem::exists("tasks/tasks"+to_string(cnt)+".txt")) cnt++;
    ofstream fx("tasks/tasks"+to_string(cnt)+".txt");
    fx<<p<<' '<<width<<' '<<sym<<' '<<l4h<<'\n';
    while(fin >> nx) {
      if(tree[nx].state != 'q') cout << "[W4RN1NG] "<<nx<<" ST4T3 DUPL1C4T3"<< endl;
      std::string r;
      while(fin >> r) {
        if(r == "-") break;
        if(tree[nx].state != 'q') continue;
        node x = {stoull(r), tree[nx].depth+1, 0, nx, 'q'};
        tree[onx = newNode()] = x, solutions++;
        emit(onx, Compl3t34bl3(getState(onx), x.depth%p));
        if(!xall){
          fx<<onx<<' '<<x.depth%p;
          for(uint64_t x:getState(onx)) fx<<' '<<x;
          fx << '\n';
        }
      }
      if(tree[nx].state == 'q') tree[nx].state = 'u', solved++;
    }
    fin.close();
    cout << "[1NFO] " << solved << " NOD3S SOLV3D " << solutions << " SOLUT1ONS" << endl;
    filesystem::rename("pending.txt", "pending-old.txt");
    map<int,int> wp;
    for(int i=0; i<treeSize; i++) {
      total[tree[i].depth]++, bdep = max(bdep, tree[i].depth);
      if(tree[i].state == 'q'){
        depths[tree[i].depth]++;
        wp[getWidth(i)]++;
        if(xall){
          fx<<i<<' '<<tree[i].depth%p;
          for(uint64_t x:getState(i)) fx<<' '<<x;
          fx << '\n';
        }
      }
    }
    cout << "[1NFO] D3PTH PROF1L3 ";
    for(int i=2*p;i<=bdep;i++){
      cout<<' '<<depths[i];
      if(depths[i] != total[i])cout<<'/'<<total[i];
    }
    cout<<endl;
    cout << "[1NFO] W1DTH PROFIL3 ";
    for(int i=1;i<=width;i++)cout<<' '<<wp[i];
    cout<<endl;
    fx.flush(); fx.close();
  }
  int idx = 0;
  if(filesystem::exists("dump.txt")) {
    while(filesystem::exists("dumps/dump"+to_string(idx)+".txt")) idx++;
    filesystem::copy_file("dump.txt","dumps/dump"+to_string(idx)+".txt");
  }
  dumpTree("dump.txt");
  return 0;
}