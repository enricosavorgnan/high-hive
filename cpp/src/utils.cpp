#include "headers/utils.h"

namespace Hive
{
    std::string PieceToString(const Piece& piece)
    {
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

    Piece StringToPiece(const std::string_view str) {
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

    std::string CoordToString(const Hive::Coord& pieceCoord, const Hive::Coord& neighCoord, const std::string& neighName) {
        Hive::Coord diff = pieceCoord - neighCoord;

        if (diff == Hive::Coord{1, 0}) return neighName + "-";
        if (diff == Hive::Coord{-1, 0}) return "-" + neighName;
        if (diff == Hive::Coord{0, -1}) return "\\" + neighName;
        if (diff == Hive::Coord{0, 1}) return neighName + "\\";
        if (diff == Hive::Coord{1, -1}) return "/" + neighName;
        if (diff == Hive::Coord{-1, 1}) return neighName + "/";
        
        return ""; // No valid direction found
    } 

    std::string MoveToString(const Hive::Move& move, const Hive::Board& board) {
        // ---- Pass -----
        if (move.type == Hive::Move::Pass) return "pass";

        // ----- Place & Move -----
        std::string str = PieceToString(move.piece);

        // First Move Check 
        // No references
        if (move.type == Hive::Move::Place && board.occupiedCoords().empty()) {
            return str; 
        }

        for (int i = 0; i < 6; ++i) {
            Hive::Coord neigh = move.to + Hive::DIRECTIONS[i];

            if (move.type == Hive::Move::PieceMove && neigh == move.from) continue;

            const Hive::Piece* referencePiece = board.top(neigh);
            if (referencePiece != nullptr) {
                std::string referenceName = Hive::PieceToString(*referencePiece);
                std::string referenceStr = Hive::CoordToString(move.to, neigh, referenceName);
                if (!referenceStr.empty()) {
                    return str + " " + referenceStr;
                }

            }
        }

        // Fallback return
        return str;
    }


} // namespace Hive
