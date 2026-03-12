#pragma once

#include <string>
#include <string_view>
#include <sstream>
#include "board.h"
#include "moves.h"
#include "coords.h"
#include "state.h"

namespace Hive {
    // Set as false for speeding up move insertion
    inline bool VALIDATE = true;

    // --- Basic String Utilities ---
    std::string trim(const std::string& s);
    std::vector<std::string> split_ws(const std::string& line);

    // --- UHP Translation Structures ---
    struct ParsedPieceName {
        Color color{Color::White};
        Bug bug{Bug::Queen};
        int index{1};
        bool hasIndex{false};
    };

    struct ParsedRequest {
        bool isPass{false};
        std::string pieceName;
        std::optional<ParsedPieceName> parsedPiece;
        std::optional<Coord> dest;
    };

    class UhpBoard {
    private:
        std::unordered_map<Coord, std::vector<std::string>, CoordHash> stacks;
        std::unordered_map<std::string, Coord> piecePos;

        static int maxIndexForBug(Bug bug);
        static std::string basePieceString(Color color, Bug bug);

    public:
        void clear();

        [[nodiscard]] bool occupied(const Coord &coord) const;
        [[nodiscard]] std::optional<std::string> topName(const Coord& coord) const;
        [[nodiscard]] bool hasPiece(const std::string& pieceName) const;
        [[nodiscard]] std::optional<Coord> whereIs(const std::string& pieceName) const;

        void push(const Coord& coord, const std::string& pieceName);
        std::optional<std::string> pop(const Coord& coord);
        bool moveTop(const Coord& from, const Coord& to);

        [[nodiscard]] std::string nextPieceName(Color color, Bug bug) const;
    };

    class UhpCodec {
    private:
        const State& state;
        const UhpBoard& uhpBoard;

        [[nodiscard]] std::optional<Coord> parseRelativePositionToken(const std::string& tok) const;
        [[nodiscard]] std::string destToRelativeToken(const Coord& dest) const;

    public:
        UhpCodec(const State& st, const UhpBoard& uhpboard) : state(st), uhpBoard(uhpboard) {}

        // Step 3: Intent Extraction (String -> Request)
        [[nodiscard]] std::optional<ParsedRequest> parseUhpRequest(const std::string& moveStr) const;

        // Step 5: Serialization (Move -> String)
        [[nodiscard]] std::string moveToUhpString(const Move& m) const;
    };

    // -----------------------------------------------------------------------
    // ----- The following is currently LEGACY CODE and will soon delete -----
    // -----------------------------------------------------------------------

    // Util to split a UHP command into chunks
    std::vector<std::string> splitCommand(const std::string& line);

    // Util to retrieve whether a piece is on the board or in hand
    bool findPieceOnBoard(const Board& board, const Piece& targetPiece, Coord& outCoord);

    // Converts a Piece into a valid UHP string
    inline std::string PieceToString(const Piece& piece);

    // Converts a UHP piece string into a defined Piece element
    inline Piece StringToPiece(const std::string_view str);

    // Converts a Coordinate (Piece + Direction) to a valid UHP string
    inline std::string CoordToString(const Coord& pieceCoord, const Coord& neighCoord, const std::string& neighName);

    // Converts a Move to a valid UHP string
    inline std::string MoveToString(const Move& move, const Board& board);

    // Helper to find a piece's coordinate on the board by scanning occupied cells
    bool findPieceOnBoard(const Board& board, const Piece& targetPiece, Coord& outCoord);

    // Converts a UHP move string to a Move
    Move StringToMove(const std::string& moveStr, const Board& board);
    
}