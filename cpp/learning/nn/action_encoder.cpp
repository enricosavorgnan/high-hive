#include "nn/headers/action_encoder.h"
#include "board.h"
#include "coords.h"
#include "pieces.h"
#include "utils.h"

namespace Hive::Learning {

    // Piece ordering per color (14 pieces):
    // Q, B1, B2, S1, S2, G1, G2, G3, A1, A2, A3, L1, M1, P1
    int ActionEncoder::pieceToIndex(const Piece& piece) {
        int colorOffset = (piece.color == Color::White) ? 0 : 14;
        int bugOffset = 0;

        switch (piece.bug) {
            case Bug::Queen:       bugOffset = 0; break;
            case Bug::Beetle:      bugOffset = 1 + (piece.id - 1); break; // 1-2
            case Bug::Spider:      bugOffset = 3 + (piece.id - 1); break; // 3-4
            case Bug::Grasshopper: bugOffset = 5 + (piece.id - 1); break; // 5-7
            case Bug::Ant:         bugOffset = 8 + (piece.id - 1); break; // 8-10
            case Bug::Ladybug:     bugOffset = 11; break;
            case Bug::Mosquito:    bugOffset = 12; break;
            case Bug::Pillbug:     bugOffset = 13; break;
        }

        return colorOffset + bugOffset;
    }

    Piece ActionEncoder::indexToPiece(int idx) {
        Color color = (idx < 14) ? Color::White : Color::Black;
        int local = idx % 14;

        // Map local index to bug type and id
        struct BugInfo { Bug bug; uint8_t id; };
        static const BugInfo mapping[14] = {
            {Bug::Queen, 1},
            {Bug::Beetle, 1}, {Bug::Beetle, 2},
            {Bug::Spider, 1}, {Bug::Spider, 2},
            {Bug::Grasshopper, 1}, {Bug::Grasshopper, 2}, {Bug::Grasshopper, 3},
            {Bug::Ant, 1}, {Bug::Ant, 2}, {Bug::Ant, 3},
            {Bug::Ladybug, 1},
            {Bug::Mosquito, 1},
            {Bug::Pillbug, 1}
        };

        return Piece{color, mapping[local].bug, mapping[local].id};
    }

    int ActionEncoder::directionIndex(Coord from, Coord to, const Board& board) {
        // If moving on top of an occupied cell (beetle), direction = 6
        if (!board.empty(to) || from == to) {
            return 6;
        }

        Coord diff = to - from;

        // Check if adjacent
        for (int i = 0; i < 6; ++i) {
            if (diff == DIRECTIONS[i]) return i;
        }

        // Non-adjacent move (grasshopper, ant, spider, ladybug): compute general direction
        // Use the direction that best matches the displacement
        // For grasshopper: the direction is exact along one axis
        // For others: pick the closest hex direction
        if (diff.q > 0 && diff.r == 0) return 0;  // East
        if (diff.q == 0 && diff.r > 0) return 1;   // SE
        if (diff.q < 0 && diff.r > 0) return 2;     // SW
        if (diff.q < 0 && diff.r == 0) return 3;    // West
        if (diff.q == 0 && diff.r < 0) return 4;    // NW
        if (diff.q > 0 && diff.r < 0) return 5;     // NE

        // Mixed direction: pick based on dominant component
        // Convert hex to approximate angle and pick closest direction
        float angle = std::atan2(static_cast<float>(diff.r), static_cast<float>(diff.q));
        // Normalize to 0-6 direction range
        int dir = static_cast<int>(std::round((angle + M_PI) / (M_PI / 3.0f))) % 6;
        return dir;
    }

    int ActionEncoder::findRefPieceIndex(const Board& board, Coord dest, Coord exclude) {
        // Find a neighboring piece at the destination as a reference
        auto neighbors = coordNeighbors(dest);
        for (const auto& n : neighbors) {
            if (n == exclude) continue;
            const Piece* p = board.top(n);
            if (p) return pieceToIndex(*p);
        }
        // If no neighbor found (shouldn't happen in valid game), also check the dest itself
        const Piece* p = board.top(dest);
        if (p) return pieceToIndex(*p);

        return 0; // fallback
    }

    int ActionEncoder::moveToAction(const Move& move, const GameState& state) {
        if (move.type == Move::Pass) {
            // Pass is encoded as action 0 (will be masked appropriately)
            return 0;
        }

        const Board& board = state.board();
        int srcIdx, dirIdx, refIdx;

        if (move.type == Move::Place) {
            srcIdx = pieceToIndex(move.piece);
            // For placement: direction is "on top" (6) if placing on occupied (shouldn't happen),
            // otherwise find the direction to the nearest neighbor
            dirIdx = 6; // Placement uses direction 6 as convention
            refIdx = findRefPieceIndex(board, move.to, Coord{-999, -999});
        } else { // PieceMove
            srcIdx = pieceToIndex(move.piece);
            dirIdx = directionIndex(move.from, move.to, board);
            refIdx = findRefPieceIndex(board, move.to, move.from);
        }

        return dirIdx * NUM_PIECE_TYPES * NUM_PIECE_TYPES + srcIdx * NUM_PIECE_TYPES + refIdx;
    }

    Move ActionEncoder::actionToMove(int action, const GameState& state) {
        int refIdx = action % NUM_PIECE_TYPES;
        action /= NUM_PIECE_TYPES;
        int srcIdx = action % NUM_PIECE_TYPES;
        int dirIdx = action / NUM_PIECE_TYPES;

        Piece srcPiece = indexToPiece(srcIdx);
        const Board& board = state.board();

        // Check if the source piece is on the board or in hand
        Coord srcCoord;
        bool onBoard = findPieceOnBoard(board, srcPiece, srcCoord);

        Move m;
        if (!onBoard) {
            // Placement move
            m.type = Move::Place;
            m.piece = srcPiece;

            // Find destination: an empty cell adjacent to the reference piece
            Piece refPiece = indexToPiece(refIdx);
            Coord refCoord;
            if (findPieceOnBoard(board, refPiece, refCoord)) {
                // Find empty neighbor adjacent to reference piece
                auto neighbors = coordNeighbors(refCoord);
                for (const auto& n : neighbors) {
                    if (board.empty(n)) {
                        m.to = n;
                        return m;
                    }
                }
            }
            m.to = Coord{0, 0}; // fallback for first move
        } else {
            // Movement move
            m.type = Move::PieceMove;
            m.piece = srcPiece;
            m.from = srcCoord;

            if (dirIdx < 6) {
                // Move in hex direction - for pieces that move far (ant, spider, grasshopper),
                // we need to find the exact legal move matching this action encoding
                Coord dest = srcCoord + DIRECTIONS[dirIdx];
                // Check if this is a valid destination; if not, search legal moves
                m.to = dest;
            } else {
                // On-top move (beetle)
                // Find destination from reference piece
                Piece refPiece = indexToPiece(refIdx);
                Coord refCoord;
                if (findPieceOnBoard(board, refPiece, refCoord)) {
                    m.to = refCoord;
                }
            }
        }

        return m;
    }

    torch::Tensor ActionEncoder::legalMask(const GameState& state) {
        auto mask = torch::zeros({ACTION_SPACE});
        auto acc = mask.accessor<float, 1>();

        auto moves = state.legalMoves();

        for (const auto& move : moves) {
            int action = moveToAction(move, state);
            if (action >= 0 && action < ACTION_SPACE) {
                acc[action] = 1.0f;
            }
        }

        // If no legal moves, the pass "action" should be allowed
        if (moves.empty()) {
            acc[0] = 1.0f;
        }

        return mask;
    }

} // namespace Hive::Learning
