#include <vector>
#include <fstream>
#include <cstdint>
#include <set>

int p, width, sym, l4h;
int maxwid, stator;
std::vector<uint64_t> exInitrow;
std::vector<uint64_t> filters;
std::vector<uint64_t> leftborder[2];

#include "logic.h"
#include "satlogic.h"

int main(int argc, char* argv[]) {
  // args: host id/name timeout threads
  if(argc != 3) {
    std::cerr << "usage: " << argv[0] << " input-file output-file\n";
    return 1;
  }
  std::ifstream ix(argv[1]);
  std::ofstream oup(argv[2]);
  ix >> p >> width >> sym >> l4h >> maxwid >> stator;
  int FS; ix>>FS; exInitrow = std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)ix>>exInitrow[i];
  ix >> FS;
  filters = std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)ix>>filters[i];
  for(int s=0;s<2;s++){ix>>FS;leftborder[s]=std::vector<uint64_t>(FS);
  for(int i=0;i<FS;i++)ix>>leftborder[s][i];}
  int cnt;
  ix >> cnt;
  for(int i=0; i<cnt; i++) {
    int id, depth; ix >> id >> depth;
    std::vector<uint64_t> rows(2*p); for(auto& s:rows)ix>>s;
    std::vector<uint64_t> resp;
    std::cerr << id << ' ' << depth << '\n';
    genNextRows(rows, depth, l4h, [&](uint64_t x){ 
      resp.push_back(x);
    });
    oup << id << ' ' << resp.size();
    for(auto t:resp) oup << ' ' << t;
    oup << '\n';
  }
  return 0;
}