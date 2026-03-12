#pragma once

#include "board.h"
#include "moves.h"
#include <vector>

// RULES DECLARATION
// The file declares the methods for retrieving the possible moves

namespace Hive {

    class RuleEngine {
        public:
            // Method aimed to retrieve whether a piece can move from coordinate fromIdx to coordinate toIdx
            // Returns True if the move is valid, otherwise False
            static bool canSlide(const Board& board, Coord from, Coord to, std::optional<Coord> ignoreProp = std::nullopt);

            // The method defines whether the move from exclude coordinate to target coordinates does not break the One Hive Rule,
            // leaving the piece in target coordinate far from other pieces.
            static bool touchesHive(const Board& board, Coord target, Coord exclude);

            // Method for checking the One Hive Rule, i.e.,for retrieving whether a board is connected if a piece at coordinate idx is removed.
            // ATTENTION: Runs a BFS under-the-hood. It is slow.
            //
            // Returns True if:
            // - Size of the tile at index idx is >= 2 return True.
            // - Piece at index idx is a leaf in the graph, return True.
            // - All neighbors are visited in BFS, return True.
            // Otherwise, returns False
            static bool isBoardConnected(const Board& board, Coord coord);

            // Precomputes all articulation points in O(V) time using Tarjan's Algorithm.
            static std::unordered_set<Coord, CoordHash> getArticulationPoints(const Board& board);

            // Replaces the old 'isBoardConnected'. Operates in O(1) time.
            static bool canLiftPiece(const Board& board, Coord coord, const std::unordered_set<Coord, CoordHash>& articulationPoints);
    };

}