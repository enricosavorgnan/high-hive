#pragma once

#include "board.h"
#include "pieces.h"
#include "moves.h"
#include <vector>
#include <array>
#include <optional>
#include <algorithm>

namespace Hive {

    // Terminal result (used by learning module)
    enum class GameResult { None, WhiteWin, BlackWin, Draw };

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

            // For applying a specific move onto the Board
            void applyMove(const Move& move);
            // For undoing the last move
            void undoLastMove();

            // Get unique available pieces from hand (one representative per bug type, for placement)
            [[nodiscard]] std::vector<Piece> getUniqueAvailablePieces(Color c) const {
                const auto& hand = (c == Color::White) ? _whiteHand : _blackHand;
                std::vector<Piece> unique;
                uint8_t seenBug = 0; // bitmask of Bug types already added
                for (const auto& p : hand) {
                    if (p.id == 255) continue; // removed piece
                    uint8_t bit = 1u << static_cast<uint8_t>(p.bug);
                    if (!(seenBug & bit)) {
                        seenBug |= bit;
                        unique.push_back(p);
                    }
                }
                return unique;
            }

            // --- Learning module convenience methods ---

            // Get hand as vector (excludes removed pieces marked with id=255)
            [[nodiscard]] std::vector<Piece> getHand(Color c) const {
                const auto& hand = (c == Color::White) ? _whiteHand : _blackHand;
                std::vector<Piece> result;
                for (const auto& p : hand) {
                    if (p.id != 255) result.push_back(p);
                }
                return result;
            }

            // Count remaining pieces of a bug type in hand
            [[nodiscard]] int remaining(Color c, Bug bug) const {
                const auto& hand = (c == Color::White) ? _whiteHand : _blackHand;
                int count = 0;
                for (const auto& p : hand) {
                    if (p.bug == bug && p.id != 255) ++count;
                }
                return count;
            }

            // Check if a queen is surrounded (all 6 neighbors occupied)
            [[nodiscard]] bool isQueenSurrounded(Color c) const {
                if (!isQueenPlaced(c)) return false;
                for (const auto& coord : _board.occupiedCoords()) {
                    int idx = Board::AxToIndex(coord);
                    for (int h = 0; h < _board.height(coord); ++h) {
                        const Piece& p = _board.cellAt(idx)._data[h];
                        if (p.color == c && p.bug == Bug::Queen) {
                            auto neighbors = coordNeighbors(coord);
                            for (const auto& n : neighbors) {
                                if (_board.empty(n)) return false;
                            }
                            return true;
                        }
                    }
                }
                return false;
            }

            [[nodiscard]] GameResult result() const {
                bool ws = isQueenSurrounded(Color::White);
                bool bs = isQueenSurrounded(Color::Black);
                if (ws && bs) return GameResult::Draw;
                if (ws) return GameResult::BlackWin;
                if (bs) return GameResult::WhiteWin;
                return GameResult::None;
            }

            [[nodiscard]] bool isTerminal() const {
                return result() != GameResult::None;
            }

            [[nodiscard]] float resultForColor(Color c) const {
                GameResult r = result();
                if (r == GameResult::Draw || r == GameResult::None) return 0.0f;
                bool win = (r == GameResult::WhiteWin && c == Color::White) ||
                           (r == GameResult::BlackWin && c == Color::Black);
                return win ? 1.0f : -1.0f;
            }
        };

} // namespace Hive