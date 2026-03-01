#ifndef HIVE_CORE_MOVEGEN_MOVE_H
#define HIVE_CORE_MOVEGEN_MOVE_H

#include "2hexgrid.h"
#include "6state.h"
#include "8onehive_art.h"

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

namespace hive::core {

// -----------------------------
// Helper: "stato dopo il lift"
// -----------------------------
// Per generare mosse di un pezzo che si sposta, è comodo ragionare come se il pezzo fosse già
// stato sollevato dalla sua cella di origine `liftedFrom`. In particolare:
// - se height(liftedFrom)==1 allora quella cella diventa vuota
// - se height(liftedFrom)>=2 allora resta occupata, ma con altezza -1

inline int HeightAfterLift(const Board& b, const Coord& liftedFrom, const Coord& c) {
  if (c == liftedFrom) {
    const int h = b.height(c);
    return (h <= 0) ? 0 : (h - 1);
  }
  return b.height(c);
}

inline bool OccupiedAfterLift(const Board& b, const Coord& liftedFrom, const Coord& c) {
  return HeightAfterLift(b, liftedFrom, c) > 0;
}

inline bool EmptyAfterLift(const Board& b, const Coord& liftedFrom, const Coord& c) {
  return !OccupiedAfterLift(b, liftedFrom, c);
}

inline bool HasOccupiedNeighborAfterLift(const Board& b, const Coord& liftedFrom, const Coord& c) {
  for (const auto& nb : hexgridNeighbors(c)) {
    if (OccupiedAfterLift(b, liftedFrom, nb)) return true;
  }
  return false;
}

// -------------------------------------------------
// Regole di scorrimento (freedom to move) in 1 passo
// -------------------------------------------------
// CanSlideOneStep:
// - src e dst adiacenti
// - dst deve essere vuota (dopo lift)
// - il corridoio non deve essere "chiuso" (non entrambi i laterali occupati)
// - dst deve restare a contatto con l'alveare (almeno un vicino occupato dopo lift)
inline bool CanSlideOneStep(const Board& b,
                           const Coord& liftedFrom,
                           const Coord& src,
                           const Coord& dst) {
  if (!EmptyAfterLift(b, liftedFrom, dst)) return false;

  auto lr = hexgridCommonNeighborsAdjacent(src, dst);
  const bool occL = OccupiedAfterLift(b, liftedFrom, lr.first);
  const bool occR = OccupiedAfterLift(b, liftedFrom, lr.second);
  if (occL && occR) return false;

  // evita di "scappare" nel vuoto infinito: la destinazione deve toccare l'alveare
  if (!HasOccupiedNeighborAfterLift(b, liftedFrom, dst)) return false;

  return true;
}

// CanSlideOneStepOverHive:
// Variante per chi si muove "sopra" l'alveare (coleottero, coccinella).
// L'idea: non puoi passare attraverso una strettoia se entrambi i due stack laterali
// sono almeno alti quanto il livello su cui ti trovi.
// - livello = altezza ("z") del pezzo che sta scorrendo in quel momento
// Nota: qui NON richiediamo dst vuota. Serve anche per salire su stack.
inline bool CanSlideOneStepOverHive(const Board& b,
                                   const Coord& liftedFrom,
                                   const Coord& src,
                                   const Coord& dst,
                                   int livello) {
  auto lr = hexgridCommonNeighborsAdjacent(src, dst);
  const int hL = HeightAfterLift(b, liftedFrom, lr.first);
  const int hR = HeightAfterLift(b, liftedFrom, lr.second);
  if (hL >= livello && hR >= livello) return false;
  return true;
}

// ---------------------------------
// Movimento di un singolo pezzo (API)
// ---------------------------------

inline void SingleQueenMoves(const Board& b, const Coord& from, std::vector<Move>& out) {
  for (const auto& nb : hexgridNeighbors(from)) {
    if (CanSlideOneStep(b, from, from, nb)) out.push_back(Move::move(from, nb));
  }
}

inline void SingleBeetleMoves(const Board& b, const Coord& from, std::vector<Move>& out) {
  const int hFrom = b.height(from);
  const int livello = hFrom; // il coleottero sta al livello pari all'altezza dello stack che lo contiene

  for (const auto& nb : hexgridNeighbors(from)) {
    if (hFrom >= 2) {
      // Se è sopra l'alveare, può andare su qualunque adiacente (vuoto o occupato),
      // ma non può infilarsi in strettoie a questo livello.
      if (CanSlideOneStepOverHive(b, from, from, nb, livello)) {
        out.push_back(Move::move(from, nb));
      }
    } else {
      // A terra: se va su vuoto usa CanSlideOneStep (con vincolo di contatto).
      // Se sale su occupato, basta la freedom-to-move a livello 1 (non entrambi i laterali occupati).
      if (b.occupied(nb)) {
        if (CanSlideOneStepOverHive(b, from, from, nb, /*livello=*/1)) {
          out.push_back(Move::move(from, nb));
        }
      } else {
        if (CanSlideOneStep(b, from, from, nb)) {
          out.push_back(Move::move(from, nb));
        }
      }
    }
  }
}

inline void SingleGrasshopperMoves(const Board& b, const Coord& from, std::vector<Move>& out) {
  for (int d = 0; d < 6; ++d) {
    Coord cur = from + HEXGRID_DIRS[d];
    if (!OccupiedAfterLift(b, from, cur)) continue; // deve saltare almeno un pezzo

    while (OccupiedAfterLift(b, from, cur)) {
      cur = cur + HEXGRID_DIRS[d];
    }
    out.push_back(Move::move(from, cur));
  }
}

inline void SingleAntMoves(const Board& b, const Coord& from, std::vector<Move>& out) {
  // DFS/BFS sulle celle vuote raggiungibili, applicando CanSlideOneStep ad ogni arco.
  std::unordered_set<Coord, CoordHash> visited;
  visited.reserve(128);
  visited.insert(from);

  std::vector<Coord> st;
  st.reserve(128);
  st.push_back(from);

  while (!st.empty()) {
    Coord u = st.back();
    st.pop_back();

    for (const auto& v : hexgridNeighbors(u)) {
      if (!EmptyAfterLift(b, from, v)) continue;
      if (visited.find(v) != visited.end()) continue;
      if (!CanSlideOneStep(b, from, u, v)) continue;

      visited.insert(v);
      st.push_back(v);
      if (v != from) out.push_back(Move::move(from, v));
    }
  }
}

inline void AllAntsMovements(const Board& b, const std::vector<Coord>& antLikeFroms, std::vector<Move>& out) {
  // Versione semplice (corretta): per ora chiamiamo SingleAntMoves per ognuna.
  // In seguito si può ottimizzare (es. precomputando strutture sul bordo).
  for (const Coord& from : antLikeFroms) {
    SingleAntMoves(b, from, out);
  }
}

inline void SingleSpiderMoves(const Board& b, const Coord& from, std::vector<Move>& out) {
  // Esattamente 3 passi di crawling, senza tornare su celle già percorse.
  // Implementazione volutamente semplice: triplo ciclo sui vicini.
  std::unordered_set<Coord, CoordHash> dests;
  dests.reserve(64);

  for (const auto& a : hexgridNeighbors(from)) {
    if (!CanSlideOneStep(b, from, from, a)) continue;

    for (const auto& b2 : hexgridNeighbors(a)) {
      if (b2 == from) continue;
      if (!CanSlideOneStep(b, from, a, b2)) continue;

      for (const auto& c : hexgridNeighbors(b2)) {
        if (c == from) continue;
        if (c == a) continue;
        if (c == b2) continue;
        if (!CanSlideOneStep(b, from, b2, c)) continue;

        dests.insert(c);
      }
    }
  }

  for (const auto& d : dests) {
    out.push_back(Move::move(from, d));
  }
}

inline void SingleLadybugMoves(const Board& b, const Coord& from, std::vector<Move>& out) {
  // Coccinella: 3 passi: 2 sopra l'alveare (su occupato), 1 giù su vuoto.
  // Applichiamo CanSlideOneStepOverHive per evitare strettoie troppo alte.
  std::unordered_set<Coord, CoordHash> dests;
  dests.reserve(128);

  for (const auto& a : hexgridNeighbors(from)) {
    if (!OccupiedAfterLift(b, from, a)) continue; // passo 1 su occupato
    {
      const int livello = HeightAfterLift(b, from, from) + 1;
      if (!CanSlideOneStepOverHive(b, from, from, a, livello)) continue;
    }

    for (const auto& b2 : hexgridNeighbors(a)) {
      if (!OccupiedAfterLift(b, from, b2)) continue; // passo 2 su occupato
      {
        const int livello = HeightAfterLift(b, from, a) + 1;
        if (!CanSlideOneStepOverHive(b, from, a, b2, livello)) continue;
      }

      for (const auto& c : hexgridNeighbors(b2)) {
        if (!EmptyAfterLift(b, from, c)) continue; // passo 3 su vuoto
        {
          const int livello = HeightAfterLift(b, from, b2) + 1;
          if (!CanSlideOneStepOverHive(b, from, b2, c, livello)) continue;
        }
        // la coccinella deve atterrare a contatto dell'alveare (in pratica è sempre vero,
        // ma lo controlliamo per simmetria)
        if (!HasOccupiedNeighborAfterLift(b, from, c)) continue;

        dests.insert(c);
      }
    }
  }

  for (const auto& d : dests) {
    out.push_back(Move::move(from, d));
  }
}

// --- Pillbug: movimento base + abilità Drag ---

inline void AppendPillbugDragMoves(const Board& b,
                                  const std::unordered_set<Coord, CoordHash>& articulation,
                                  const std::optional<Coord>& lastMovedTo,
                                  const Coord& actingPillbug,
                                  std::vector<Move>& out) {
  // Abilità: sposta (Drag) un pezzo adiacente (non impilato) in una cella adiacente vuota,
  // entrambe adiacenti al pillbug.
  // Vincoli che possiamo verificare qui:
  // - pezzo sorgente deve avere height==1
  // - non deve essere il pezzo mosso nel turno precedente
  // - sollevare il pezzo non deve spezzare l'alveare (one-hive)

  const auto adjs = hexgridNeighbors(actingPillbug);

  // Candidati sorgente (pezzi da trascinare)
  for (const auto& src : adjs) {
    if (!b.occupied(src)) continue;
    if (src == actingPillbug) continue;
    if (b.height(src) != 1) continue;
    if (lastMovedTo && *lastMovedTo == src) continue;
    if (!oneHiveAllowsLiftFrom(b, src, articulation)) continue;

    // Candidati destinazione (vuoti adiacenti al pillbug)
    for (const auto& dst : adjs) {
      if (dst == src) continue;
      if (b.occupied(dst)) continue;
      // deve restare attaccato all'alveare (di sicuro tocca il pillbug)
      out.push_back(Move::drag(src, actingPillbug, dst));
    }
  }
}

inline void SinglePillbugMoves(const Board& b,
                              const std::unordered_set<Coord, CoordHash>& articulation,
                              const std::optional<Coord>& lastMovedTo,
                              const Coord& from,
                              std::vector<Move>& out) {
  // Movimento base = come la regina
  SingleQueenMoves(b, from, out);
  // Abilità Drag
  AppendPillbugDragMoves(b, articulation, lastMovedTo, from, out);
}

// ----------------
// Mosquito (API)
// ----------------

inline bool MosquitoCopiesAnt(const Board& b, const Coord& from) {
  // Se è impilato (height>=2) copia sempre il coleottero, quindi NON è "formica".
  if (b.height(from) >= 2) return false;
  for (const auto& nb : hexgridNeighbors(from)) {
    auto t = b.top(nb);
    if (!t) continue;
    if (t->bug == Bug::Ant) return true;
  }
  return false;
}

inline void SingleMosquitoNotAnt(const Board& b,
                                const std::unordered_set<Coord, CoordHash>& articulation,
                                const std::optional<Coord>& lastMovedTo,
                                const Coord& from,
                                std::vector<Move>& out) {
  // Regola base: copia le mosse dei pezzi adiacenti (esclusi mosquito).
  // Se è impilato (height>=2) copia sempre il coleottero.
  if (b.height(from) >= 2) {
    SingleBeetleMoves(b, from, out);
    return;
  }

  std::array<char, bugCount()> can{};
  bool hasAny = false;

  for (const auto& nb : hexgridNeighbors(from)) {
    auto t = b.top(nb);
    if (!t) continue;
    if (t->bug == Bug::Mosquito) continue;
    hasAny = true;
    if (t->bug == Bug::Ant) continue; // la parte "formica" la gestiamo fuori
    can[bugIndex(t->bug)] = 1;
  }

  if (!hasAny) return;

  if (can[bugIndex(Bug::Queen)])       SingleQueenMoves(b, from, out);
  if (can[bugIndex(Bug::Beetle)])      SingleBeetleMoves(b, from, out);
  if (can[bugIndex(Bug::Spider)])      SingleSpiderMoves(b, from, out);
  if (can[bugIndex(Bug::Grasshopper)]) SingleGrasshopperMoves(b, from, out);
  if (can[bugIndex(Bug::Ladybug)])     SingleLadybugMoves(b, from, out);
  if (can[bugIndex(Bug::Pillbug)])     SinglePillbugMoves(b, articulation, lastMovedTo, from, out);
}

inline void SingleMosquitoMoves(const Board& b,
                               const std::unordered_set<Coord, CoordHash>& articulation,
                               const std::optional<Coord>& lastMovedTo,
                               const Coord& from,
                               std::vector<Move>& out) {
  SingleMosquitoNotAnt(b, articulation, lastMovedTo, from, out);
  if (MosquitoCopiesAnt(b, from)) {
    SingleAntMoves(b, from, out);
  }
}

// ---------------------------------
// Generatore principale (solo movimenti)
// ---------------------------------

// Chiave minima per deduplicare mosse (Move / Drag)
struct MoveKey {
  MoveKind kind{MoveKind::Resign};
  Coord from{0, 0};
  Coord to{0, 0};
  Coord pillbug{0, 0};
  bool hasPillbug{false};

