#pragma once

#include <cstdint>
#include <iostream>
#include <vector>
#include <ranges>
#include <cassert>
#include <type_traits>

#define sz(x) ((int)(x.size()))

class ColorStream {
private:
  std::ostream& stream;
  std::string color;
public:
  ColorStream(std::ostream& out, std::string color) : stream(out), color(color) {

  }
  void setColor(std::string newColor) { color = newColor; }
  template <typename T>
  const ColorStream& operator<<(const T& data) const {
    if(color.size())
      stream << "\033[" << color << "m" << data << "\033[0m";
    else
      stream << data;
    return *this;
  }
} DEBUG(std::cerr, "37"), STATUS(std::cerr, "32"), INFO(std::cerr, "1;1;36"), WARN(std::cerr, "1;1;31");

int th, deplim = 1000, l4h;
int p, width, sym;
int maxwid, stator, bthh;
int qSize = 0;

uint64_t enforce, remember, enforce2, remember2;
uint64_t overlap = remember & remember2;

int overlap_ctz;

#define CENTER_ID(row) ((uint8_t)(((row) >> overlap_ctz) & 3))
#define SIDE0_ID(row) ((row) & remember)
#define SIDE1_ID(row) ((row) & remember2)

std::map<int, int> partialLoadNewNodeid;
int partialLoadOriginalNodeid[200]; // 2 * p
int wu_onx;

std::vector<uint64_t> filters;
std::vector<uint64_t> leftborder[2];

std::vector<uint64_t> arithmeticDecode(const std::vector<uint64_t>& dwords, const std::vector<uint64_t>& maxvals) {
  std::vector<uint64_t> vals;
  auto it = dwords.rbegin();
  __uint128_t state = *it++;
  for (auto maxval : maxvals | std::views::reverse) {
    if (maxval > ((__uint128_t(1)) << 64))
      throw std::runtime_error("Max value " + std::to_string(maxval) + " is too large");
    if (it != dwords.rend() && (state << 64) + *it < (((__uint128_t)maxval) << 64)) {
      state = (state << 64) + *it;
      it++;
    }
    vals.push_back(state % maxval);
    state /= maxval;
  }
  std::reverse(vals.begin(), vals.end());
  return vals;
}

std::vector<uint64_t> arithmeticEncode(const std::vector<uint64_t>& vals, const std::vector<uint64_t>& maxvals) {
  __uint128_t state = 0;
  std::vector<uint64_t> dwords;
  assert(maxvals.size() == vals.size());
  for(int i=0; i<(int)vals.size(); i++) {
    uint64_t val = vals[i], maxval = maxvals[i];
    if(val >= maxval)
      throw std::runtime_error("Value " + std::to_string(val) + " is not in [0, " + std::to_string(maxval) + ")");
    if(maxval > ((__uint128_t(1)) << 64))
      throw std::runtime_error("Max value " + std::to_string(maxval) + " is too large");
    state = state * maxval + val;
    if(state >= ((__uint128_t(1)) << 64)) {
      dwords.push_back(state & 0xffffffffffffffff);
      state >>= 64;
    }
  }
  dwords.push_back(state);
  // assert(arithmeticDecode(dwords, maxvals) == vals);
  return dwords;
}

template<typename T>
void writeInt(T x, std::ofstream& f) {
  char y;
  while(x >= 128) {
    y = (x & 127) | 128;
    x >>= 7;
    f.write(&y, 1);
  }
  y = x;
  f.write(&y, 1);
}

template<typename T>
void readInt(T& x, std::ifstream& f) {
  x = 0;
  char y = -128;
  int s = 0;
  while(y & 128) {
    f.read(&y, 1);
    x |= ((uint64_t)((unsigned char)(y & 127))) << s;
    s += 7;
  }
}

void arithWriteToStream(const std::vector<uint64_t>& vals, const std::vector<uint64_t>& maxvals, std::ofstream& f) {
  assert(f.binary);
  std::vector<uint64_t> encoded = arithmeticEncode(vals, maxvals);
  int cnt = sz(encoded);
  std::vector<char> asBytes((cnt - 1) * 8);
  for(int i=0; i < cnt-1; i++)
    for(int j=0; j<8; j++)
      asBytes[i*8+j] = (encoded[i] >> (j*8)) & 255;
  uint64_t x = encoded[cnt-1];
  if(x) {
    while(x) {
      asBytes.push_back(x & 255);
      x >>= 8;
    }
  } else
    asBytes.push_back(0);
  int v = sz(asBytes);
  assert(v >= 1);
  writeInt(v, f);
  f.write(asBytes.data(), v);
}

void arithReadFromStream(std::vector<uint64_t>& vals, const std::vector<uint64_t>& maxvals, std::ifstream& f) {
  assert(f.binary);
  int v; readInt(v, f);
  // WARN << v << '\n';
  assert(v >= 1);
  std::vector<char> asBytes(v);
  f.read(asBytes.data(), v);
  std::vector<uint64_t> encoded((v + 7)/8);
  for(int i=0; i<v; i++)
    encoded[i / 8] |= (((uint64_t)((unsigned char)(asBytes[i]))) << ((i % 8) * 8));
  vals = arithmeticDecode(encoded, maxvals);
}