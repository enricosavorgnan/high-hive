#include "headers/uhp.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

namespace Hive {

    void UhpHandler::loop() {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            std::vector<std::string> chunks = splitCommand(line);
            if (chunks.empty()) continue;

            const std::string& cmd = chunks[0];

            if (cmd == "u1") {
                cmdU1();
            }
            else if (cmd == "info") {
                cmdInfo();
            }
            else if (cmd == "newgame") {
                cmdNewGame(chunks, line);
            }
            else if (cmd == "play") {
                cmdPlay(chunks, line);
            }
            else if (cmd == "pass") {
                cmdPass();
            }
            else if (cmd == "validmoves") {
                cmdValidMoves();
            }
            else if (cmd == "bestmove") {
                cmdBestMove(chunks);
            }
            else if (cmd == "undo") {
                cmdUndo();
            }
            else if (cmd == "options") {
                cmdOptions();
            }
            else if (cmd == "exit") {
                break;
            }

            // Guarantee buffer flush to MZinga
            std::cout << std::flush;
        }
    }

    // --- State Generators ---

    std::vector<Piece> UhpHandler::getHand(Color player) const {
        std::vector<Piece> startingHand = {
            {player, Bug::Queen, 1},
            {player, Bug::Spider, 1}, {player, Bug::Spider, 2},
            {player, Bug::Beetle, 1}, {player, Bug::Beetle, 2},
            {player, Bug::Grasshopper, 1}, {player, Bug::Grasshopper, 2}, {player, Bug::Grasshopper, 3},
            {player, Bug::Ant, 1}, {player, Bug::Ant, 2}, {player, Bug::Ant, 3},
            {player, Bug::Mosquito, 1},
            {player, Bug::Ladybug, 1},
            {player, Bug::Pillbug, 1}
        };

        std::vector<Piece> currentHand;
        Coord dummy;
        for (const auto& p : startingHand) {
            // Only add to hand if it is NOT found on the board
            if (!findPieceOnBoard(board, p, dummy)) {
                currentHand.push_back(p);
            }
        }
        return currentHand;
    }

    std::string UhpHandler::generateGameString() const {
        std::string s = gameType + ";" + gameState + ";" +
                        (turnPlayer == Color::White ? "White" : "Black") +
                        "[" + std::to_string(turnNumber) + "]";
        for (const auto& m : moveHistory) {
            s += ";" + m;
        }
        return s;
    }

    void UhpHandler::applyMove(const std::string&moveStr) {
        // 1. Parse string to move object
        Move move = StringToMove(moveStr, board);

        // 2. Apply to board memory
        if (move.type == Move::Place) {
            board.place(move.to, move.piece);
        } else if (move.type == Move::PieceMove) {
            board.move(move.from, move.to);
        }

        // 3. Update internal state
        moveHistory.push_back(moveStr);
        gameState = "InProgress";

        if (turnPlayer == Color::Black) {
            turnNumber++;
            turnPlayer = Color::White;
        } else {
            turnPlayer = Color::Black;
        }
    }

    // ----- Command Handlers -----

    void UhpHandler::cmdU1() {
        std::cout << "ok\n";
    }

    void UhpHandler::cmdInfo() {
        std::cout << "id high-hive-engine v0.1\n";
        std::cout << "Mosquito;Ladybug;Pillbug;\n";
        std::cout << "ok\n";
    }


    void UhpHandler::cmdNewGame(const std::vector<std::string>& chunks, const std::string& line) {
        // Reset state
        board = Board();
        moveHistory.clear();
        turnNumber = 1;
        turnPlayer = Color::White;
        gameState = "NotStarted";

        // Parse optional GameString
        if (chunks.size() > 1) {
            std::string gameString = line.substr(line.find(chunks[1]));
            std::vector<std::string> gameChunks = splitCommand(gameString); // Splitting by ';' may be needed here based on UHP

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
        moveHistory.push_back("pass");

        if (turnPlayer == Color::Black) {
            turnNumber++;
            turnPlayer = Color::White;
        } else {
            turnPlayer = Color::Black;
        }

        std::cout << generateGameString() << "\n";
        std::cout << "ok\n";
    }

    void UhpHandler::cmdValidMoves() const {
        std::vector<Piece> hand = getHand(turnPlayer);
        std::vector<Move> validMoves = RuleEngine::generateMoves(board, turnPlayer, hand);

        if (validMoves.empty()) {
            std::cout << "pass\n";
        } else {
            for (size_t i = 0; i < validMoves.size(); ++i) {
                std::cout << MoveToString(validMoves[i], board);
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
        std::vector<Piece> hand = getHand(turnPlayer);
        std::vector<Move> validMoves = RuleEngine::generateMoves(board, turnPlayer, hand);

        if (validMoves.empty()) {
            std::cout << "pass\n";
            std::cout << "ok\n";
            return;
        }

        Move bestMove = engine->getBestMove(board, turnPlayer, hand, validMoves);

        std::cout << MoveToString(bestMove, board) << "\n";
        std::cout << "ok\n";
    }

    void UhpHandler::cmdUndo() {
        // TODO
        std::cout << "ok\n";
    }

    void UhpHandler::cmdOptions() {
        // TODO
        // Reserved for engine options
    }

} // namespace Hive