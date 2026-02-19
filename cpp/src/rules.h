#pragma once

#include "board.h"
#include "moves.h"
#include <vector>

namespace Hive {

    class RuleEngine {
        public: 
            static std::vector<Move> generateMoves(const Board& board, Color turnPlayer, const std::vector<Piece>& hand);

        private:
            static bool canSlide(const Board& board, int fromIdx, int toIdx);
            static bool isBoardConnected(const Board& board, int idx);

            static void generatePlacements(const Board& board, Color player, const std::vector<Piece>& hand, std::vector<Move>& moves);
            static void generateMovements(const Board& board, Color player, std::vector<Move>& moves); 
            static void getBugMoves(const Board& board, int idx, Piece p, std::vector<Move>& moves);
    };

}