#ifndef HIVE_CORE_HEXGRID_H
#define HIVE_CORE_HEXGRID_H

#include "1coord.h"
#include <array>
#include <cassert>
#include <utility>

namespace hive::core {

// 6 direzioni della griglia esagonale in coordinate assiali (q,r)
static constexpr std::array<Coord, 6> HEXGRID_DIRS = {
  Coord{+1,  0}, Coord{ 0, +1}, Coord{-1, +1}, Coord{ -1, 0},
  Coord{0,  -1}, Coord{+1, -1}
};


inline std::array<Coord, 6> hexgridNeighbors(const Coord& c) {
  return { c + HEXGRID_DIRS[0], c + HEXGRID_DIRS[1], c + HEXGRID_DIRS[2],
           c + HEXGRID_DIRS[3], c + HEXGRID_DIRS[4], c + HEXGRID_DIRS[5] };
}

// Se B è adiacente ad A ritorna l'indice direzione da A a B, altrimenti -1
inline int hexgridDirectionIndex(const Coord& A, const Coord& B) {
  Coord d = B - A;
  for (int i = 0; i < 6; ++i) {
    if (d == HEXGRID_DIRS[i]) return i;
  }
  return -1;
}

// Se A e B sono adiacenti, restituisce le due celle ai lati del corridoio A<->B
inline std::pair<Coord, Coord> hexgridCommonNeighborsAdjacent(const Coord& A, const Coord& B) {
  int d = hexgridDirectionIndex(A, B);
  assert(d != -1 && "A e B devono essere adiacenti");
  Coord left  = A + HEXGRID_DIRS[(d + 5) % 6];
  Coord right = A + HEXGRID_DIRS[(d + 1) % 6];
  return {left, right};
}

} // namespace hive::core

#endif
