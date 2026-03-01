#pragma once

#include <torch/torch.h>
#include "state.h"
#include "config/headers/config.h"

// STATE ENCODER
// Converts a GameState into a tensor of shape [NUM_CHANNELS, GRID_SIZE, GRID_SIZE]
// for neural network input.
//
// The board is mapped from axial hex coordinates to a 26x26 grid, centered on the
// centroid of the current hive. Feature planes are always from the perspective of
// the current player (channels 0-7 = my pieces, 8-15 = opponent pieces).
//
// Channel layout (24 planes):
//   0-7:   Current player's pieces by bug type (Q,B,S,G,A,L,M,P) - binary
//   8-15:  Opponent's pieces by bug type - binary
//   16:    Stack height (normalized)
//   17:    Color of top piece (1=mine, -1=opponent, 0=empty)
//   18:    Legal placement targets (binary)
//   19:    My queen adjacency (fraction of neighbors occupied)
//   20:    Opponent queen adjacency
//   21:    Articulation points (pieces that can't move)
//   22:    Turn parity (all 1s if my turn, which is always the case)
//   23:    Hand fullness (fraction of pieces remaining, uniform plane)

namespace Hive::Learning {

    class StateEncoder {
    public:
        // Encode a game state into a tensor [NUM_CHANNELS, GRID_SIZE, GRID_SIZE]
        static torch::Tensor encode(const GameState& state);

    private:
        // Compute the centroid of the hive for centering the grid
        static std::pair<int, int> computeCentroid(const GameState& state);

        // Map axial coordinate to grid position, centered on centroid
        static std::pair<int, int> axialToGrid(Coord coord, int centQ, int centR);
    };

} // namespace Hive::Learning
