#include "6state.h"
#include "7movegen_place.h"
#include "9movegen_move.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Wrapper UHP minimale per testare il core (file 1..9) con MzingaViewer.
// Obiettivo: essere "parlante" UHP, tenere lo stato e rispondere a validmoves/bestmove.
// AI: sceglie una mossa a caso tra quelle valide.
//
// Nota: NON modifichiamo i file del core. Qui facciamo solo adattamento I/O + conversioni UHP.

namespace hive::uhp {
using namespace hive::core;

// ------------------ util stringhe ------------------

static inline std::string trim(const std::string& s) {
  size_t a = 0;
  while (a < s.size() && std::isspace((unsigned char)s[a])) ++a;
  size_t b = s.size();
  while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
  return s.substr(a, b - a);
}

static inline std::vector<std::string> split_ws(const std::string& line) {
  std::istringstream iss(line);
  std::vector<std::string> out;
  std::string tok;
  while (iss >> tok) out.push_back(tok);
  return out;
}

static inline std::vector<std::string> split_semicolon(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  std::istringstream iss(s);
  while (std::getline(iss, cur, ';')) out.push_back(cur);
  return out;
}

// ------------------ mappa direzioni UHP <-> Coord ------------------

// UHP (BoardSpace):
//  ref/  = top right
//  ref-  = right
//  ref\  = bottom right
//  /ref  = bottom left
//  -ref  = left
//  \ref  = top left
// rispetto al nostro HEXGRID_DIRS:
//  E  = dir0  ( +1, 0)
//  SE = dir1  ( 0, +1)
//  SW = dir2  ( -1,+1)
//  W  = dir3  ( -1, 0)
//  NW = dir4  ( 0, -1)
//  NE = dir5  ( +1,-1)

static inline Coord dirFromUhpIndicator(bool isPrefix, char ind) {
  // restituisce il vettore direzione dal riferimento alla destinazione
  if (!isPrefix) {
    // suffissi
    if (ind == '-') return HEXGRID_DIRS[0]; // destra
    if (ind == '/') return HEXGRID_DIRS[5]; // alto-destra
    if (ind == '\\') return HEXGRID_DIRS[1]; // basso-destra
  } else {
    // prefissi
    if (ind == '-') return HEXGRID_DIRS[3]; // sinistra
    if (ind == '/') return HEXGRID_DIRS[2]; // basso-sinistra
    if (ind == '\\') return HEXGRID_DIRS[4]; // alto-sinistra
  }
  return Coord{0,0};
}

static inline std::string uhpPosTokenFromRefAndDest(const std::string& refPiece, int dirIdx) {
  // costruisce la stringa posizione (ref con prefisso/suffisso) dato dirIdx = direzione da ref -> dest
  switch (dirIdx) {
    case 0: return refPiece + "-";          // destra
    case 1: return refPiece + "\\";         // basso-destra
    case 2: return std::string("/") + refPiece; // basso-sinistra
    case 3: return std::string("-") + refPiece; // sinistra
    case 4: return std::string("\\") + refPiece; // alto-sinistra
    case 5: return refPiece + "/";          // alto-destra
    default: break;
  }
  return refPiece; // fallback
}

// ------------------ parsing PieceString UHP ------------------

struct ParsedPieceName {
  Color color{Color::White};
  Bug bug{Bug::Queen};
  int index{1};            // 1-based se presente; altrimenti 1
  bool hasIndex{false};
};

static inline std::optional<ParsedPieceName> parsePieceName(const std::string& s) {
  if (s.size() < 2) return std::nullopt;
  ParsedPieceName p;
  if (s[0] == 'w') p.color = Color::White;
  else if (s[0] == 'b') p.color = Color::Black;
  else return std::nullopt;

  char b = s[1];
  switch (b) {
    case 'Q': p.bug = Bug::Queen; break;
    case 'B': p.bug = Bug::Beetle; break;
    case 'S': p.bug = Bug::Spider; break;
    case 'G': p.bug = Bug::Grasshopper; break;
    case 'A': p.bug = Bug::Ant; break;
    case 'M': p.bug = Bug::Mosquito; break;
    case 'L': p.bug = Bug::Ladybug; break;
    case 'P': p.bug = Bug::Pillbug; break;
    default: return std::nullopt;
  }

  if (s.size() > 2) {
    // numero presente
    int idx = 0;
    for (size_t i = 2; i < s.size(); ++i) {
      if (!std::isdigit((unsigned char)s[i])) return std::nullopt;
      idx = idx * 10 + (s[i] - '0');
    }
    if (idx <= 0) return std::nullopt;
    p.index = idx;
    p.hasIndex = true;
  }

  return p;
}

static inline int maxIndexForBug(Bug bug) {
  switch (bug) {
    case Bug::Queen: return 1;
    case Bug::Beetle: return 2;
    case Bug::Spider: return 2;
    case Bug::Grasshopper: return 3;
    case Bug::Ant: return 3;
    case Bug::Ladybug: return 1;
    case Bug::Mosquito: return 1;
    case Bug::Pillbug: return 1;
  }
  return 1;
}

static inline std::string pieceNameToString(Color c, Bug b, int idx) {
  char col = (c == Color::White) ? 'w' : 'b';
  char letter = '?';
  switch (b) {
    case Bug::Queen: letter = 'Q'; break;
    case Bug::Beetle: letter = 'B'; break;
    case Bug::Spider: letter = 'S'; break;
    case Bug::Grasshopper: letter = 'G'; break;
    case Bug::Ant: letter = 'A'; break;
    case Bug::Ladybug: letter = 'L'; break;
    case Bug::Mosquito: letter = 'M'; break;
    case Bug::Pillbug: letter = 'P'; break;
  }

  std::string out;
  out.push_back(col);
  out.push_back(letter);
  int m = maxIndexForBug(b);
  if (m > 1) out += std::to_string(idx);
  return out;
}

// ------------------ "Matrice" UHP: stacks di nomi + posizioni ------------------

class UhpBoard {
public:
  bool occupied(const Coord& c) const {
    auto it = stacks_.find(c);
    return it != stacks_.end() && !it->second.empty();
  }

