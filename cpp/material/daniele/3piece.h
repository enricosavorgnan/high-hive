#ifndef HIVE_CORE_PIECE_H
#define HIVE_CORE_PIECE_H

#include <cstdint>
#include <string_view>

namespace hive::core {

enum class Color : std::uint8_t { White, Black };

constexpr Color other(Color c) {
  return (c == Color::White) ? Color::Black : Color::White;
}

// Base set + espansioni
enum class Bug : std::uint8_t {
  Queen,
  Beetle,
  Spider,
  Grasshopper,
  Ant,
  Ladybug,
  Mosquito,
  Pillbug
};

struct Piece {
  Color color{Color::White};
  Bug bug{Bug::Queen};

  friend bool operator==(const Piece& a, const Piece& b) {
    return a.color == b.color && a.bug == b.bug;
  }
  friend bool operator!=(const Piece& a, const Piece& b) { return !(a == b); }
};

//Utility leggere per debug (non è ancora UHP)
constexpr std::string_view colorName(Color c) {
  return (c == Color::White) ? "White" : "Black";
}

constexpr std::string_view bugName(Bug b) {
  switch (b) {
    case Bug::Queen:       return "Queen";
    case Bug::Beetle:      return "Beetle";
    case Bug::Spider:      return "Spider";
    case Bug::Grasshopper: return "Grasshopper";
    case Bug::Ant:         return "Ant";
    case Bug::Ladybug:     return "Ladybug";
    case Bug::Mosquito:    return "Mosquito";
    case Bug::Pillbug:     return "Pillbug";
  }
  return "Unknown"; // per completezza
}

} // namespace hive::core

#endif
