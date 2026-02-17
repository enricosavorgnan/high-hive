#ifndef HIVE_CORE_MOVE_H
#define HIVE_CORE_MOVE_H

#include "1coord.h"
#include "3piece.h"
#include <optional>

namespace hive::core {

enum class MoveKind : std::uint8_t { Place, Move, Resign };

struct Move {
  MoveKind kind{MoveKind::Resign};

  // Place: piece + to
  // Move : from + to (il pezzo è implicito: top(from)), quindi non andrebbe messo
  std::optional<Piece> piece;
  std::optional<Coord> from;
  std::optional<Coord> to;

  static Move resign() { return Move{}; }

  static Move place(Piece p, Coord dest) {
    Move m;
    m.kind = MoveKind::Place;
    m.piece = p;
    m.to = dest;
    return m;
  }

  static Move move(Coord src, Coord dest) {
    Move m;
    m.kind = MoveKind::Move;
    m.from = src;
    m.to = dest;
    return m;
  }
};

} // namespace hive::core

#endif
