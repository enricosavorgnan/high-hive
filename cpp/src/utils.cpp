#include "headers/utils.h"

namespace Hive
{
    // Helper to split string by spaces to mimic Python's line.split()
    std::vector<std::string> splitCommand(const std::string& line) {
        std::vector<std::string> chunks;
        std::istringstream stream(line);
        std::string chunk;
        while (stream >> chunk) {
            chunks.push_back(chunk);
        }
        return chunks;
    }

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


    // Helper to find a piece's coordinate on the board by scanning occupied cells
    bool findPieceOnBoard(const Board& board, const Piece& targetPiece, Coord& outCoord) {
        for (Coord c : board.occupiedCoords()) {
            int idx = Board::AxToIndex(c);
            // We only need to check the top of the stack for movement origins,
            // but for references, UHP allows referencing covered pieces.
            // Assuming the board's CellStack has a way to iterate or we just check the top for now.
            const Piece* topPiece = board.top(c);
            if (topPiece && topPiece->color == targetPiece.color && topPiece->bug == targetPiece.bug && topPiece->id == targetPiece.id) {
                outCoord = c;
                return true;
            }
        }
        return false;
    }

    Move StringToMove(const std::string& moveStr, const Board& board) {
        if (moveStr == "pass") {
            return {Move::Pass, {Color::White, Bug::Ant, 0}, {0,0}, {0,0}};
        }

        auto spaceIdx = moveStr.find(' ');
        std::string pieceStr = moveStr.substr(0, spaceIdx);
        Piece piece = StringToPiece(pieceStr);

        Move move;
        move.piece = piece;

        Coord fromCoord;
        bool isMove = findPieceOnBoard(board, piece, fromCoord);
        move.type = isMove ? Move::PieceMove : Move::Place;
        move.from = isMove ? fromCoord : Coord{0, 0};

        if (spaceIdx == std::string::npos) {
            // First move of the game (no reference piece)
            move.to = {0, 0};
            return move;
        }

        std::string refStr = moveStr.substr(spaceIdx + 1);
        Coord offset{0, 0};
        std::string refPieceStr;

        // Map UHP relative position characters to your axial coordinate logic
        if (refStr.front() == '-') { offset = {-1, 0}; refPieceStr = refStr.substr(1); }
        else if (refStr.front() == '/') { offset = {1, -1}; refPieceStr = refStr.substr(1); }
        else if (refStr.front() == '\\') { offset = {0, -1}; refPieceStr = refStr.substr(1); }
        else if (refStr.back() == '-') { offset = {1, 0}; refPieceStr = refStr.substr(0, refStr.size() - 1); }
        else if (refStr.back() == '/') { offset = {-1, 1}; refPieceStr = refStr.substr(0, refStr.size() - 1); }
        else if (refStr.back() == '\\') { offset = {0, 1}; refPieceStr = refStr.substr(0, refStr.size() - 1); }
        else {
            // No prefix/suffix means placing directly ON TOP of the reference piece (Beetle/Mosquito)
            offset = {0, 0};
            refPieceStr = refStr;
        }

        Piece refPiece = StringToPiece(refPieceStr);
        Coord refCoord;
        if (!findPieceOnBoard(board, refPiece, refCoord)) {
            // In a robust engine, handle invalid UHP strings gracefully
            throw std::invalid_argument("Reference piece not found on board");
        }

        move.to = refCoord + offset;
        return move;
    }


} // namespace Hive
