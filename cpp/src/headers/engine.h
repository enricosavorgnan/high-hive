#pragma once

#include "moves.h"
#include "state.h"
#include <vector>
#include <string>

#include "alphaZeroEngine/nn/headers/action_encoder.h"
#include "alphaZeroEngine/nn/headers/neural_net.h"
#include "alphaZeroEngine/mcts/headers/mcts.h"


namespace Hive {

    // Abstract base class for all game engines (Random, Minimax, AlphaZero, etc.)
    class Engine {
    public:
        virtual ~Engine() = default;

        // The core method every engine must implement.
        // Receives the full game State (needed by MCTS for apply/undo).
        virtual Move getBestMove(const State& state, const std::vector<Move>& validMoves) = 0;
        virtual std::string getName() = 0;
    };

    // A purely random mover for baseline testing
    class RandomEngine : public Engine {
    public:
        Move getBestMove(const State& state, const std::vector<Move>& validMoves) override;
        std::string getName() override
        {
            return "Random";
        };
    };

    class AlphaZeroEngine : public Engine {
    public:
        explicit AlphaZeroEngine(const std::string& modelPath, int timeBudget = 3500) : timeBudget_(timeBudget) {

            network_ = Learning::HiveNet();

            torch::serialize::InputArchive archive;
            archive.load_from(modelPath, torch::kCPU);
            network_->load(archive);
            network_->eval();

            if (torch::cuda::is_available()) {
                network_->to(torch::kCUDA);
                network_->to(torch::kHalf);
            }

            mcts_ = std::make_unique<Learning::MCTS>(network_);
        }

    private:
        Learning::HiveNet network_{nullptr};
        std::unique_ptr<Learning::MCTS> mcts_{};
        int timeBudget_;

    public:
        Move getBestMove(const State& state, const std::vector<Move>& validMoves) override;
        std::string getName() override
        {
            return "AlphaZero";
        };
    };

} // namespace Hive

// AlphaZeroEngine is defined in learning/alphazero_engine.h
// and only available when compiled with ENABLE_LEARNING