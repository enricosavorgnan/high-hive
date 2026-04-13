#pragma once

#include "moves.h"
#include "rules.h"
#include "state.h"

namespace Hive {

    class MoveGenerator {
        public:
            // Method that internally calls generatePlacements and generateMovements and returns all the moves found
            static std::vector<Move> generateMoves(const State& state);

        private:
            static std::vector<Move> generatePlacements(const State& state);
            static std::vector<Move> generateMovements(const State& state);
    };
}