  friend bool operator==(const MoveKey& a, const MoveKey& b) {
    if (a.kind != b.kind) return false;
    if (a.from != b.from) return false;
    if (a.to != b.to) return false;
    if (a.hasPillbug != b.hasPillbug) return false;
    if (a.hasPillbug && a.pillbug != b.pillbug) return false;
    return true;
  }
};

struct MoveKeyHash {
  std::size_t operator()(const MoveKey& k) const noexcept {
    std::size_t h = static_cast<std::size_t>(k.kind);
    CoordHash ch;
    h ^= (ch(k.from) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
    h ^= (ch(k.to)   + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
    if (k.hasPillbug) {
      h ^= (ch(k.pillbug) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
    }
    return h;
  }
};

inline MoveKey toKey(const Move& m) {
  MoveKey k;
  k.kind = m.kind;
  if (m.from) k.from = *m.from;
  if (m.to) k.to = *m.to;
  if (m.pillbug) {
    k.hasPillbug = true;
    k.pillbug = *m.pillbug;
  }
  return k;
}

inline std::vector<Move> generateMovements(const State& s) {
  const Color player = s.toMove();

  // Regola classica: finché la tua regina non è piazzata, puoi solo piazzare (quindi qui 0 movimenti).
  if (!s.queenPlaced(player)) return {};

  const Board& b = s.board();
  const auto articulation = oneHiveArticulationPoints(b);
  const auto& lastMovedTo = s.lastMovedTo();

  std::vector<Move> raw;
  raw.reserve(256);

  // Raccolta per batch delle mosse "formica" (formiche vere + mosquito che copia formica)
  std::vector<Coord> antLike;
  antLike.reserve(16);
  std::unordered_set<Coord, CoordHash> antLikeSet;
  antLikeSet.reserve(32);

  for (const Coord& from : b.occupiedCells()) {
    auto t = b.top(from);
    if (!t) continue;
    if (t->color != player) continue;

    // se sollevare spezza l'alveare, non può muoversi
    if (!oneHiveAllowsLiftFrom(b, from, articulation)) continue;

    switch (t->bug) {
      case Bug::Queen:
        SingleQueenMoves(b, from, raw);
        break;
      case Bug::Beetle:
        SingleBeetleMoves(b, from, raw);
        break;
      case Bug::Spider:
        SingleSpiderMoves(b, from, raw);
        break;
      case Bug::Grasshopper:
        SingleGrasshopperMoves(b, from, raw);
        break;
      case Bug::Ant:
        if (antLikeSet.insert(from).second) antLike.push_back(from);
        break;
      case Bug::Ladybug:
        SingleLadybugMoves(b, from, raw);
        break;
      case Bug::Pillbug:
        SinglePillbugMoves(b, articulation, lastMovedTo, from, raw);
        break;
      case Bug::Mosquito: {
        // parte non-formica
        SingleMosquitoNotAnt(b, articulation, lastMovedTo, from, raw);
        // se copia la formica, aggiungilo al batch ant
        if (MosquitoCopiesAnt(b, from)) {
          if (antLikeSet.insert(from).second) antLike.push_back(from);
        }
        break;
      }
    }
  }

  // batch "formica" (formiche vere + mosquito che copia formica)
  if (!antLike.empty()) {
    AllAntsMovements(b, antLike, raw);
  }

  // dedup finale (utile soprattutto per mosquito/pillbug)
  std::unordered_set<MoveKey, MoveKeyHash> seen;
  seen.reserve(raw.size() * 2 + 16);

  std::vector<Move> out;
  out.reserve(raw.size());
  for (const auto& m : raw) {
    MoveKey k = toKey(m);
    if (seen.insert(k).second) out.push_back(m);
  }
  return out;
}

} // namespace hive::core

#endif
