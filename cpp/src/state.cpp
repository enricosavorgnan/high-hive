#include "headers/state.h"

#include <cstdlib>
#include <iostream>

namespace Hive {

namespace {
    const char* moveTypeStr(int t) {
        switch (t) {
            case Move::Place:     return "Place";
            case Move::PieceMove: return "PieceMove";
            case Move::Drag:      return "Drag";
            case Move::Pass:      return "Pass";
        }
        return "??";
    }

    void dumpMove(const Move& m, const char* prefix) {
        std::cerr << prefix << " type=" << moveTypeStr(m.type)
                  << " piece=" << (m.piece.color == Color::White ? "W" : "B")
                  << "/bug=" << static_cast<int>(m.piece.bug)
                  << "/id=" << static_cast<int>(m.piece.id)
                  << " from=(" << m.from.q << "," << m.from.r << ")"
                  << " to=(" << m.to.q << "," << m.to.r << ")";
        if (m.type == Move::Drag) {
            std::cerr << " pillbug=(" << m.pillbug.q << "," << m.pillbug.r << ")";
        }
        std::cerr << "\n";
    }

    void dumpHistory(const std::vector<HistoryStep>& history, size_t n = 12) {
        size_t start = history.size() > n ? history.size() - n : 0;
        std::cerr << "  History (last " << (history.size() - start)
                  << " of " << history.size() << "):\n";
        for (size_t i = start; i < history.size(); ++i) {
            std::cerr << "    [" << i << "] ";
            dumpMove(history[i].move, "");
        }
    }
} // anonymous namespace
    // State Constructor
    State::State()
        : _board(),
          _currentPlayer(Color::White),
          _currentPlayerTurn(0),
          _whiteHand(INITIAL_WHITE_HAND),
          _blackHand(INITIAL_BLACK_HAND),
          _whiteQueenPlaced(false),
          _blackQueenPlaced(false),
          _lastMovedPieceCoord(std::nullopt)
    {}


    void State::applyMove(const Move &move) {
        // DIAGNOSTIC: catch moves that would crash board.move/remove BEFORE the
        // CellStack::pop assert fires, so we can see the offending move and the
        // last few history moves. Remove once the self-play bug is fixed.
        if (move.type == Move::PieceMove || move.type == Move::Drag) {
            if (_board.empty(move.from)) {
                std::cerr << "[FATAL applyMove] move.from is empty (cannot move/drag a piece that is not there)\n";
                dumpMove(move, "  offending move:");
                dumpHistory(_history);
                std::cerr.flush();
                std::abort();
            }
        }

        HistoryStep step;
        step.move = move;
        step.previousLastMovedPieceCoord = _lastMovedPieceCoord;
        step.previousWhiteQueenPlaced = _whiteQueenPlaced;
        step.previousBlackQueenPlaced = _blackQueenPlaced;
        step.handIndex = -1;


        if (move.type == Move::Place) {
            auto& hand = (move.piece.color == Color::White) ? _whiteHand : _blackHand;
            auto piece = std::ranges::find(hand, move.piece);

            // If the piece is found in the hand, cache its index and overwrite it with a dummy "invalid" piece
            if (piece != hand.end()) {
                step.handIndex = static_cast<int>(std::distance(hand.begin(), piece));
                *piece = Piece{move.piece.color, Bug::Queen, 255};
            }

            _board.place(move.to, move.piece);

            // Queen Placement
            if (move.piece.bug == Bug::Queen) {
               if (move.piece.color == Color::White) _whiteQueenPlaced = true;
               else _blackQueenPlaced = true;
            }

            _lastMovedPieceCoord = move.to;


        } else if (move.type == Move::PieceMove || move.type == Move::Drag) {
            _board.move(move.from, move.to);
            _lastMovedPieceCoord = move.to;

        } else if (move.type == Move::Pass) {
            _lastMovedPieceCoord = std::nullopt;
        }

        // Commit history changes
        _history.push_back(step);
        // Update player turn
        _currentPlayerTurn++;
        // Update player
        _currentPlayer = rival(_currentPlayer);

    }

    void State::undoLastMove() {

        if (_history.empty()) return;

        const HistoryStep& step = _history.back();
        const Move& move = step.move;

        // DIAGNOSTIC: catch undos whose target cell is unexpectedly empty.
        if ((move.type == Move::PieceMove || move.type == Move::Drag || move.type == Move::Place)
            && _board.empty(move.to)) {
            std::cerr << "[FATAL undoLastMove] move.to is empty (the piece we placed there is gone)\n";
            dumpMove(move, "  offending move:");
            dumpHistory(_history);
            std::cerr.flush();
            std::abort();
        }

        _lastMovedPieceCoord = step.previousLastMovedPieceCoord;
        _whiteQueenPlaced = step.previousWhiteQueenPlaced;
        _blackQueenPlaced = step.previousBlackQueenPlaced;
        _currentPlayerTurn--;
        _currentPlayer = rival(_currentPlayer);

        if (move.type == Move::Place) {
            _board.remove(move.to);
            // Add piece on hand
            if (step.handIndex != -1) {
                auto& hand = (move.piece.color == Color::White) ? _whiteHand : _blackHand;
                hand[step.handIndex] = move.piece;
            }

        } else if (move.type == Move::PieceMove || move.type == Move::Drag) {
            _board.move(move.to, move.from);
        }

        // Turn back in history
        _history.pop_back();


    }
}
