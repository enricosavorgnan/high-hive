#pragma once

#include "moves.h"
#include "state.h"
#include <vector>
#include <string>

namespace Hive {

    // Abstract base class for all game engines (Random, Minimax, AlphaZero, etc.)
    class Engine {
    public:
        virtual ~Engine() = default;

        // The core method every engine must implement.
        // Receives the full game State (needed by MCTS for apply/undo).
        virtual Move getBestMove(const State& state, const std::vector<Move>& validMoves) = 0;
    };

    // A purely random mover for baseline testing
    class RandomEngine : public Engine {
    public:
        Move getBestMove(const State& state, const std::vector<Move>& validMoves) override;
    };

} // namespace Hive

// AlphaZeroEngine is defined in learning/alphazero_engine.h
// and only available when compiled with ENABLE_LEARNING