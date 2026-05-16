#pragma once

#include <torch/torch.h>
#include <optional>
#include <unordered_set>
#include <utility>
#include "state.h"
#include "coords.h"
#include "alphaZeroEngine/config/headers/config.h"

// STATE ENCODER
// Converts a State into:
//   - planes:  tensor [NUM_CHANNELS, GRID_SIZE, GRID_SIZE]  (spatial features)
//   - scalars: tensor [NUM_SCALAR_FEATURES]                 (global features)
//
// The board is mapped from axial hex coordinates to a 26x26 grid, centered on
// the centroid of the current hive. Features are always from the perspective
// of the current player (channels 0-7 = my pieces, 8-15 = opponent pieces).
//
// Plane layout (19 planes):
//   0-7:  Current player's pieces by bug type (Q,B,S,G,A,L,M,P) - binary
//   8-15: Opponent's pieces by bug type - binary
//   16:   Stack height (normalized by 6.0)
//   17:   Color of top piece (1=mine, -1=opponent, 0=empty)
//   18:   Articulation points (pieces that can't be lifted - cached Tarjan)
//
// Scalar layout (3 scalars):
//   0: my queen adjacency (fraction of neighbors occupied)
//   1: opponent queen adjacency
//   2: my hand fullness (fraction of pieces remaining)

namespace Hive::Learning {

    struct EncodedState {
        torch::Tensor planes;   // [NUM_CHANNELS, GRID_SIZE, GRID_SIZE], float32
        torch::Tensor scalars;  // [NUM_SCALAR_FEATURES], float32
    };

    class StateEncoder {
    public:
        // Encode a game state. If artPointsCache is non-null, Tarjan is not
        // recomputed — the caller is responsible for providing a result that
        // matches `state.board()`. Used to share computation with the move
        // generator (which also computes articulation points).
        static EncodedState encode(
            const State& state,
            const std::unordered_set<Coord, CoordHash>* artPointsCache = nullptr);

        // Map axial coordinate to grid position, centered on the hive centroid.
        // Returns std::nullopt if the cell falls outside the GRID_SIZE box.
        static std::optional<std::pair<int, int>> axialToGrid(
            Coord coord, int centQ, int centR);

        // Centroid is exposed so the move/policy encoder can use the *same*
        // centering as the state encoder for a given state.
        static std::pair<int, int> computeCentroid(const State& state);
    };

} // namespace Hive::Learning
