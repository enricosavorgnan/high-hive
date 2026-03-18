#pragma once

#include "board.h"
#include "pieces.h"
#include "moves.h"
#include <vector>
#include <array>
#include <optional>
#include <algorithm>

namespace Hive {

    static constexpr std::array<Piece, 14> INITIAL_WHITE_HAND = {{
        {Color::White, Bug::Ant, 1},
        {Color::White, Bug::Ant, 2},
        {Color::White, Bug::Ant, 3},
        {Color::White, Bug::Beetle, 1},
        {Color::White, Bug::Beetle, 2},
        {Color::White, Bug::Grasshopper, 1},
        {Color::White, Bug::Grasshopper, 2},
        {Color::White, Bug::Grasshopper, 3},
        {Color::White, Bug::Ladybug, 1},
        {Color::White, Bug::Mosquito, 1},
        {Color::White, Bug::Pillbug, 1},
        {Color::White, Bug::Queen, 1},
        {Color::White, Bug::Spider, 1},
        {Color::White, Bug::Spider, 2},
    }};


    static constexpr std::array<Piece, 14> INITIAL_BLACK_HAND = {{
        {Color::Black, Bug::Ant, 1},
        {Color::Black, Bug::Ant, 2},
        {Color::Black, Bug::Ant, 3},
        {Color::Black, Bug::Beetle, 1},
        {Color::Black, Bug::Beetle, 2},
        {Color::Black, Bug::Grasshopper, 1},
        {Color::Black, Bug::Grasshopper, 2},
        {Color::Black, Bug::Grasshopper, 3},
        {Color::Black, Bug::Ladybug, 1},
        {Color::Black, Bug::Mosquito, 1},
        {Color::Black, Bug::Pillbug, 1},
        {Color::Black, Bug::Queen, 1},
        {Color::Black, Bug::Spider, 1},
        {Color::Black, Bug::Spider, 2},
    }};

    struct HistoryStep {
        Move move;
        std::optional<Coord> previousLastMovedPieceCoord;
        bool previousWhiteQueenPlaced{};
        bool previousBlackQueenPlaced{};
        int handIndex{}; // Caches the array index of the placed piece
    };


    class State {
        Board _board;
        Color _currentPlayer;
        int _currentPlayerTurn;

        // Hand tracking: Array indexed by Bug type for O(1) lookups
        std::array<Piece, 14> _whiteHand;
        std::array<Piece, 14> _blackHand;

        // for Placing-the-Queen Rule
        bool _whiteQueenPlaced;
        bool _blackQueenPlaced;

        // for Pillbug, Mosquitoes, Undo Operations
        std::optional<Coord> _lastMovedPieceCoord;
        // History is currently just a vector of steps
        std::vector<HistoryStep> _history;


        public:
            State(); // Initializes hands to standard 11 pieces, ply = 0

            // Constant O(1) Accessors required by Generators
            [[nodiscard]] const Board& board() const { return _board; }
            [[nodiscard]] Color toMove() const { return _currentPlayer; }
            [[nodiscard]] int getCurrentPlayerTurn() const { return _currentPlayerTurn; }
            [[nodiscard]] bool isQueenPlaced(Color color) const { return color == Color::White ? _whiteQueenPlaced : _blackQueenPlaced; }
            [[nodiscard]] std::optional<Coord> lastMovedPieceCoord() const { return _lastMovedPieceCoord; }

            // Returns weyther the hand of the color of the piece contains the given piece
            [[nodiscard]] bool hasInHand(Piece piece) const {
                const auto& hand = (piece.color == Color::White) ? _whiteHand : _blackHand;
                return std::ranges::find(hand, piece) != hand.end();
            }

            // Retrieves one of each available bug type to prevent duplicate game-tree branches
            [[nodiscard]] std::vector<Piece> getUniqueAvailablePieces(Color color) const {
                    std::vector<Piece> uniquePieces;
                    uniquePieces.reserve(8);
                    const auto& hand = (color == Color::White) ? _whiteHand : _blackHand;

                    bool found[8] = {false};
                    for (const Piece& p : hand) {
                        if (p.id != 255) { // 255 is the empty slot marker
                            int bugIdx = static_cast<int>(p.bug);
                            if (!found[bugIdx]) {
                                uniquePieces.push_back(p);
                                found[bugIdx] = true;
                            }
                        }
                    }
                    return uniquePieces;
                }

            // For applying a specific move onto the Board
            void applyMove(const Move& move);
            // For undoing the last move
            void undoLastMove();
        };

} // namespace Hive