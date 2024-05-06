#include <bits/stdc++.h>
#include "cadical/src/cadical.hpp"

#include "debug.h"
#include "globals.h"
#include "logic.h"
#include "searchtree.h"
using namespace std;

int main(int argc, char* argv[]) {
  auto t1 = chrono::high_resolution_clock::now();
  assert(argc == 3);
  ifstream fin(argv[1], std::ios::binary);
  loadf(fin, true);
  // int l = stoi(argv[3]), r = stoi(argv[4]);
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
  dumpf(fx);
  fx.close();
  return 0;
}