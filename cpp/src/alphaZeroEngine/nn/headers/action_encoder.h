#pragma once

#include <torch/torch.h>
#include "state.h"
#include "moves.h"
#include "pieces.h"
#include "alphaZeroEngine/config/headers/config.h"

// ACTION ENCODER (spatial policy)
// Maps between Move objects and action indices in [0, ACTION_SPACE).
//
// Action layout:
//   index = plane * GRID_SIZE * GRID_SIZE + gy * GRID_SIZE + gx
// where:
//   plane 0..13   = piece P of side-to-move ends at (gy, gx) via Place or PieceMove
//   plane 14..27  = piece P dragged to (gy, gx) via pillbug / mosquito-as-pillbug
//                   (the dragged piece may be any color; plane index ignores color)
//   index == ACTION_SPACE - 1  = Pass move
//
// Plane index for a piece is determined by bug type + id only, not color:
//   Queen=0, Beetle1=1, Beetle2=2, Spider1=3, Spider2=4,
//   Grasshopper{1,2,3}=5..7, Ant{1,2,3}=8..10, Ladybug=11, Mosquito=12, Pillbug=13
//
// Centroid centering is shared with StateEncoder: the same hive centroid is
// used to map a Move's destination axial coord to a grid cell, so input and
// output live in the same frame.

namespace Hive::Learning {

    class ActionEncoder {
    public:
        // Map a Move to its action index. Out-of-grid destinations (extremely
        // spread positions where the centered grid can't hold the destination)
        // currently fall back to PASS_ACTION_INDEX — this is a known minor
        // ambiguity to monitor during pretraining.
        static int moveToAction(const Move& move, const State& state);

        // Brute-force decode: scan the legal moves for the one matching the
        // given action index. O(legal moves) ~ 100, called only on debug paths.
        static Move actionToMove(int action, const State& state);

        // Build the legal-move mask of shape [ACTION_SPACE]: 1.0 for each
        // legal move's encoded action, 0.0 elsewhere. If no moves are legal,
        // PASS_ACTION_INDEX is marked legal instead.
        static torch::Tensor legalMask(const State& state);

        // Same as legalMask but skips the internal generateMoves call by
        // accepting a pre-computed legal-moves vector. Used by MCTS to share
        // the move-generation result with the expansion phase that needs the
        // same vector — one generateMoves call per leaf instead of two.
        static torch::Tensor legalMaskFromMoves(
            const std::vector<Move>& legalMoves,
            const State& state);

        // Plane index (0-13) for a piece's identity, color-agnostic.
        static int pieceToPlane(const Piece& piece);

        // Convenience: pack (plane, gy, gx) into a flat action index.
        static inline int encodeAction(int plane, int gy, int gx) {
            return plane * GRID_SIZE * GRID_SIZE + gy * GRID_SIZE + gx;
        }
    };

} // namespace Hive::Learning
