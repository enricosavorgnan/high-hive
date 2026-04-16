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
        virtual std::string getName() = 0;
        virtual void setTimeBudget(int timeMs) {}
    };

    // A purely random mover for baseline testing
    class RandomEngine : public Engine {
    public:
        explicit RandomEngine(const std::string& modelPath="", int timeBudget=5000) {}
        Move getBestMove(const State& state, const std::vector<Move>& validMoves) override;
        std::string getName() override
        {
            return "RandomEngine";
        };
    };
} // namespace Hive