  const std::vector<std::string>* stackAt(const Coord& c) const {
    auto it = stacks_.find(c);
    if (it == stacks_.end()) return nullptr;
    return &it->second;
  }

  std::optional<std::string> topName(const Coord& c) const {
    auto it = stacks_.find(c);
    if (it == stacks_.end() || it->second.empty()) return std::nullopt;
    return it->second.back();
  }

  bool hasPiece(const std::string& pieceName) const {
    return piecePos_.find(pieceName) != piecePos_.end();
  }

  std::optional<Coord> whereIs(const std::string& pieceName) const {
    auto it = piecePos_.find(pieceName);
    if (it == piecePos_.end()) return std::nullopt;
    return it->second;
  }

  void clear() {
    stacks_.clear();
    piecePos_.clear();
  }

  // piazza un nuovo pezzo (come nome UHP)
  void push(const Coord& to, const std::string& pieceName) {
    stacks_[to].push_back(pieceName);
    piecePos_[pieceName] = to;
  }

  // muove il top da from a to
  bool moveTop(const Coord& from, const Coord& to) {
    auto it = stacks_.find(from);
    if (it == stacks_.end() || it->second.empty()) return false;
    std::string pieceName = it->second.back();
    it->second.pop_back();
    if (it->second.empty()) stacks_.erase(it);

    stacks_[to].push_back(pieceName);
    piecePos_[pieceName] = to;
    return true;
  }

  // toglie il top e lo ritorna
  std::optional<std::string> pop(const Coord& from) {
    auto it = stacks_.find(from);
    if (it == stacks_.end() || it->second.empty()) return std::nullopt;
    std::string pieceName = it->second.back();
    it->second.pop_back();
    if (it->second.empty()) stacks_.erase(it);

    piecePos_.erase(pieceName);
    return pieceName;
  }

