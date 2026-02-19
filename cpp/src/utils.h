#pragma once

#include <string>
#include <sstream>
#include "board.h"
#include "moves.h"

namespace Hive {

    inline std::string PieceToString(const Piece& piece) {
        std::string str = (piece.color == Color::White) ? "w" : "b";
        
        if (piece.bug == Bug::Ant) str += "A";
        else if (piece.bug == Bug::Beetle) str += "B";
        else if (piece.bug == Bug::Grasshopper) str += "G";
        else if (piece.bug == Bug::Ladybug) str += "L";
        else if (piece.bug == Bug::Mosquito) str += "M";
        else if (piece.bug == Bug::Pillbug) str += "P";
        else if (piece.bug == Bug::Queen) str += "Q";
        else if (piece.bug == Bug::Spider) str += "S";

        if (piece.id > 0) {
            str += std::to_string(piece.id);
        }
        return str;
    }

    inline Piece StringToPiece(const std::string_view str) {
        // Safety Check
        if (str.empty() || (str[0] != 'w' && str[0] != 'b')) {
            throw std::invalid_argument("Invalid piece string format: " + std::string(str));
        }

        Color color = (str[0] == 'w') ? Color::White : Color::Black;
        Bug bug = Bug::Ant;
        if (str.size() > 1) {
            switch(str[1]) {
                case 'Q': bug = Bug::Queen; break;
                case 'S': bug = Bug::Spider; break;
                case 'B': bug = Bug::Beetle; break;
                case 'G': bug = Bug::Grasshopper; break;
                case 'A': bug = Bug::Ant; break;
                case 'L': bug = Bug::Ladybug; break;
                case 'M': bug = Bug::Mosquito; break;
                case 'P': bug = Bug::Pillbug; break;
            }
        }
        uint8_t id = (str.size() > 2) ? str[2]-'0' : 0;
        return {color, bug, id};
    }

    inline std::string CoordToString(const Coord& pieceCoord, const Coord& neighCoord, const std::string& neighName) {
        Coord diff = pieceCoord - neighCoord;

        if (diff == Coord{1, 0}) return neighName + "-";
        if (diff == Coord{-1, 0}) return "-" + neighName;
        if (diff == Coord{0, -1}) return "\\" + neighName;
        if (diff == Coord{0, 1}) return neighName + "\\";
        if (diff == Coord{1, -1}) return "/" + neighName;
        if (diff == Coord{-1, 1}) return neighName + "/";
        
        return ""; // No valid direction found
    }

    inline std::string MoveToString(const Move& move, const Board& board) {
        // ---- Pass -----
        if (move.type == Move::Pass) return "pass";

        // ----- Place & Move -----
        std::string str = PieceToString(move.piece);

        // First Move Check 
        // No references
        if (move.type == Move::Place && board.occupiedCoords().empty()) {
            return str; 
        }

        for (int i = 0; i < 6; ++i) {
            Coord neigh = move.to + DIRECTIONS[i];

            if (move.type == Move::PieceMove && neigh == move.from) continue;

            const Piece* referencePiece = board.top(neigh);
            if (referencePiece != nullptr) {
                std::string referenceName = PieceToString(*referencePiece);
                std::string referenceStr = CoordToString(move.to, neigh, referenceName);
                if (!referenceStr.empty()) {
                    return str + " " + referenceStr;
                }

            }
        }

        // Fallback return
        return str;
    }
        
}