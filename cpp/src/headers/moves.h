#pragma once

#include "coords.h"
#include "pieces.h"
#include "board.h"

#include <unordered_set>
#include <deque>
#include <algorithm>

// This header declares the structure Move and the bugs moves.
// The implementation of the bugs moves mainly follows the one in the Python implementation.

namespace Hive {

    // struct Move can have 3 possible types:
    // - Place: The move consists in taking a piece from the hand and placing it on the board
    // - PieceMove: The move consists in taking a piece already placed in the board and move into some other coordinates
    // - Pass: The move consists in a pass. It can occur if, e.g., the player have no moves left
    struct Move {
        enum Type{
            Place,
            PieceMove,
            Pass,
            Drag
        } type;
        Piece piece;            // for Place
        Coord from;             // for Move
        Coord to;               // for Place and Move
        Coord pillbug;          // pillbug operating the Drag move
    };

    namespace Moves {
        // Ant Move Coordinates
        void getAntMoves(const Board& board, Coord prop, std::vector<Coord>& targets);
        // Beetle Move Coordinates
        void getBeetleMoves(const Board& board, Coord prop, std::vector<Coord>& targets);
        // Grasshopper Move Coordinates
        void getGrasshopperMoves(const Board& board, Coord prop, std::vector<Coord>& targets);
        // Ladybug Move Coordinates
        void getLadybugMoves(const Board& board, Coord prop, std::vector<Coord>& targets);

        // Mosquito Move Coordinates
        void getMosquitoMoves(const Board& board, Coord prop, std::vector<Coord>& targets, std::optional<Coord> lastMovedPieceCoord, std::vector<std::pair<Coord, Coord>>& dragTargets, const std::unordered_set<Coord, CoordHash>& articulationPoints);

        // Pillbug Move Coordinates
        void getPillbugMoves(const Board& board, Coord prop, std::vector<Coord>& targets, std::optional<Coord> lastMovedPieceCoord, std::vector<std::pair<Coord, Coord>>& dragTargets, const std::unordered_set<Coord, CoordHash>& articulationPoints);
        // Pillbug Drag Move Coordinates
        void getPillbugDragMoves(const Board& board, Coord prop, std::optional<Coord> lastMovedPieceCoord, std::vector<std::pair<Coord, Coord>>& dragTargets, const std::unordered_set<Coord, CoordHash>& articulationPoints);

        // Queen Bee Move Coordinates
        void getQueenMoves(const Board& board, Coord prop, std::vector<Coord>& targets);
        // Spider Move Coordinates
        void getSpiderMoves(const Board& board, Coord prop, std::vector<Coord>& targets);
    }
    
}