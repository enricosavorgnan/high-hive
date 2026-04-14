#pragma once

#include "engine.h"
#include "state.h"
#include "alphaZeroEngine/nn/headers/neural_net.h"
#include "alphaZeroEngine/mcts/headers/mcts.h"

#include <string>
#include <memory>
#include <ranges>

// ALPHAZERO ENGINE
// Uses MCTS + HiveNet to choose moves via the Engine interface.
// Loads a trained model from a checkpoint file.

namespace Hive {
     class AlphaZeroEngine : public Engine {
         public:
              explicit AlphaZeroEngine(const std::string& modelPath, int timeBudget = 4500): budgetMs_(timeBudget){
                  network_ = Learning::HiveNet();

                  // Load to CPU first (checkpoint may have been saved on CUDA)
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


              Move getBestMove(const State& state, const std::vector<Move>& validMoves) override {
                  if (validMoves.empty()) {
                      return {Move::Pass, {Color::White, Bug::Ant, 0}, {0, 0}, {0, 0}};
                  }

                  // Copy state: MCTS applies/undoes moves during simulation
                  State mutableState = state;

                  // Fresh search each UHP call (no tree reuse across opponent turns)
                  mcts_->reset();

                  // Run time-budgeted MCTS search
                  auto moveVisits = mcts_->searchWithBudget(
                  mutableState, std::chrono::milliseconds(budgetMs_), false);

                  if (moveVisits.empty()) {
                      return validMoves[0];
                  }

                  // Select best move (greedy, temperature=0)
                  std::vector<int> visits;
                  visits.reserve(moveVisits.size());
                  for (const auto& v : moveVisits | std::views::values) {
                      visits.push_back(v);
                  }

                  int bestIdx = Learning::MCTS::selectAction(visits, 0.0f);
                  return moveVisits[bestIdx].first;
              }


              std::string getName() override {
                  return "AlphaZeroEngine";
              }

         private:
              Learning::HiveNet network_{nullptr};
              std::unique_ptr<Learning::MCTS> mcts_;
              int budgetMs_;
    };

} // namespace Hive
