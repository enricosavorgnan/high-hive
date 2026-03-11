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

} // namespace Hive