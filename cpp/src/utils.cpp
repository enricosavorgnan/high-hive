#include "headers/utils.h"

namespace Hive
{
    // --- String Utilities ---
    std::string trim(const std::string& s) {
        size_t a = 0;
        while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        size_t b = s.size();
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
        return s.substr(a, b - a);
    }

    std::vector<std::string> split_ws(const std::string& line) {
        std::istringstream iss(line);
        std::vector<std::string> out;
        std::string tok;
        while (iss >> tok) out.push_back(tok);
        return out;
    }

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


    // --- Anonymous Parsing Helpers ---
    namespace {
        Coord dirFromUhpIndicator(bool isPrefix, char ind) {
            if (!isPrefix) {
                if (ind == '-') return DIRECTIONS[0]; // E
                if (ind == '\\') return DIRECTIONS[1]; // SE
                if (ind == '/') return DIRECTIONS[5]; // NE
            } else {
                if (ind == '/') return DIRECTIONS[2]; // SW
                if (ind == '-') return DIRECTIONS[3]; // W
                if (ind == '\\') return DIRECTIONS[4]; // NW
            }
            return Coord{0, 0};
        }

        std::string uhpPosTokenFromRefAndDest(const std::string& refPiece, int dirIdx) {
            switch (dirIdx) {
                case 0: return refPiece + "-";
                case 1: return refPiece + "\\";
                case 2: return std::string("/") + refPiece;
                case 3: return std::string("-") + refPiece;
                case 4: return std::string("\\") + refPiece;
                case 5: return refPiece + "/";
                default: break;
            }
            return refPiece;
        }

        std::optional<ParsedPieceName> parsePieceName(const std::string& s) {
            if (s.size() < 2) return std::nullopt;
            ParsedPieceName p;
            if (s[0] == 'w') p.color = Color::White;
            else if (s[0] == 'b') p.color = Color::Black;
            else return std::nullopt;

            switch (s[1]) {
                case 'Q': p.bug = Bug::Queen; break;
                case 'B': p.bug = Bug::Beetle; break;
                case 'S': p.bug = Bug::Spider; break;
                case 'G': p.bug = Bug::Grasshopper; break;
                case 'A': p.bug = Bug::Ant; break;
                case 'M': p.bug = Bug::Mosquito; break;
                case 'L': p.bug = Bug::Ladybug; break;
                case 'P': p.bug = Bug::Pillbug; break;
                default: return std::nullopt;
            }

            if (s.size() > 2) {
                int idx = 0;
                for (size_t i = 2; i < s.size(); ++i) {
                    if (!std::isdigit(static_cast<unsigned char>(s[i]))) return std::nullopt;
                    idx = idx * 10 + (s[i] - '0');
                }
                if (idx <= 0) return std::nullopt;
                p.index = idx;
                p.hasIndex = true;
            }
            return p;
        }
    }


    // ----- UHP Board Implementation -----
    void UhpBoard::clear() {
        stacks.clear();
        piecePos.clear();
    }

    bool UhpBoard::occupied(const Coord& coord) const {
        auto it = stacks.find(coord);
        return it != stacks.end() && !it->second.empty();
    }

    std::optional<std::string> UhpBoard::topName(const Coord& c) const {
        auto it = stacks.find(c);
        if (it == stacks.end() || it->second.empty()) return std::nullopt;
        return it->second.back();
    }

    bool UhpBoard::hasPiece(const std::string& pieceName) const {
        return piecePos.contains(pieceName);
    }

    std::optional<Coord> UhpBoard::whereIs(const std::string& pieceName) const {
        auto it = piecePos.find(pieceName);
        if (it == piecePos.end()) return std::nullopt;
        return it->second;
    }

    void UhpBoard::push(const Coord& to, const std::string& pieceName) {
        stacks[to].push_back(pieceName);
        piecePos[pieceName] = to;
    }

    std::optional<std::string> UhpBoard::pop(const Coord& from) {
        auto it = stacks.find(from);
        if (it == stacks.end() || it->second.empty()) return std::nullopt;

        std::string pieceName = it->second.back();
        it->second.pop_back();
        if (it->second.empty()) stacks.erase(it);

        piecePos.erase(pieceName);
        return pieceName;
    }

    bool UhpBoard::moveTop(const Coord& from, const Coord& to) {
        auto it = stacks.find(from);
        if (it == stacks.end() || it->second.empty()) return false;

        std::string pieceName = it->second.back();
        it->second.pop_back();
        if (it->second.empty()) stacks.erase(it);

        stacks[to].push_back(pieceName);
        piecePos[pieceName] = to;
        return true;
    }

    int UhpBoard::maxIndexForBug(Bug bug) {
        switch (bug) {
            case Bug::Queen: return 1;
            case Bug::Beetle: return 2;
            case Bug::Spider: return 2;
            case Bug::Grasshopper: return 3;
            case Bug::Ant: return 3;
            case Bug::Ladybug: return 1;
            case Bug::Mosquito: return 1;
            case Bug::Pillbug: return 1;
            default: return 1;
        }
    }

    std::string UhpBoard::basePieceString(Color c, Bug b) {
        char col = (c == Color::White) ? 'w' : 'b';
        char letter = '?';
        switch (b) {
            case Bug::Queen: letter = 'Q'; break;
            case Bug::Beetle: letter = 'B'; break;
            case Bug::Spider: letter = 'S'; break;
            case Bug::Grasshopper: letter = 'G'; break;
            case Bug::Ant: letter = 'A'; break;
            case Bug::Ladybug: letter = 'L'; break;
            case Bug::Mosquito: letter = 'M'; break;
            case Bug::Pillbug: letter = 'P'; break;
        }
        return std::string{col, letter};
    }