  // ritorna il nome del prossimo pezzo ("il prossimo A" ecc.) secondo gli indici non ancora usati.
  std::string nextPieceName(Color c, Bug b) const {
    int m = maxIndexForBug(b);
    for (int idx = 1; idx <= m; ++idx) {
      std::string name = pieceNameToString(c, b, idx);
      if (!hasPiece(name)) return name;
    }
    // fallback (non dovrebbe succedere se hand counts sono coerenti)
    return pieceNameToString(c, b, m);
  }

private:
  std::unordered_map<Coord, std::vector<std::string>, CoordHash> stacks_;
  std::unordered_map<std::string, Coord> piecePos_;
};

// ------------------ Generazione mosse "totale" (Place + Move) ------------------

static inline std::vector<Move> generateAllValidMoves(const State& s) {
  std::vector<Move> out;

  // Placements
  auto pl = generatePlacements(s);

  // Regola (compat Mzinga/UHP): NON puoi piazzare la regina come prima mossa del giocatore.
  // (Mzinga wiki mostra esplicitamente questo caso come invalidmove.)
  if (s.ply(s.toMove()) == 0) {
    pl.erase(std::remove_if(pl.begin(), pl.end(), [&](const Move& m) {
      return m.kind == MoveKind::Place && m.piece && m.piece->bug == Bug::Queen;
    }), pl.end());
  }

  out.insert(out.end(), pl.begin(), pl.end());

  // Movements
  auto mv = generateMovements(s);
  out.insert(out.end(), mv.begin(), mv.end());

  return out;
}

// ------------------ Conversioni: Move (core) -> MoveString (UHP) ------------------

class UhpCodec {
public:
  UhpCodec(const State& st, const UhpBoard& ub, const std::vector<std::string>& history)
    : s_(st), ub_(ub), history_(history) {}

  // serializza una mossa core in notazione UHP, usando la "matrice" UHP per i nomi pezzo.
  std::string moveToUhp(const Move& m) const {
    if (m.kind == MoveKind::Resign) {
      // nel protocollo UHP non c'è "resign" come MoveString; qui lo usiamo internamente per pass.
      return "pass";
    }

    const Board& b = s_.board();

    if (m.kind == MoveKind::Place) {
      const Piece p = m.piece.value();
      const Coord dest = m.to.value();
      const std::string pieceName = ub_.nextPieceName(p.color, p.bug);

      // Prima mossa della partita: solo il pezzo.
      if (b.occupiedCells().empty()) return pieceName;

      // Destinazione relativa a un vicino occupato.
      return pieceName + " " + destToRelativeToken(dest);
    }

    // Move
    const Coord from = m.from.value();
    const Coord dest = m.to.value();

    auto moverNameOpt = ub_.topName(from);
    if (!moverNameOpt) {
      // fallback: non dovrebbe succedere
      return "pass";
    }
    const std::string moverName = *moverNameOpt;

    // Se la destinazione è occupata (beetle/mosquito-beetle): UHP vuole il pezzo che stai per coprire.
    if (ub_.occupied(dest)) {
      auto coveredNameOpt = ub_.topName(dest);
      if (coveredNameOpt) {
        return moverName + " " + *coveredNameOpt;
      }
    }

    return moverName + " " + destToRelativeToken(dest);
  }

  // ------------------ parsing "leggero": MoveString (UHP) -> richiesta ------------------

  struct ParsedRequest {
    bool isPass{false};
    std::string pieceName; // wA1...
    std::optional<ParsedPieceName> parsedPiece;
    std::optional<Coord> dest;
  };

  // Parsing leggero: ricostruisce solo (pieceName, destCoord).
  // La scelta "Move vs Drag" si fa matchando contro le mosse valide del core.
  std::optional<ParsedRequest> parseUhpRequest(const std::string& moveStr) const {
    ParsedRequest pr;
    std::string ms = trim(moveStr);
    if (ms == "pass") {
      pr.isPass = true;
      return pr;
    }

    auto toks = split_ws(ms);
    if (toks.empty()) return std::nullopt;

    pr.pieceName = toks[0];
    pr.parsedPiece = parsePieceName(pr.pieceName);
    if (!pr.parsedPiece) return std::nullopt;

    // Primo move del game: solo il pezzo, va in (0,0)
    if (toks.size() == 1) {
      if (!s_.board().occupiedCells().empty()) return std::nullopt;
      pr.dest = Coord{0,0};
      return pr;
    }

    auto dOpt = parseRelativePositionToken(toks[1]);
    if (!dOpt) return std::nullopt;
    pr.dest = *dOpt;
    return pr;
  }

private:
  const State& s_;
  const UhpBoard& ub_;
  const std::vector<std::string>& history_;

  // Dato un dest vuoto, produce un token relativo usando un vicino occupato.
  std::string destToRelativeToken(const Coord& dest) const {
    // Cerca un vicino occupato in ordine deterministico.
    auto neigh = hexgridNeighbors(dest);
    for (const Coord& refCoord : neigh) {
      auto refNameOpt = ub_.topName(refCoord);
      if (!refNameOpt) continue;

      int dirIdx = hexgridDirectionIndex(refCoord, dest);
      if (dirIdx < 0) continue;
      return uhpPosTokenFromRefAndDest(*refNameOpt, dirIdx);
    }

    // Fallback (non dovrebbe succedere per mosse valide)
    // Se proprio non troviamo un vicino, scegliamo un pezzo qualunque esistente.
    auto occ = s_.board().occupiedCells();
    if (!occ.empty()) {
      auto refNameOpt = ub_.topName(occ[0]);
      if (refNameOpt) return *refNameOpt + "-";
    }
    return "wQ-";
  }

