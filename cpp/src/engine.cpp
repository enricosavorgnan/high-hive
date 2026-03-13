#include "headers/engine.h"
#include <chrono>
#include <thread>
#include <random>

namespace Hive {

    Move RandomEngine::getBestMove(const Board& board, Color turnPlayer, const std::vector<Piece>& hand, const std::vector<Move>& validMoves) {
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

} // namespace Hive