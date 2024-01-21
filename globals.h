#pragma once

#include <cstdint>
#include <iostream>
#include <vector>

#define sz(x) ((int)(x.size()))

#ifdef nohomestuck
class nohomestuckstream {
public:
  std::ostream &x;
  nohomestuckstream(std::ostream &x) : x(x) {}
  template <typename T> const nohomestuckstream &operator<<(const T &v) const {
    x << v;
    return *this;
  }
  const nohomestuckstream &operator<<(const char *s) const {
    for (char c : std::string(s))
      x << ((c == '4' ? 'A' : (c == '1' ? 'I' : (c == '3' ? 'E' : c))));
    x.flush();
    return *this;
  }
} nohomestuckcout(std::cout);
#define cout nohomestuckcout
#define endl "\n"
#else
#define cout std::cout
#define endl std::endl
#endif

int th, nodelim = -1, deplim = 1000, l4h;
int p, width, sym;
int maxwid, stator, bthh;
int qSize = 0;

uint64_t enforce, remember, enforce2, remember2;
uint64_t overlap = remember & remember2;

std::vector<uint64_t> filters;