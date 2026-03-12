#include "headers/uhp.h"
#include <iostream>
#include <chrono>
#include <random>

#include "generator.h"

namespace Hive {

    void UhpHandler::loop() {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            std::vector<std::string> chunks = splitCommand(line);
            if (chunks.empty()) continue;

            const std::string& cmd = chunks[0];

            if (cmd == "u1") cmdU1();
            else if (cmd == "info") cmdInfo();
            else if (cmd == "newgame") cmdNewGame(chunks, line);
            else if (cmd == "play") cmdPlay(chunks, line);
            else if (cmd == "pass") cmdPass();
            else if (cmd == "validmoves") cmdValidMoves();
            else if (cmd == "bestmove") cmdBestMove(chunks);
            else if (cmd == "undo") cmdUndo();
            else if (cmd == "options") cmdOptions();
            else if (cmd == "exit") break;

            // Guarantee buffer flush to MZinga
            std::cout << std::flush;
        }
    }

    // --- State Generators ---

    std::string UhpHandler::generateGameString() const {
        // Get the current turn number from state.
        int turnNumber = (state.getCurrentPlayerTurn() / 2) +1; // State only saves the number of turns played IN TOTAL
        std::string s = gameType + ";" + gameState + ";" +
                        (state.toMove() == Color::White ? "White" : "Black") +
                        "[" + std::to_string(turnNumber) + "]";

        for (const auto& m : moveHistory) s += ";" + m;

        return s;
    }

    void UhpHandler::applyMove(const std::string& moveStr) {
        // 1. Check on move type
        if (moveStr == "pass") {
            Move move;
            move.type = Move::Pass;
            state.applyMove(move);
            return;
        } else {
            Move move = StringToMove(moveStr, state.board());
            state.applyMove(move);
        }

        // 2. Update internal state
        moveHistory.push_back(moveStr);
        gameState = "InProgress";
    }

    // ----- Command Handlers -----

    void UhpHandler::cmdU1() { std::cout << "ok\n"; }

    void UhpHandler::cmdInfo() {
        std::cout << "id high-hive-engine v0.1\n";
        std::cout << "Mosquito;Ladybug;Pillbug;\n";
        std::cout << "ok\n";
    }


    void UhpHandler::cmdNewGame(const std::vector<std::string>& chunks, const std::string& line) {
        // Reset state
        state = State();
        moveHistory.clear();
        gameState = "NotStarted";

        // Parse optional GameString
        if (chunks.size() > 1) {
            std::string gameString = line.substr(line.find(chunks[1]));

            // Re-tokenize by ';' to handle strict UHP GameStrings
            std::istringstream stream(gameString);
            std::string token;
            int tokenIndex = 0;

            while (std::getline(stream, token, ';')) {
                if (tokenIndex == 0) gameType = token;
                else if (tokenIndex == 1) gameState = token;
                else if (tokenIndex == 2) { /* Turn string - we infer this from moves applied */ }
                else {
                    applyMove(token); // Replay history onto the board
                }
                tokenIndex++;
            }
        }

        std::cout << generateGameString() << "\n";
        std::cout << "ok\n";
    }

    void UhpHandler::cmdPlay(const std::vector<std::string>& chunks, const std::string& line) {
        if (chunks.size() > 1) {
            // Extract the exact move string avoiding split manipulation errors
            std::string moveStr = line.substr(line.find(chunks[1]));

            applyMove(moveStr);

            std::cout << generateGameString() << "\n";
        }
        std::cout << "ok\n";
    }

    void UhpHandler::cmdPass() {
        // A pass is technically a move in UHP. We apply it directly.
        applyMove("pass");
        std::cout << generateGameString() << "\n";
        std::cout << "ok\n";
    }

    void UhpHandler::cmdValidMoves() const {
        std::vector<Move> validMoves = MoveGenerator::generateMoves(state);

        if (validMoves.empty() || (validMoves.size() == 1 && validMoves[0].type == Move::Pass)) {
            std::cout << "pass\n";
        } else {
            for (size_t i = 0; i < validMoves.size(); ++i) {
                if (validMoves[i].type == Move::Pass) continue;

                std::cout << MoveToString(validMoves[i], state.board());
                if (i < validMoves.size() - 1) {
                    std::cout << ";";
                }
            }
            std::cout << "\n";
        }
        std::cout << "ok\n";
    }

    void UhpHandler::cmdBestMove(const std::vector<std::string>& chunks) const {
        // assume bestmove time 00:00:05
        std::vector<Move> validMoves = MoveGenerator::generateMoves(state);

        if (validMoves.empty() || (validMoves.size() == 1 && validMoves[0].type == Move::Pass)) {
            std::cout << "pass\n";
            std::cout << "ok\n";
            return;
        }

        // Bridge to the legacy engine signature
        std::vector<Piece> availableHand = state.getUniqueAvailablePieces(state.toMove());
        Move bestMove = engine->getBestMove(state.board(), state.toMove(), availableHand, validMoves);

        std::cout << MoveToString(bestMove, state.board()) << "\n";
        std::cout << "ok\n";
    }

    void UhpHandler::cmdUndo() {
        if (!moveHistory.empty()) {
            state.undoLastMove(); // Mathematical O(1) reversion
            moveHistory.pop_back();
            if (moveHistory.empty()) {
                gameState = "NotStarted";
            }
        }
        std::cout << generateGameString() << "\n";
        std::cout << "ok\n";
    }

    void UhpHandler::cmdOptions() {
        // TODO
        // Reserved for engine options
        std::cout << "ok\n";
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
        return piecePos.find(pieceName) != piecePos.end();
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



} // namespace Hive