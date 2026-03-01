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

namespace Hive {

    class AlphaZeroEngine : public Engine {
    public:
        explicit AlphaZeroEngine(const std::string& modelPath, int simulations = Learning::MCTS_SIMS)
            : simulations_(simulations) {

            network_ = Learning::HiveNet();
            torch::load(network_, modelPath);
            network_->eval();

            if (torch::cuda::is_available()) {
                network_->to(torch::kCUDA);
            }

            mcts_ = std::make_unique<Learning::MCTS>(network_);
        }

        Move getBestMove(
            const Board& board, Color turnPlayer,
            const std::vector<Piece>& hand,
            const std::vector<Move>& validMoves) override {

            if (validMoves.empty()) {
                return {Move::Pass, {Color::White, Bug::Ant, 0}, {0, 0}, {0, 0}};
            }

            // Reconstruct GameState from board + turn info
            // (In a full integration, the UhpHandler would maintain a GameState directly)
            GameState state;
            // For now, we build a minimal state from the board
            // This is a simplification — ideally the GameState is passed through
            state.board() = board;

            // Run MCTS search
            auto moveVisits = mcts_->search(state, /*addNoise=*/false);

            if (moveVisits.empty()) {
                // Fallback to random
                return validMoves[0];
            }

            // Select best move (greedy)
            std::vector<int> visits;
            visits.reserve(moveVisits.size());
            for (const auto& [m, v] : moveVisits) {
                visits.push_back(v);
            }

            int bestIdx = Learning::MCTS::selectAction(visits, /*temperature=*/0.0f);
            const Move& bestMove = moveVisits[bestIdx].first;

            // Advance tree for next call
            int bestAction = Learning::ActionEncoder::moveToAction(bestMove, state);
            mcts_->advanceTree(bestAction);

            return bestMove;
        }

    private:
        Learning::HiveNet network_{nullptr};
        std::unique_ptr<Learning::MCTS> mcts_;
        int simulations_;
    };

} // namespace Hive
