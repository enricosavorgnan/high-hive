#include "headers/engine.h"
#include <chrono>
#include <thread>
#include <random>

namespace Hive {

    Move RandomEngine::getBestMove(const State& state, const std::vector<Move>& validMoves) {
        if (validMoves.empty()) {
            // Return a pass move if absolutely no moves are available
            return {Move::Pass, {Color::White, Bug::Ant, 0}, {0,0}, {0,0}};
        }


        // Initialize random number generator
        // A non-deterministic seed is used.
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<std::mt19937::result_type> dist(0, validMoves.size() - 1);

        return validMoves[dist(rng)];
    }


    Move AlphaZeroEngine::getBestMove(const State& state, const std::vector<Move>& validMoves) {

        if (validMoves.empty()) {
            return {Move::Pass, {Color::White, Bug::Ant, 0}, {0, 0}, {0, 0}};
        }

        // Copy state: MCTS applies/undoes moves during simulation
        State mutableState = state;

        // Fresh search each UHP call (no tree reuse across opponent turns)
        mcts_->reset();

        // Run time-budgeted MCTS search
        auto moveVisits = mcts_->searchWithBudget(
            mutableState, std::chrono::milliseconds(timeBudget_), false);

        if (moveVisits.empty()) {
            return validMoves[0];
        }

        // Select best move (greedy, temperature=0)
        std::vector<int> visits;
        visits.reserve(moveVisits.size());
        for (const auto& [m, v] : moveVisits) {
            visits.push_back(v);
        }

        int bestIdx = Learning::MCTS::selectAction(visits, /*temperature=*/0.0f);
        return moveVisits[bestIdx].first;
    }

} // namespace Hive