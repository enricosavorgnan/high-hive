#pragma once

#include "board.h"
#include "moves.h"
#include "rules.h"

namespace Hive {

    class MoveGenerator {
        public:
            // Method that internally calls generatePlacements and generateMovements and returns all the moves found
            static std::vector<Move> generateMoves(const Board& board, Color turnPlayer, const std::vector<Piece>& hand, std::optional<Coord> lastMovedPieceCoord = std::nullopt);

        private:
            // TODO
            static std::vector<Move> generatePlacements(const Board& board, Color player, const std::vector<Piece>& hand);
            // TODO
            static std::vector<Move> generateMovements(const Board& board, Color player, std::optional<Coord> lastMovedPieceCoord);
    };
}