  // parsing token posizione (secondo sintassi UHP)
  std::optional<Coord> parseRelativePositionToken(const std::string& tok) const {
    if (tok.empty()) return std::nullopt;

    bool isPrefix = false;
    char ind = 0;
    std::string ref = tok;

    if (tok[0] == '-' || tok[0] == '/' || tok[0] == '\\') {
      isPrefix = true;
      ind = tok[0];
      ref = tok.substr(1);
    } else if (tok.back() == '-' || tok.back() == '/' || tok.back() == '\\') {
      isPrefix = false;
      ind = tok.back();
      ref = tok.substr(0, tok.size() - 1);
    } else {
      // nessun indicatore: significa "vai sopra quel pezzo" (dest = coord del ref)
      ind = 0;
      ref = tok;
    }

    auto refCoordOpt = ub_.whereIs(ref);
    if (!refCoordOpt) return std::nullopt;
    Coord refCoord = *refCoordOpt;

    if (ind == 0) return refCoord;

    Coord d = dirFromUhpIndicator(isPrefix, ind);
    return refCoord + d;
  }
};

// ------------------ UHP engine loop ------------------

struct HistoryEntry {
  std::string uhpMove;
  Undo undo;
  bool isPass{false};

  // per undo della "matrice" UHP
  std::optional<Coord> from;
  std::optional<Coord> to;
  std::optional<std::string> placedOrMovedPieceName; // nome UHP del pezzo coinvolto (top)
};

class UhpEngine {
public:
  UhpEngine() : rng_(std::random_device{}()) {
    reset("Base+MLP");
  }

  void loop() {
    // UHP: bisogna rispondere a info automaticamente all'avvio.
    cmdInfo();

    std::string line;
    while (std::getline(std::cin, line)) {
      line = trim(line);
      if (line.empty()) continue;

      auto chunks = split_ws(line);
      if (chunks.empty()) continue;

      const std::string& cmd = chunks[0];

      if (cmd == "u1") {
        // alcuni tool lo mandano; rispondiamo ok.
        std::cout << "ok\n";
      } else if (cmd == "info") {
        cmdInfo();
      } else if (cmd == "newgame") {
        cmdNewGame(line);
      } else if (cmd == "play") {
        cmdPlay(line);
      } else if (cmd == "pass") {
        cmdPass();
      } else if (cmd == "validmoves" || cmd == "genmoves") {
        cmdValidMoves();
      } else if (cmd == "bestmove") {
        cmdBestMove();
      } else if (cmd == "undo") {
        cmdUndo(line);
      } else if (cmd == "options") {
        cmdOptions();
      } else if (cmd == "exit" || cmd == "quit") {
        break;
      } else {
        std::cout << "err Comando sconosciuto\n";
        std::cout << "ok\n";
      }

      std::cout << std::flush;
    }
  }

private:
  State s_;
  UhpBoard ub_;
  std::string gameType_;
  std::vector<HistoryEntry> hist_;
  std::mt19937 rng_;

  // ----------- comandi -----------

  void cmdInfo() {
    std::cout << "id high-hive-engine v0.2\n";
    std::cout << "Mosquito;Ladybug;Pillbug\n";
    std::cout << "ok\n";
  }

  void cmdNewGame(const std::string& line) {
    // newgame
    // newgame GameTypeString
    // newgame GameString

    std::string rest;
    {
      auto pos = line.find(' ');
      if (pos != std::string::npos) rest = trim(line.substr(pos + 1));
    }

    if (rest.empty()) {
      // richiesta: sempre espansioni
      reset("Base+MLP");
      std::cout << gameString() << "\n";
      std::cout << "ok\n";
      return;
    }

    // Se contiene ';' è un GameString, altrimenti è un GameTypeString
    if (rest.find(';') == std::string::npos) {
      reset(rest);
      std::cout << gameString() << "\n";
      std::cout << "ok\n";
      return;
    }

    // GameString: Base+MLP;InProgress;White[3];...
    auto parts = split_semicolon(rest);
    if (parts.size() < 3) {
      std::cout << "err GameString non valido\n";
      std::cout << "ok\n";
      return;
    }

    reset(parts[0]);

    // replay mosse da parts[3..]
    for (size_t i = 3; i < parts.size(); ++i) {
      std::string mv = trim(parts[i]);
      if (mv.empty()) continue;
      if (!applyMoveString(mv, /*validate*/false)) {
        std::cout << "err GameString contiene una mossa non applicabile: " << mv << "\n";
        std::cout << "ok\n";
        return;
      }
    }

    std::cout << gameString() << "\n";
    std::cout << "ok\n";
  }

