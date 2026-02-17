#ifndef HIVE_CORE_COORD_H
#define HIVE_CORE_COORD_H

#include <cstdint>
#include <cstddef>

namespace hive::core {

struct Coord {
  /*la coppia di coordinate assiale, q per la colonna, r per la diagonale della griglia esagonale.
  
  dir0: ( +1, 0) spostamento attraverso lato destro,                                   >
  dir5: ( +1, -1) spostamento attraverso lato basso destro      2 / \ 1               /
  dir4: ( 0, -1) spostamento attraverso lato basso sinistro   3  |   | 0  q= ---> r= /
  dir3: ( -1, 0) spostamento attraverso lato sinistro          4  \ / 5
  dir2: ( -1, +1) spostamento attraverso lato alto sinistro
  dir1: ( 0, +1) spostamento attraverso lato alto destro
  */
  std::int32_t q = 0;
  std::int32_t r = 0;

  //operatori sulle coordinate, utili per utilizzare unordered set e altre
  friend bool operator==(const Coord& a, const Coord& b) {
    return a.q == b.q && a.r == b.r;
  }
  friend bool operator!=(const Coord& a, const Coord& b) { return !(a == b); }

  friend Coord operator+(const Coord& a, const Coord& b) {
    return {a.q + b.q, a.r + b.r};
  }
  friend Coord operator-(const Coord& a, const Coord& b) {
    return {a.q - b.q, a.r - b.r};
  }
};

struct CoordHash {
  std::size_t operator()(const Coord& c) const noexcept {
    // hash semplice e veloce per coppie di int, così coppie diverse hanno probabilmente hash diversi
    // (moltiplicatori diversi per mischiare altrimenti a,b e b,a si assomiglierebbero troppo)
    std::uint64_t x = static_cast<std::uint32_t>(c.q) * 73856093u;
    std::uint64_t y = static_cast<std::uint32_t>(c.r) * 19349663u;

    //XOR di x e y shiftando di 1 y verso sinistra
    return (std::size_t)(x ^ (y << 1));
  }
};

}

#endif
