#ifndef HIVE_CORE_BOARD_H
#define HIVE_CORE_BOARD_H

#include "1coord.h"
#include "3piece.h"

#include <unordered_map>
#include <vector>
#include <optional>

namespace hive::core {

class Board {
public:
  //l'immagine evocativa è che moralmente è una scacchiera di stack di pezzi
  //il primo pezzo che abbandona una cella è quello posto più in alto
  //finchè quello più in alto non si sposta, quelli sotto non si spostano
  using Stack = std::vector<Piece>;

  bool occupied(const Coord& c) const {
    auto it = cells_.find(c);
    return it != cells_.end() && !it->second.empty();
  }

  int height(const Coord& c) const {
    auto it = cells_.find(c);
    return (it == cells_.end()) ? 0 : static_cast<int>(it->second.size());
  }

  std::optional<Piece> top(const Coord& c) const {
    auto it = cells_.find(c);
    if (it == cells_.end() || it->second.empty()) return std::nullopt;
    return it->second.back();
  }

  void push(const Coord& c, const Piece& p) {
    cells_[c].push_back(p);
  }

  Piece pop(const Coord& c) {
    auto& st = cells_.at(c);
    Piece p = st.back();
    st.pop_back();
    if (st.empty()) cells_.erase(c);
    return p;
  }

  void moveTop(const Coord& from, const Coord& to) {
    Piece p = pop(from);
    push(to, p);
  }

  std::vector<Coord> occupiedCells() const {
    std::vector<Coord> out;
    //ci serve che il vettore out delle celle abbia posto per esattamente tante coordinate quanti gli elementi di cells
    out.reserve(cells_.size());
    //ciclo for per iterare in maniera più semplice su tutte le componenti di cells, kv=key,value
    for (const auto& kv : cells_) out.push_back(kv.first);
    return out;
  }

private:
  std::unordered_map<Coord, Stack, CoordHash> cells_;
};

} // namespace hive::core

#endif
