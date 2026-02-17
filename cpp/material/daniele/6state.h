#ifndef HIVE_CORE_STATE_H
#define HIVE_CORE_STATE_H

#include "3piece.h"
#include "4board.h"
#include "5move.h"


#include <array>
#include <cstdint>
#include <optional>

namespace hive::core {

// --- helper: index per array ---
constexpr int colorIndex(Color c) { return (c == Color::White) ? 0 : 1; }

constexpr int bugCount() { return 8; } // Queen, Beetle, Spider, Grasshopper, Ant, Ladybug, Mosquito, Pillbug

constexpr int bugIndex(Bug b) {
  switch (b) {
    case Bug::Queen:       return 0;
    case Bug::Beetle:      return 1;
    case Bug::Spider:      return 2;
    case Bug::Grasshopper: return 3;
    case Bug::Ant:         return 4;
    case Bug::Ladybug:     return 5;
    case Bug::Mosquito:    return 6;
    case Bug::Pillbug:     return 7;
  }
  return -1; // unreachable in pratica
}

using HandCounts = std::array<int, bugCount()>;
using BothHands  = std::array<HandCounts, 2>; // [color][bug]

// Conteggi standard Hive + espansioni (Ladybug, Mosquito, Pillbug)
constexpr HandCounts standardHand() {
  //Queen 1, Beetle 2, Spider 2, Grasshopper 3, Ant 3, Ladybug 1, Mosquito 1, Pillbug 1
  return HandCounts{1, 2, 2, 3, 3, 1, 1, 1};
}

//serve per memorizzrae tutte le informazioni utili per annullare una mossa nello sviluppo dell'albero
struct Undo {
  Move move;                      //la mossa applicata
  Color prevToMove{Color::White}; //turno precedente
  int prevPlyWhite{0};
  int prevPlyBlack{0};
  bool prevWhiteQueenPlaced{false};
  bool prevBlackQueenPlaced{false};

  //per Place: quale pezzo era stato piazzato (così ripristini il contatore)
  std::optional<Piece> placedPiece;
};

class State {
public:
  State() {
    hands_[0] = standardHand(); // White
    hands_[1] = standardHand(); // Black
  }

  //accesso base
  const Board& board() const { return board_; }
  Board& board() { return board_; }

  Color toMove() const { return toMove_; }

  int ply(Color c) const { return (c == Color::White) ? plyWhite_ : plyBlack_; }

  bool queenPlaced(Color c) const { return (c == Color::White) ? whiteQueenPlaced_ : blackQueenPlaced_; }

  int remaining(Color c, Bug b) const { return hands_[colorIndex(c)][bugIndex(b)]; }

  bool hasInHand(Color c, Bug b) const { return remaining(c, b) > 0; }

  // prende in input una mossa, assumendo che sia valida
  // ritorna un Undo, ossia le informazioni per poterla annullare
  Undo apply(const Move& m) {
    Undo u;
    //salva tutte le info prima di applicarla
    u.move = m;
    u.prevToMove = toMove_;
    u.prevPlyWhite = plyWhite_;
    u.prevPlyBlack = plyBlack_;
    u.prevWhiteQueenPlaced = whiteQueenPlaced_;
    u.prevBlackQueenPlaced = blackQueenPlaced_;

    //dopo aver salvato, la applica
    if (m.kind == MoveKind::Resign) {
      //niente da fare
    } 
    else if (m.kind == MoveKind::Place) {
      // richiede: m.piece e m.to presenti in m
      const Piece p = m.piece.value();
      const Coord dest = m.to.value();

      // aggiorna riserva
      hands_[colorIndex(p.color)][bugIndex(p.bug)] -= 1;
      u.placedPiece = p;

      // piazza sul board
      board_.push(dest, p);

      // aggiorna "queen placed"
      if (p.bug == Bug::Queen) {
        if (p.color == Color::White) whiteQueenPlaced_ = true;
        else blackQueenPlaced_ = true;
      }
    } 
    else {// MoveKind::Move
      const Coord src  = m.from.value();
      const Coord dest = m.to.value();
      board_.moveTop(src, dest);
    }

    // aggiorna ply e turno
    if (toMove_ == Color::White) ++plyWhite_;
    else ++plyBlack_;
    toMove_ = other(toMove_);

    return u;
  }

  //Annulla una mossa applicata con apply(), non ritorna nulla
  void undo(const Undo& u) {
    //ripristina turno e contatori
    toMove_ = u.prevToMove;
    plyWhite_ = u.prevPlyWhite;
    plyBlack_ = u.prevPlyBlack;
    whiteQueenPlaced_ = u.prevWhiteQueenPlaced;
    blackQueenPlaced_ = u.prevBlackQueenPlaced;

    const Move& m = u.move;

    if (m.kind == MoveKind::Resign) {
      return;
    } 
    else if (m.kind == MoveKind::Place) {
      const Coord dest = m.to.value();
      //togli il pezzo piazzato. Nota: .pop(dest) toglie dest e ritorna cosa c'era in quella pos
      //mettere (void) davanti ingora cosa c'era, sostanzialmente vuole solo togleire senza usare cosa si è toltos
      (void)board_.pop(dest);

      //ripristina riserva
      const Piece p = u.placedPiece.value();
      hands_[colorIndex(p.color)][bugIndex(p.bug)] += 1;
    } 
    else {// Move
      const Coord src  = m.from.value();
      const Coord dest = m.to.value();
      // la pedina mossa è ora in cima a dest, riportala indietro
      board_.moveTop(dest, src);
    }
  }

private:
  //la disposizione dei pezzi
  Board board_;

  //da questo stato, chi deve giocare
  Color toMove_{Color::White};
  
  //pezzi ancora da giocare
  BothHands hands_{};
  
  //numeri di mezzi turni di ogni giocatore: un turno completo è una mossa di A e una mossa di B
  //in hive non c'è questa alternanza, numero di mosse giocate da un giocatore è utile per queen rule
  int plyWhite_{0};
  int plyBlack_{0};
  
  //per queen rule
  bool whiteQueenPlaced_{false};
  bool blackQueenPlaced_{false};
};

} // namespace hive::core

#endif