    std::string UhpBoard::nextPieceName(Color c, Bug b) const {
        int maxIdx = maxIndexForBug(b);
        std::string baseName = basePieceString(c, b);

        if (maxIdx == 1) return baseName; // Unique pieces don't get numbers (e.g., "wQ", not "wQ1")

        // Find the lowest unused integer for multiple pieces (e.g., "wA1", "wA2", "wA3")
        for (int idx = 1; idx <= maxIdx; ++idx) {
            std::string name = baseName + std::to_string(idx);
            if (!hasPiece(name)) return name;
        }
        return baseName + std::to_string(maxIdx); // Safe fallback
    }


    // ----- UHP Coded -----
    std::optional<Coord> UhpCodec::parseRelativePositionToken(const std::string& tok) const {
        if (tok.empty()) return std::nullopt;

        bool isPrefix = false;
        char ind = 0;
        std::string ref = tok;

        if (tok[0] == '-' || tok[0] == '/' || tok[0] == '\\') {
            isPrefix = true;
            ind = tok[0];
            ref = tok.substr(1);
        } else if (tok.back() == '-' || tok.back() == '/' || tok.back() == '\\') {
            isPrefix = false;
            ind = tok.back();
            ref = tok.substr(0, tok.size() - 1);
        } else {
            // No prefix/suffix means placing ON TOP of the reference piece
            ind = 0;
            ref = tok;
        }

        auto refCoordOpt = uhpBoard.whereIs(ref);
        if (!refCoordOpt) return std::nullopt;

        if (ind == 0) return *refCoordOpt;

        return *refCoordOpt + dirFromUhpIndicator(isPrefix, ind);
    }

    std::string UhpCodec::destToRelativeToken(const Coord& dest, std::optional<Coord> ignoreOrigin) const {
        auto neighbors = coordNeighbors(dest);
        for (const Coord& refCoord : neighbors) {
            if (ignoreOrigin.has_value() && refCoord == *ignoreOrigin) continue;

            auto refNameOpt = uhpBoard.topName(refCoord);
            if (!refNameOpt) continue;

            int dirIdx = neighborDirectionIndex(refCoord, dest);
            if (dirIdx < 0) continue;
            return uhpPosTokenFromRefAndDest(*refNameOpt, dirIdx);
        }

        // Fallback: This should mathematically never trigger on a valid connected board unless it's the 1st piece
        auto occ = state.board().occupiedCoords();
        if (!occ.empty()) {
            if (const auto refNameOpt = uhpBoard.topName(occ[0])) return *refNameOpt + "-";
        }
        return "wQ-";
    }

    std::optional<ParsedRequest> UhpCodec::parseUhpRequest(const std::string& moveStr) const {
        ParsedRequest pr;
        const std::string ms = trim(moveStr);
        if (ms == "pass") {
            pr.isPass = true;
            return pr;
        }

        auto toks = split_ws(ms);
        if (toks.empty()) return std::nullopt;

        pr.pieceName = toks[0];
        pr.parsedPiece = parsePieceName(pr.pieceName);
        if (!pr.parsedPiece) return std::nullopt;

        if (toks.size() == 1) {
            // First move of the game always goes to (0,0)
            if (!state.board().occupiedCoords().empty()) return std::nullopt;
            pr.dest = Coord{0, 0};
            return pr;
        }

        auto dOpt = parseRelativePositionToken(toks[1]);
        if (!dOpt) return std::nullopt;
        pr.dest = *dOpt;
        return pr;
    }

    std::string UhpCodec::moveToUhpString(const Move& m) const {
        if (m.type == Move::Pass) return "pass";

        if (m.type == Move::Place) {
            const std::string pieceName = uhpBoard.nextPieceName(m.piece.color, m.piece.bug);
            if (state.board().occupiedCoords().empty()) return pieceName;
            return pieceName + " " + destToRelativeToken(m.to, std::nullopt);
        }

        // Both Move::PieceMove and Move::Drag resolve identically in UHP string space
        auto moverNameOpt = uhpBoard.topName(m.from);
        if (!moverNameOpt) return "pass"; // Critical fallback

        const std::string moverName = *moverNameOpt;

        // If target is occupied (Beetle/Mosquito climb), UHP expects the name of the covered piece
        if (uhpBoard.occupied(m.to)) {
            if (const auto coveredNameOpt = uhpBoard.topName(m.to)) return moverName + " " + *coveredNameOpt;
        }

        return moverName + " " + destToRelativeToken(m.to, m.from);
    }


    // ----- LEGACY METHODS ----


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

        const Color color = (str[0] == 'w') ? Color::White : Color::Black;
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
                default: ;
            }
        }
        uint8_t id = (str.size() > 2) ? str[2]-'0' : 0;
        return {color, bug, id};
    }

    std::string CoordToString(const Hive::Coord& pieceCoord, const Hive::Coord& neighCoord, const std::string& neighName) {
        Coord diff = pieceCoord - neighCoord;

        if (diff == Coord{1, 0}) return neighName + "-";
        if (diff == Coord{-1, 0}) return "-" + neighName;
        if (diff == Coord{0, -1}) return "\\" + neighName;
        if (diff == Coord{0, 1}) return neighName + "\\";
        if (diff == Coord{1, -1}) return "/" + neighName;
        if (diff == Coord{-1, 1}) return neighName + "/";
        
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


    // Helper to find a piece's coordinate on the board by scanning occupied cells
    bool findPieceOnBoard(const Board& board, const Piece& targetPiece, Coord& outCoord) {
        for (Coord c : board.occupiedCoords()) {
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