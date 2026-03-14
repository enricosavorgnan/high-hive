#pragma once

#include "board.h"
#include "moves.h"
#include <vector>
#include <unordered_set>
#include <optional>

namespace Hive {

    class RuleEngine {
        public:
            // Checks if a piece can slide from one cell to another (Freedom to Move rule)
            static bool canSlide(const Board& board, Coord from, Coord to, std::optional<Coord> ignoreProp = std::nullopt);

            // Checks if a cell touches the hive (has at least one occupied neighbor, ignoring prop)
            static bool touchesHive(const Board& board, Coord target, Coord prop);

            // One Hive check: board stays connected if piece at coord is removed
            static bool isBoardConnected(const Board& board, Coord coord);

            // Tarjan's algorithm: find all articulation points
            static std::unordered_set<Coord, CoordHash> getArticulationPoints(const Board& board);

            // Check if a piece can be lifted (stacked or not an articulation point)
            static bool canLiftPiece(const Board& board, Coord coord, const std::unordered_set<Coord, CoordHash>& articulationPoints);
    };

}
