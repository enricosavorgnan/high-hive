#include "headers/engine.h"
#include <random>

namespace Hive {

    Move RandomEngine::getBestMove(const State& state, const std::vector<Move>& validMoves) {
        if (validMoves.empty()) {
            return {Move::Pass, {Color::White, Bug::Ant, 0}, {0,0}, {0,0}};
        }

        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<std::mt19937::result_type> dist(0, validMoves.size() - 1);

        return validMoves[dist(rng)];
    }

} // namespace Hive