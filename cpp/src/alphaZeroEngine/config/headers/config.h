#pragma once

#include "pieces.h"

// All hyperparameters for the AlphaZero learning pipeline.

namespace Hive::Learning {

    // --- Bug type helpers (for encoding) ---
    constexpr int NUM_BUG_TYPES = 8;

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
        return 0;
    }

    constexpr Bug bugFromIndex(int i) {
        constexpr Bug mapping[] = {
            Bug::Queen, Bug::Beetle, Bug::Spider, Bug::Grasshopper,
            Bug::Ant, Bug::Ladybug, Bug::Mosquito, Bug::Pillbug
        };
        return mapping[i];
    }

    // --- Board encoding ---
    constexpr int GRID_SIZE = 26;           // Spatial dimension of the encoded board
    constexpr int NUM_CHANNELS = 19;        // Feature planes for state encoding
    constexpr int NUM_SCALAR_FEATURES = 3;  // Per-state scalars fed into value head:
                                            // queen adj me, queen adj opp, hand fullness me

    // --- Action space (spatial policy) ---
    // Output = (POLICY_PLANES x GRID_SIZE x GRID_SIZE) + 1 pass logit
    // Planes 0..13:  piece P of side-to-move ends at (y, x) via place / normal move
    // Planes 14..27: piece P dragged to (y, x) via pillbug / mosquito-as-pillbug
    // Last index:    pass move
    constexpr int POLICY_PIECE_PLANES = 14;
    constexpr int POLICY_PLANES = 2 * POLICY_PIECE_PLANES; // 28: normal + drag
    constexpr int ACTION_SPACE = POLICY_PLANES * GRID_SIZE * GRID_SIZE + 1; // 18929
    constexpr int PASS_ACTION_INDEX = ACTION_SPACE - 1;    // 18928
    constexpr int DRAG_PLANE_OFFSET = POLICY_PIECE_PLANES; // 14

    // --- Neural network architecture ---
    constexpr int NUM_FILTERS = 128;        // Channels in residual blocks
    constexpr int NUM_RESIDUAL_BLOCKS = 10; // Number of residual blocks
    constexpr int VALUE_CHANNELS = 1;       // Channels in value head conv
    constexpr int VALUE_HIDDEN = 128;       // Hidden layer size in value head

    // --- MCTS ---
    constexpr int MCTS_SIMS = 800;          // Number of MCTS simulations per move
    constexpr int MCTS_BATCH = 16;          // Parallel leaves per batched NN forward pass
    constexpr float VIRTUAL_LOSS = 1.0f;    // Per-pending-sim penalty applied during selection
    constexpr float C_PUCT = 2.5f;          // Exploration constant in PUCT formula
    constexpr float DIRICHLET_ALPHA = 0.15f;// Dirichlet noise parameter
    constexpr float DIRICHLET_EPSILON = 0.25f; // Noise mixing weight
    constexpr float TEMP_THRESHOLD = 15;    // Move number after which temperature drops
    constexpr float TEMP_HIGH = 1.0f;       // Temperature for early moves
    constexpr float TEMP_LOW = 0.1f;        // Temperature for later moves

    // --- Training ---
    // Sized for a ~10 GiB MIG slice on Demetra's `lovelace` partition (shared
    // with other tenants). Bumping back toward 512 needs a full A100 slice.
    constexpr int BATCH_SIZE = 128;
    constexpr int REPLAY_BUFFER_SIZE = 500000;
    constexpr float LEARNING_RATE = 0.01f;
    constexpr float WEIGHT_DECAY = 1e-4f;
    constexpr float MOMENTUM = 0.9f;
    constexpr int TRAIN_STEPS_PER_ITER = 1000;
    constexpr int SELF_PLAY_GAMES = 256;
    constexpr int EVAL_GAMES = 400;
    constexpr float EVAL_THRESHOLD = 0.55f; // Win rate to promote new model

    // --- Self-play ---
    constexpr int MAX_GAME_LENGTH = 100;    // Safety cutoff (tournament: 100 half-moves)

    // --- Pre-training ---
    constexpr int PRETRAIN_EPOCHS = 30;
    constexpr float PRETRAIN_LR = 0.001f;

} // namespace Hive::Learning
