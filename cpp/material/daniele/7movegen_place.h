#ifndef HIVE_CORE_MOVEGEN_PLACE_H
#define HIVE_CORE_MOVEGEN_PLACE_H

#include "2hexgrid.h"
#include "6state.h"

#include <unordered_set>
#include <vector>

namespace hive::core {

//controlla se in b la coord c ha almeno una cella col adiacente
inline bool touchesColor(const Board& b, const Coord& c, Color col) {
  for (const auto& n : hexgridNeighbors(c)) {
    auto t = b.top(n);
    if (t && t->color == col) return true;
  }
  return false;
}

//controlla se in b la coord c ha almeno una cella not_col adiacente
inline bool touchesOppColor(const Board& b, const Coord& c, Color col) {
  for (const auto& n : hexgridNeighbors(c)) {
    auto t = b.top(n);
    if (t && t->color != col) return true;
  }
  return false;
}

//prende uno stato di gioco s e restiuisce le cooridnate in cui è possibile piazzare un pezzo nuovo
inline std::vector<Coord> placementTargets(const State& s) {
  const Board& b = s.board();
  const Color player = s.toMove();

  std::vector<Coord> occ = b.occupiedCells();

  // Prima mossa della partita: fissiamo l'origine a (0,0) e nel caso piazziamo lì
  if (occ.empty()) {
    return {Coord{0,0}};
  }

  // Candidate = tutte le celle vuote adiacenti a QUALSIASI cella occupata, +8 per stare larghi
  std::unordered_set<Coord, CoordHash> cands;
  cands.reserve(6 * occ.size() + 8);
  
  //aggiungiamo tutti gli adiacenti liberi
  for (const auto& u : occ) {
    for (const auto& v : hexgridNeighbors(u)) {
      if (!b.occupied(v)) cands.insert(v);
    }
  }

  std::vector<Coord> out;
  out.reserve(cands.size());

  // Prima mossa del giocatore (es. il nero al secondo turno della partita):
  // l'unico vincolo è "essere adiacente all'alveare" (che già vale per costruzione dei cands).
  if (s.ply(player) == 0) {
    for (const auto& x : cands) out.push_back(x);
    return out;
  }

  // Piazzamenti standard: deve toccare il proprio colore e NON toccare l'avversario
  for (const auto& x : cands) {
    if (touchesColor(b, x, player) && !touchesOppColor(b, x, player)) {
      out.push_back(x);
    }
  }

  return out;
}

inline std::vector<Bug> placeableBugsThisTurn(const State& s) {
  const Color player = s.toMove();

  // Queen entro la 4a mossa del giocatore (ply==3 => stai per fare la tua 4a mossa)
  if (!s.queenPlaced(player) && s.ply(player) == 3) {
    if (s.hasInHand(player, Bug::Queen)) return {Bug::Queen};
    return {}; // non dovrebbe accadere se contatori sono corretti
  }

  std::vector<Bug> bugs;
  bugs.reserve(8);

  // Base + espansioni
  constexpr Bug all[] = {
    Bug::Queen, Bug::Beetle, Bug::Spider, Bug::Grasshopper, Bug::Ant,
    Bug::Ladybug, Bug::Mosquito, Bug::Pillbug
  };

  for (Bug b : all) {
    if (s.hasInHand(player, b)) bugs.push_back(b);
  }
  return bugs;
}

//prendo lo stato attuale della partita, restituisce tutti i possibili piazzamenti per il giocatore che muove
inline std::vector<Move> generatePlacements(const State& s) {
  const Color player = s.toMove();

  auto targets = placementTargets(s);
  auto bugs = placeableBugsThisTurn(s);

  std::vector<Move> moves;
  moves.reserve(targets.size() * bugs.size());

  for (const auto& c : targets) {
    for (Bug bug : bugs) {
      moves.push_back(Move::place(Piece{player, bug}, c));
    }
  }
  return moves;
}

} // namespace hive::core

#endif
