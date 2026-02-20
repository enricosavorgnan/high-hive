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

        // Enforce the strict 5-second constraint
        auto timeLimit = std::chrono::seconds(5);
        auto startTime = std::chrono::steady_clock::now();

        while (true) {
            auto currentTime = std::chrono::steady_clock::now();
            if (currentTime - startTime >= timeLimit) {
                break;
            }
            // Yield the thread to prevent 100% CPU lockup during the waiting period
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Initialize random number generator
        // Note: Using a non-deterministic seed (std::random_device) here to ensure
        // the RandomEngine actually plays different games. For strict ML debugging,
        // you may later change this to a fixed seed (e.g., std::mt19937 rng(42);).
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<std::mt19937::result_type> dist(0, validMoves.size() - 1);

        return validMoves[dist(rng)];
    }

} // namespace Hive