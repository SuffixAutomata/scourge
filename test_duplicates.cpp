// TODO: why do nodes swallow & forget about their work units?

#include <bits/stdc++.h>

int worker_ping_duration = 8000; // ms

int p, width, sym, l4h;
int maxwid, stator;

namespace _searchtree {
std::mutex searchtree_mutex;
// state: d - done, q - not done, 2 - duplicate
// TODO: for non-ephemeral nodes, include state s - sent to NE node
struct node { uint64_t row; int depth, shift, parent, contrib; char state; };
node* tree = new node[16777216];
int treeSize = 0, treeAlloc = 16777216;
std::vector<uint64_t> exInitrow;
std::vector<uint64_t> filters;
std::vector<uint64_t> leftborder[2];
std::vector<std::string> contributors; std::map<std::string, int> contributorIDs;

void dumpTree(std::string fn) {
  std::ofstream fx(fn);
  fx << p << ' ' << width << ' ' << sym << ' ' << stator << ' ' << l4h << '\n';
  fx << exInitrow.size(); for(auto s:exInitrow){fx << ' ' << s;} fx << '\n';
  fx << filters.size(); for(auto s:filters){fx << ' ' << s;} fx << '\n';
  for(int t=0;t<2;t++){ fx<<leftborder[t].size(); for(auto s:leftborder[t]){fx<<' '<<s;}fx<<'\n';}
  fx<<treeSize<<'\n';
  for(int i=0;i<treeSize;i++)
    fx<<tree[i].row<<' '<<tree[i].shift<<' '<<tree[i].parent<<' '<<tree[i].contrib<<' '<<tree[i].state<<'\n';
  fx<<contributorIDs.size()<<'\n';
  for(auto s:contributors) fx<<s<<'\n';
  fx.flush(); fx.close();
}

// void flushTree(node* dest = tree) { assert(0); }

int newNode() {
  assert(treeSize < treeAlloc);
  // if(treeSize == treeAlloc)
  //   treeAlloc *= 2, flushTree(new node[treeAlloc]);
  // if((treeSize - time(0)) % 8192 == 0) flushTree();
  tree[treeSize] = {0,0,0,0,0,'0'};
  // std::cerr << "called newnode " << treeSize << '\n';
  return treeSize++;
}

std::vector<uint64_t> getState(int onx) {
  std::vector<uint64_t> mat;
  while(onx != -1)
    mat.push_back(tree[onx].row), onx = tree[onx].parent;
  std::reverse(mat.begin(), mat.end());
  return std::vector<uint64_t>(mat.end() - 2*p, mat.end());
}

uint64_t calculateHash(int onx) {
  uint64_t v = 0;
  for(int i=0; i<2*p; i++) {
    v = 3 * v + tree[onx].row;
    onx = tree[onx].parent;
  }
  return v;
}

std::unordered_multimap<uint64_t, int> hasht;

bool checkduplicate(int onx) {
  if(onx < 2*p) return 0;
  uint64_t targHash = calculateHash(onx);
  auto [st, en] = hasht.equal_range(targHash);
  bool found = false;
  std::vector<uint64_t> targState;
  for(auto it=st; it!=en; it++) {
    if(tree[it->second].depth != tree[onx].depth) continue;
    if(!targState.size()) targState = getState(onx);
    if(getState(it->second) == targState) {
      found = true;
      break;
    }
  }
  if(!found) hasht.emplace(targHash, onx);
  return found;
}

void loadTree(std::string fn) {
  using namespace std;
  ifstream fx(fn);
  fx>>p>>width>>sym>>stator>>l4h;
  int FS; fx>>FS; exInitrow=std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)fx>>exInitrow[i];
  fx>>FS; filters=std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)fx>>filters[i];
  for(int pp=0;pp<2;pp++){fx>>FS;leftborder[pp]=std::vector<uint64_t>(FS);
  for(int t=0;t<FS;t++)fx>>leftborder[pp][t];}
  fx>>treeSize;
  for(int i=0;i<treeSize;i++){
    fx>>tree[i].row>>tree[i].shift>>tree[i].parent>>tree[i].contrib>>tree[i].state;
    tree[i].depth = 1 + (i ? tree[tree[i].parent].depth : 0);
    if(i >= 2*p && (tree[tree[i].parent].state == '2' || checkduplicate(i)))
      tree[i].state = '2';
  }
  fx>>FS;
  contributors=std::vector<std::string>(FS);
  for(int i=0; i<FS;i++) {
    fx>>contributors[i];
    contributorIDs[contributors[i]] = i;
  }
}

int getWidth(int i) {
  uint64_t bito = 0;
  for(uint64_t x:getState(i)){ bito |= x; }
  return 64-__builtin_clzll(bito)-__builtin_ctzll(bito);
}
}; using namespace _searchtree;

int main(int argc, const char* argv[]) {
  std::cerr << "tree name: ";
  std::string fn; std::cin >> fn;
  loadTree(fn);
  int dup = 0;
  for(int i=0; i<treeSize; i++) if(tree[i].state == '2') dup++;
  std::cerr << dup << ' ' << hasht.size() << ' ' << treeSize << '\n';
  std::set<std::vector<uint64_t>> pp;
  for(int i=2*p; i<treeSize; i++) pp.insert(getState(i));
  std::cerr << pp.size() << '\n';
  return 0;
}