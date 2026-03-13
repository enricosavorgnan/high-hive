#include "headers/state.h"

namespace Hive {
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
