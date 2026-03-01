#pragma once

#include "board.h"
#include <vector>

// Forward declaration - Move is defined in moves.h
// This breaks the circular dependency between rules.h and moves.h

namespace Hive {

    struct Move; // forward declaration

    class RuleEngine {
        public:
            // Method that internally calls generatePlacements and generateMovements and returns all the moves found
            static std::vector<Move> generateMoves(const Board& board, Color turnPlayer, const std::vector<Piece>& hand);

            // Method aimed to retrieve whether a piece can move from coordinate fromIdx to coordinate toIdx
            // Returns True if the move is valid, otherwise False
            static bool canSlide(const Board& board, int fromIdx, int toIdx);

            // Method for checking the One Hive Rule, i.e.,for retrieving whether a board is connected if a piece at coordinate idx is removed.
            // ATTENTION: Runs a BFS under-the-hood. It is slow.
            //
            // Returns True if:
            // - Size of the tile at index idx is >= 2 return True.
            // - Piece at index idx is a leaf in the graph, return True.
            // - All neighbors are visited in BFS, return True.
            // Otherwise, returns False
            static bool isBoardConnected(const Board& board, int idx);

        private:
            static std::vector<Move> generatePlacements(const Board& board, Color player, const std::vector<Piece>& hand);
            static std::vector<Move> generateMovements(const Board& board, Color player);
    };

}