  void cmdPlay(const std::string& line) {
    // play MoveString
    auto pos = line.find(' ');
    if (pos == std::string::npos) {
      std::cout << "invalidmove manca la mossa\n";
      std::cout << "ok\n";
      return;
    }

    std::string moveStr = trim(line.substr(pos + 1));
    if (moveStr.empty()) {
      std::cout << "invalidmove mossa vuota\n";
      std::cout << "ok\n";
      return;
    }

    if (!applyMoveString(moveStr, /*validate*/true)) {
      std::cout << "invalidmove mossa illegale o non riconosciuta\n";
      std::cout << "ok\n";
      return;
    }

    std::cout << gameString() << "\n";
    std::cout << "ok\n";
  }

  void cmdPass() {
    // pass = play pass
    if (!applyMoveString("pass", /*validate*/true)) {
      std::cout << "invalidmove non puoi passare se hai mosse valide\n";
      std::cout << "ok\n";
      return;
    }

    std::cout << gameString() << "\n";
    std::cout << "ok\n";
  }

  void cmdValidMoves() {
    UhpCodec codec(s_, ub_, currentHistoryStrings());
    auto moves = generateAllValidMoves(s_);
    if (moves.empty()) {
      std::cout << "pass\n";
      std::cout << "ok\n";
      return;
    }

    for (size_t i = 0; i < moves.size(); ++i) {
      std::cout << codec.moveToUhp(moves[i]);
      if (i + 1 < moves.size()) std::cout << ";";
    }
    std::cout << "\n";
    std::cout << "ok\n";
  }

  void cmdBestMove() {
    UhpCodec codec(s_, ub_, currentHistoryStrings());
    auto moves = generateAllValidMoves(s_);
    if (moves.empty()) {
      std::cout << "pass\n";
      std::cout << "ok\n";
      return;
    }

    std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
    Move chosen = moves[dist(rng_)];

    // stampa la mossa in UHP
    std::cout << codec.moveToUhp(chosen) << "\n";
    std::cout << "ok\n";
  }

  void cmdUndo(const std::string& line) {
    // undo [n]
    int n = 1;
    auto chunks = split_ws(line);
    if (chunks.size() >= 2) {
      n = std::max(1, std::atoi(chunks[1].c_str()));
    }

    while (n-- > 0 && !hist_.empty()) {
      undoOne();
    }

    std::cout << gameString() << "\n";
    std::cout << "ok\n";
  }

  void cmdOptions() {
    // non abbiamo opzioni al momento
    std::cout << "ok\n";
  }

  // ----------- core: reset / apply / undo / gamestate -----------

  void reset(const std::string& gt) {
    gameType_ = gt.empty() ? "Base+MLP" : gt;
    s_ = State();
    ub_.clear();
    hist_.clear();
  }

  std::vector<std::string> currentHistoryStrings() const {
    std::vector<std::string> out;
    out.reserve(hist_.size());
    for (const auto& e : hist_) out.push_back(e.uhpMove);
    return out;
  }

  std::string gameStateString() const {
    return hist_.empty() ? "NotStarted" : "InProgress";
  }

  std::string turnString() const {
    // UHP: White[1], Black[1], White[2], Black[2]...
    Color tm = s_.toMove();
    int turn = 1;
    if (tm == Color::White) {
      turn = s_.ply(Color::Black) + 1;
    } else {
      turn = s_.ply(Color::White);
      if (turn < 1) turn = 1;
    }

    std::ostringstream oss;
    oss << (tm == Color::White ? "White" : "Black") << "[" << turn << "]";
    return oss.str();
  }

  std::string gameString() const {
    std::ostringstream oss;
    oss << gameType_ << ";" << gameStateString() << ";" << turnString();
    for (const auto& e : hist_) {
      oss << ";" << e.uhpMove;
    }
    return oss.str();
  }

