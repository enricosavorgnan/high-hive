#pragma once

#include "coords.h"
#include "pieces.h"
#include "board.h"
#include "rules.h"

#include <unordered_set>
#include <deque>
#include <algorithm>

namespace Hive {

    struct Move {
        enum Type{
            Place,
            PieceMove,
            Pass
        } type;
        Piece piece; // for Place
        Coord from;  // for Move
        Coord to;    // for Place and Move
    };

    namespace Moves {
        static bool touchesHive(const Board& board, Coord target, Coord exclude);

        void getAntMoves(const Board& board, Coord prop, std::vector<Move>& targets);
        void getBeetleMoves(const Board& board, Coord prop, std::vector<Move>& targets);
        void getGrasshopperMoves(const Board& board, Coord prop, std::vector<Move>& targets);
        void getLadybugMoves(const Board& board, Coord prop, std::vector<Move>& targets);
        void getMosquitoMoves(const Board& board, Coord prop, std::vector<Move>& targets);
        void getPillbugMoves(const Board& board, Coord prop, std::vector<Move>& targets);
        void getQueenMoves(const Board& board, Coord prop, std::vector<Move>& targets);
        void getSpiderMoves(const Board& board, Coord prop, std::vector<Move>& targets);   
    }
    
}