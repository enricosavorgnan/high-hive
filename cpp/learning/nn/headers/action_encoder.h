#pragma once

#include <torch/torch.h>
#include "state.h"
#include "moves.h"
#include "config/headers/config.h"

// ACTION ENCODER
// Maps between Move objects and action indices in [0, ACTION_SPACE).
//
// Encoding scheme:
//   action = direction_idx * NUM_PIECE_TYPES * NUM_PIECE_TYPES
//            + src_piece_idx * NUM_PIECE_TYPES
//            + ref_piece_idx
//
// direction_idx (7): 0-5 = hex directions, 6 = "on top" (beetle climb / placement)
// src_piece_idx (28): 0-13 = white pieces, 14-27 = black pieces
//   Order per color: Q, B1, B2, S1, S2, G1, G2, G3, A1, A2, A3, L1, M1, P1
// ref_piece_idx (28): same ordering, represents the reference/neighbor piece
//   For placements: the piece adjacent to the destination
//   For moves: the piece at or adjacent to the destination
//   Pass move uses a special index

namespace Hive::Learning {

    class ActionEncoder {
    public:
        // Convert a Move to an action index
        static int moveToAction(const Move& move, const GameState& state);

        // Convert an action index back to a Move
        static Move actionToMove(int action, const GameState& state);

        // Generate a legal move mask tensor [ACTION_SPACE] (1.0 = legal, 0.0 = illegal)
        static torch::Tensor legalMask(const GameState& state);

        // Convert a piece to its index (0-27)
        static int pieceToIndex(const Piece& piece);

        // Convert an index (0-27) back to a Piece
        static Piece indexToPiece(int idx);

    private:
        // Find a reference piece for encoding a move destination
        static int findRefPieceIndex(const Board& board, Coord dest, Coord exclude);

        // Determine direction index between two coordinates
        static int directionIndex(Coord from, Coord to, const Board& board);
    };

} // namespace Hive::Learning
