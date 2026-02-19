#pragma once

#include "coords.h"
#include "pieces.h"

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
    
}