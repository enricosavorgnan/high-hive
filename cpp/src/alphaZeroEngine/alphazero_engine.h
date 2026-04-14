#pragma once

#include "engine.h"
#include "state.h"
#include "nn/headers/neural_net.h"
#include "mcts/headers/mcts.h"
#include "nn/headers/state_encoder.h"
#include "nn/headers/action_encoder.h"
#include "config/headers/config.h"

#include <string>
#include <memory>

// ALPHAZERO ENGINE
// Uses MCTS + HiveNet to choose moves via the Engine interface.
// Loads a trained model from a checkpoint file.

// This below will be commented since the engine is defined in engine.cpp
// namespace Hive {
//
//     class AlphaZeroEngine : public Engine {
//     public:
//         explicit AlphaZeroEngine(const std::string& modelPath, int budgetMs = 3500)
//             : budgetMs_(budgetMs) {
//
//             network_ = Learning::HiveNet();
//
//             // Load to CPU first (checkpoint may have been saved on CUDA)
//             torch::serialize::InputArchive archive;
//             archive.load_from(modelPath, torch::kCPU);
//             network_->load(archive);
//             network_->eval();
//
//             if (torch::cuda::is_available()) {
//                 network_->to(torch::kCUDA);
//                 network_->to(torch::kHalf);
//             }
//
//             mcts_ = std::make_unique<Learning::MCTS>(network_);
//         }
//
//         Move getBestMove(const State& state, const std::vector<Move>& validMoves) override {
//
//             if (validMoves.empty()) {
//                 return {Move::Pass, {Color::White, Bug::Ant, 0}, {0, 0}, {0, 0}};
//             }
//
//             // Copy state: MCTS applies/undoes moves during simulation
//             State mutableState = state;
//
//             // Fresh search each UHP call (no tree reuse across opponent turns)
//             mcts_->reset();
//
//             // Run time-budgeted MCTS search
//             auto moveVisits = mcts_->searchWithBudget(
//                 mutableState, std::chrono::milliseconds(budgetMs_), /*addNoise=*/false);
//
//             if (moveVisits.empty()) {
//                 return validMoves[0];
//             }
//
//             // Select best move (greedy, temperature=0)
//             std::vector<int> visits;
//             visits.reserve(moveVisits.size());
//             for (const auto& [m, v] : moveVisits) {
//                 visits.push_back(v);
//             }
//
//             int bestIdx = Learning::MCTS::selectAction(visits, /*temperature=*/0.0f);
//             return moveVisits[bestIdx].first;
//         }
//
//     private:
//         Learning::HiveNet network_{nullptr};
//         std::unique_ptr<Learning::MCTS> mcts_;
//         int budgetMs_;
//     };

// } // namespace Hive
