#pragma once

#include "board.h"
#include "pieces.h"
#include "coords.h"
#include "moves.h"  // Must come before rules.h (defines Move struct)
#include "rules.h"

#include <array>
#include <vector>
#include <optional>

// GAME STATE
// Wraps Board + turn + hands + ply tracking + apply/undo.
// Needed for MCTS tree search which must apply and undo moves during simulation.

namespace Hive {

    constexpr int NUM_BUG_TYPES = 8;

    // Index helpers
    constexpr int colorIndex(Color c) { return (c == Color::White) ? 0 : 1; }

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
        return -1;
    }

    constexpr Bug bugFromIndex(int idx) {
        constexpr Bug bugs[] = {
            Bug::Queen, Bug::Beetle, Bug::Spider, Bug::Grasshopper,
            Bug::Ant, Bug::Ladybug, Bug::Mosquito, Bug::Pillbug
        };
        return bugs[idx];
    }

    // Hand counts: how many of each bug type a player has remaining
    using HandCounts = std::array<int, NUM_BUG_TYPES>;

    // Standard hand for Base+MLP: Q:1, B:2, S:2, G:3, A:3, L:1, M:1, P:1
    constexpr HandCounts standardHand() {
        return {1, 2, 2, 3, 3, 1, 1, 1};
    }

    // Next available piece id for a bug type given how many remain vs total
    // e.g., if 2 beetles total and 1 remains, next id = 2
    constexpr int standardTotal(Bug b) {
        switch (b) {
            case Bug::Queen:       return 1;
            case Bug::Beetle:      return 2;
            case Bug::Spider:      return 2;
            case Bug::Grasshopper: return 3;
            case Bug::Ant:         return 3;
            case Bug::Ladybug:     return 1;
            case Bug::Mosquito:    return 1;
            case Bug::Pillbug:     return 1;
        }
        return 0;
    }

    // Stores all info needed to undo a move
    struct UndoInfo {
        Move move;
        Color prevToMove{Color::White};
        int prevPlyWhite{0};
        int prevPlyBlack{0};
        bool prevWhiteQueenPlaced{false};
        bool prevBlackQueenPlaced{false};
        std::optional<Piece> placedPiece; // for Place moves: which piece was placed
    };

    // Terminal result
    enum class GameResult {
        None,       // game not over
        WhiteWin,
        BlackWin,
        Draw        // both queens surrounded simultaneously
    };

    class GameState {
    public:
        GameState() {
            hands_[0] = standardHand(); // White
            hands_[1] = standardHand(); // Black
        }

        // --- Accessors ---
        const Board& board() const { return board_; }
        Board& board() { return board_; }

        Color toMove() const { return toMove_; }

        int turnNumber() const {
            // Turn number = max(plyWhite, plyBlack) + 1 (1-indexed, increments after both play)
            return std::max(plyWhite_, plyBlack_) / 1 + 1;
            // Actually: turn = (total_ply / 2) + 1
        }

        int ply(Color c) const { return (c == Color::White) ? plyWhite_ : plyBlack_; }

        bool queenPlaced(Color c) const {
            return (c == Color::White) ? whiteQueenPlaced_ : blackQueenPlaced_;
        }

        int remaining(Color c, Bug b) const {
            return hands_[colorIndex(c)][bugIndex(b)];
        }

        bool hasInHand(Color c, Bug b) const {
            return remaining(c, b) > 0;
        }

        // Get the hand as a vector of Piece objects (for compatibility with RuleEngine)
        std::vector<Piece> getHand(Color c) const {
            std::vector<Piece> hand;
            for (int bi = 0; bi < NUM_BUG_TYPES; ++bi) {
                Bug bug = bugFromIndex(bi);
                int count = hands_[colorIndex(c)][bi];
                int total = standardTotal(bug);
                // Pieces on board have ids 1..total-count, remaining have ids total-count+1..total
                for (int i = 0; i < count; ++i) {
                    uint8_t id = static_cast<uint8_t>(total - count + 1 + i);
                    if (total == 1) id = 1; // Queen, Ladybug, Mosquito, Pillbug
                    hand.push_back(Piece{c, bug, id});
                }
            }
            return hand;
        }

        // Generate all legal moves for the current player
        std::vector<Move> legalMoves() const {
            std::vector<Piece> hand = getHand(toMove_);
            return RuleEngine::generateMoves(board_, toMove_, hand);
        }

        // Apply a move and return undo information
        UndoInfo apply(const Move& m) {
            UndoInfo u;
            u.move = m;
            u.prevToMove = toMove_;
            u.prevPlyWhite = plyWhite_;
            u.prevPlyBlack = plyBlack_;
            u.prevWhiteQueenPlaced = whiteQueenPlaced_;
            u.prevBlackQueenPlaced = blackQueenPlaced_;

            if (m.type == Move::Pass) {
                // Nothing to do on the board
            } else if (m.type == Move::Place) {
                Piece p = m.piece;
                u.placedPiece = p;

                // Update hand counts
                hands_[colorIndex(p.color)][bugIndex(p.bug)] -= 1;

                // Place on board
                board_.place(m.to, p);

                // Track queen placement
                if (p.bug == Bug::Queen) {
                    if (p.color == Color::White) whiteQueenPlaced_ = true;
                    else blackQueenPlaced_ = true;
                }
            } else { // PieceMove
                board_.move(m.from, m.to);
            }

            // Update ply and turn
            if (toMove_ == Color::White) ++plyWhite_;
            else ++plyBlack_;
            toMove_ = rival(toMove_);

            return u;
        }

        // Undo a move using saved undo information
        void undo(const UndoInfo& u) {
            toMove_ = u.prevToMove;
            plyWhite_ = u.prevPlyWhite;
            plyBlack_ = u.prevPlyBlack;
            whiteQueenPlaced_ = u.prevWhiteQueenPlaced;
            blackQueenPlaced_ = u.prevBlackQueenPlaced;

            const Move& m = u.move;

            if (m.type == Move::Pass) {
                return;
            } else if (m.type == Move::Place) {
                // Remove the placed piece
                board_.remove(m.to);

                // Restore hand
                const Piece& p = u.placedPiece.value();
                hands_[colorIndex(p.color)][bugIndex(p.bug)] += 1;
            } else { // PieceMove
                // Move the piece back
                board_.move(m.to, m.from);
            }
        }

        // Check if the game is over
        bool isTerminal() const {
            return result() != GameResult::None;
        }

        // Check game result by examining queen surroundings
        GameResult result() const {
            bool whiteSurrounded = isQueenSurrounded(Color::White);
            bool blackSurrounded = isQueenSurrounded(Color::Black);

            if (whiteSurrounded && blackSurrounded) return GameResult::Draw;
            if (whiteSurrounded) return GameResult::BlackWin;
            if (blackSurrounded) return GameResult::WhiteWin;
            return GameResult::None;
        }

        // Result as float from perspective of given color: +1 win, -1 loss, 0 draw
        float resultForColor(Color c) const {
            GameResult r = result();
            if (r == GameResult::Draw) return 0.0f;
            if (r == GameResult::None) return 0.0f;
            bool win = (r == GameResult::WhiteWin && c == Color::White) ||
                       (r == GameResult::BlackWin && c == Color::Black);
            return win ? 1.0f : -1.0f;
        }

    private:
        bool isQueenSurrounded(Color c) const {
            if (!queenPlaced(c)) return false;

            // Find queen position
            const auto& occupied = board_.occupiedCoords();
            for (const auto& coord : occupied) {
                int idx = Board::AxToIndex(coord);
                for (int i = 0; i < board_._grid[idx].size(); ++i) {
                    if (board_._grid[idx]._data[i].color == c &&
                        board_._grid[idx]._data[i].bug == Bug::Queen) {
                        // Check all 6 neighbors are occupied
                        auto neighbors = coordNeighbors(coord);
                        for (const auto& n : neighbors) {
                            if (board_.empty(n)) return false;
                        }
                        return true;
                    }
                }
            }
            return false;
        }

        Board board_;
        Color toMove_{Color::White};
        std::array<HandCounts, 2> hands_{};
        int plyWhite_{0};
        int plyBlack_{0};
        bool whiteQueenPlaced_{false};
        bool blackQueenPlaced_{false};
    };

} // namespace Hive
