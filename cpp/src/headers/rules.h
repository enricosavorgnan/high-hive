#pragma once

#include "board.h"
#include "moves.h"
#include <vector>

namespace Hive {

    class RuleEngine {
        public: 
            static std::vector<Move> generateMoves(const Board& board, Color turnPlayer, const std::vector<Piece>& hand);

            static bool canSlide(const Board& board, int fromIdx, int toIdx);
        
        private:
            static bool isBoardConnected(const Board& board, int idx);

            static std::vector<Move> generatePlacements(const Board& board, Color player, const std::vector<Piece>& hand);
            
            static std::vector<Move> generateMovements(const Board& board, Color player); 
    };

}