  bool applyMoveString(const std::string& moveStr, bool validate) {
    UhpCodec codec(s_, ub_, currentHistoryStrings());
    auto prOpt = codec.parseUhpRequest(moveStr);
    if (!prOpt) return false;
    const auto& pr = *prOpt;

    // Validazione: pass permesso solo se non ci sono mosse.
    if (pr.isPass) {
      if (validate) {
        auto moves = generateAllValidMoves(s_);
        if (!moves.empty()) return false;
      }
      HistoryEntry he;
      he.uhpMove = "pass";
      he.isPass = true;
      he.undo = s_.apply(Move::resign());
      hist_.push_back(he);
      return true;
    }

    if (!pr.dest || !pr.parsedPiece) return false;
    const Coord dest = *pr.dest;
    const ParsedPieceName pp = *pr.parsedPiece;

    // Genera mosse valide e seleziona quella che corrisponde semanticamente alla richiesta.
    // Questo permette anche di distinguere automaticamente Move vs Drag (se presenti nel tuo core).
    auto moves = generateAllValidMoves(s_);

    std::optional<Move> chosen;

    auto fromOpt = ub_.whereIs(pr.pieceName);
    if (!fromOpt) {
      // Place
      for (const auto& mv : moves) {
        if (mv.kind != MoveKind::Place) continue;
        if (!mv.piece || !mv.to) continue;
        if (mv.piece->color != pp.color) continue;
        if (mv.piece->bug != pp.bug) continue;
        if (*mv.to != dest) continue;
        chosen = mv;
        break;
      }
    } else {
      // Move/Drag: deve muoversi il pezzo top nello stack
      Coord from = *fromOpt;
      auto topName = ub_.topName(from);
      if (!topName || *topName != pr.pieceName) return false;

      for (const auto& mv : moves) {
        if (mv.kind == MoveKind::Place) continue;
        if (!mv.from || !mv.to) continue;
        if (*mv.from != from) continue;
        if (*mv.to != dest) continue;
        chosen = mv;
        break;
      }
    }

    if (!chosen) {
      return false;
    }

    // Applica la mossa scelta (che può essere Move o Drag nel tuo core).
    const Move cm = *chosen;

    HistoryEntry he;
    he.uhpMove = moveStr;
    he.isPass = false;

    if (cm.kind == MoveKind::Place) {
      he.to = cm.to;
      he.placedOrMovedPieceName = pr.pieceName;
      he.undo = s_.apply(cm);
      ub_.push(cm.to.value(), pr.pieceName);
    } else {
      he.from = cm.from;
      he.to = cm.to;
      he.undo = s_.apply(cm);
      if (!ub_.moveTop(cm.from.value(), cm.to.value())) return false;
      he.placedOrMovedPieceName = ub_.topName(cm.to.value());
    }

    hist_.push_back(he);
    return true;
  }

  static bool sameCoreMove(const Move& a, const Move& b) {
    if (a.kind != b.kind) return false;
    if (a.kind == MoveKind::Place) {
      if (!a.piece || !b.piece || !a.to || !b.to) return false;
      return a.piece->color == b.piece->color && a.piece->bug == b.piece->bug && *a.to == *b.to;
    }
    if (a.kind == MoveKind::Move) {
      if (!a.from || !b.from || !a.to || !b.to) return false;
      return *a.from == *b.from && *a.to == *b.to;
    }
    return true;
  }

  void undoOne() {
    HistoryEntry he = hist_.back();
    hist_.pop_back();

    // undo core prima o dopo non cambia molto, ma per sicurezza undo core dopo aver preso i nomi
    // (qui abbiamo già salvato tutto in he).

    if (!he.isPass) {
      // Undo UHP board
      if (he.undo.move.kind == MoveKind::Place) {
        Coord dest = he.undo.move.to.value();
        (void)ub_.pop(dest);
      } else if (he.undo.move.kind == MoveKind::Move) {
        Coord src = he.undo.move.from.value();
        Coord dest = he.undo.move.to.value();
        // La pedina mossa è top a dest (perché stiamo annullando l'ultima mossa).
        (void)ub_.moveTop(dest, src);
      }
    }

    // Undo core
    s_.undo(he.undo);
  }
};

} // namespace hive::uhp

int main() {
  hive::uhp::UhpEngine engine;
  engine.loop();
  return 0;
}
