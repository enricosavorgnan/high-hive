#pragma once

#include <string>
#include <sstream>
#include "board.h"
#include "moves.h"
#include "coords.h"

namespace Hive {
    // Util to split a UHP command into chunks
    std::vector<std::string> splitCommand(const std::string& line);

    // Util to retrieve whether a piece is on the board or in hand
    bool findPieceOnBoard(const Board& board, const Piece& targetPiece, Coord& outCoord);

    // Converts a Piece into a valid UHP string
    std::string PieceToString(const Piece& piece);

    // Converts a UHP piece string into a defined Piece element
    Piece StringToPiece(const std::string_view str);

    // Converts a Coordinate (Piece + Direction) to a valid UHP string
    std::string CoordToString(const Coord& pieceCoord, const Coord& neighCoord, const std::string& neighName);

    // Converts a Move to a valid UHP string
    std::string MoveToString(const Move& move, const Board& board);

    // Converts a UHP move string to a Move
    Move StringToMove(const std::string& moveStr, const Board& board);
    
}