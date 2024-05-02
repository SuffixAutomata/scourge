#pragma once

#include <cstdint>
#include <iostream>
#include <vector>

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

std::vector<uint64_t> filters;