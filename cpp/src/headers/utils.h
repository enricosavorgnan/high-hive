#pragma once

#include <string>
#include <sstream>
#include "board.h"
#include "moves.h"
#include "coords.h"

namespace Hive {

    inline std::string PieceToString(const Piece& piece);

    inline Piece StringToPiece(const std::string_view str);

    inline std::string CoordToString(const Coord& pieceCoord, const Coord& neighCoord, const std::string& neighName);

    inline std::string MoveToString(const Move& move, const Board& board)
    
}