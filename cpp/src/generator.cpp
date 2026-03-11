#include "headers/generator.h"

namespace Hive {
    std::vector<Move> MoveGenerator::generatePlacements(const Board &board, Color player, const std::vector<Piece> &hand) {
        return;
        // TODO: write function
    }


    std::vector<Move> MoveGenerator::generateMovements(const Board& board, Color player, std::optional<Coord> lastMovedPieceCoord) {
        return;
        // TODO: write function
    }

    std::vector<Move> MoveGenerator::generateMoves(const Board& board, Color turnPlayer, const std::vector<Piece>& hand, std::optional<Coord> lastMovedPieceCoord = std::nullopt) {
        std::vector<Move> placements = generatePlacements(board, turnPlayer, hand);

        std::vector<Move> movements = generateMovements(board, turnPlayer, lastMovedPieceCoord);

        placements.insert(placements.end(), movements.begin(), movements.end());

        return placements;
    }
}
