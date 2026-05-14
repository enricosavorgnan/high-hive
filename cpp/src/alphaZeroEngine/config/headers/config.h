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
    constexpr int NUM_CHANNELS = 24;        // Feature planes for state encoding

    // --- Action space ---
    // action = direction(7) x src_piece(28) x ref_piece(28) = 5488
    constexpr int NUM_DIRECTIONS = 7;       // 6 hex directions + 1 "on top" (beetle/place)
    constexpr int NUM_PIECE_TYPES = 28;     // 14 per player (Q, B1-2, S1-2, G1-3, A1-3, L, M, P)
    constexpr int ACTION_SPACE = NUM_DIRECTIONS * NUM_PIECE_TYPES * NUM_PIECE_TYPES; // 5488

    // --- Neural network architecture ---
    constexpr int NUM_FILTERS = 256;        // Channels in residual blocks
    constexpr int NUM_RESIDUAL_BLOCKS = 19; // Number of residual blocks
    constexpr int POLICY_CHANNELS = 2;      // Channels in policy head conv
    constexpr int VALUE_CHANNELS = 1;       // Channels in value head conv
    constexpr int VALUE_HIDDEN = 256;       // Hidden layer size in value head

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
    constexpr int BATCH_SIZE = 512;
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
