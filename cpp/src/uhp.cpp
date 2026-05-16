#include "headers/uhp.h"
#include <iostream>
#include <chrono>
#include <random>

#include "generator.h"

namespace Hive {

    void UhpHandler::loop() {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

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
            else if (cmd == "perft") cmdPerft(chunks); // ADD THIS ROUTING
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

    int UhpHandler::applyMove(const std::string& moveStr, bool validate) {
        UhpCodec codec(state, uhpBoard);
        auto pr0pt = codec.parseUhpRequest(moveStr);

        if (!pr0pt) return false;
        const auto& pr = *pr0pt;

        // 1. Handle Passing
        if (pr.isPass) {
            if (validate) {
                auto moves = MoveGenerator::generateMoves(state);
                // Passing is mathematically illegal if any physical moves exist
                if (!moves.empty() && !(moves.size() == 1 && moves[0].type == Move::Pass)) {
                    return false;
                }
            }
            Move passMove;
            passMove.type = Move::Pass;
            state.applyMove(passMove);
            moveHistory.push_back("pass");
            gameState = "InProgress";
            return true;
        }

        if (!pr.dest || !pr.parsedPiece) return false;
        const Coord dest = *pr.dest;
        const ParsedPieceName pp = *pr.parsedPiece;


        // 2. Generate the Game Tree Node to find the Semantic Match
        auto moves = MoveGenerator::generateMoves(state);
        std::optional<Move> chosenMove = std::nullopt;

        auto fromOpt = uhpBoard.whereIs(pr.pieceName);
        if (!fromOpt) {
            // The piece is not on the board yet, so this must be a Placement intent
            for (const auto& mv : moves) {
                if (mv.type != Move::Place) continue;
                if (mv.piece.color != pp.color) continue;
                if (mv.piece.bug != pp.bug) continue;
                if (mv.to != dest) continue; // Ensure the geometric vector matches

                chosenMove = mv;
                break;
            }
        } else {
            // The piece is on the board, so this must be a Movement or Drag intent
            Coord from = *fromOpt;
            auto topName = uhpBoard.topName(from);

            // UHP strictly requires moving the piece that is physically on top of the stack
            if (!topName || *topName != pr.pieceName) return false;

            for (const auto& mv : moves) {
                if (mv.type == Move::Place || mv.type == Move::Pass) continue;
                if (mv.from != from) continue;
                if (mv.to != dest) continue;

                // We found the exact move. If this was a Drag, mv now contains the orchestrator data.
                chosenMove = mv;
                break;
            }
        }

        if (!chosenMove) {
            // The opponent bot sent an illegal move (or a desync occurred)
            return false;
        }

        // 3. Commit the Validated Move to both memory structures
        const Move cm = *chosenMove;
        state.applyMove(cm); // Updates the physical mathematical grid
        if (cm.type == Move::Place) {
            uhpBoard.push(cm.to, pr.pieceName); // Updates the text layer mapping
        } else {
            uhpBoard.moveTop(cm.from, cm.to);
        }
        moveHistory.push_back(moveStr);
        gameState = "InProgress";
        return true;
    }

    // ----- Command Handlers -----

    void UhpHandler::cmdU1() { std::cout << "ok\n"; }

    void UhpHandler::cmdInfo() const
    {
        std::cout << "id high-hive-cpp v1.0 "<< engine->getName() << "\n";
        std::cout << "Mosquito;Ladybug;Pillbug\n";
        std::cout << "ok\n";
    }


    void UhpHandler::cmdNewGame(const std::vector<std::string>& chunks, const std::string& line) {
        // Reset state
        state = State();
        uhpBoard.clear();
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
                    if (!applyMove(token, false)) {
                        std::cerr << "err Engine failed to parse history token: " << token << "\n";
                        state = State(); // Wipe corrupted board
                        uhpBoard.clear();
                        moveHistory.clear();
                        gameState = "NotStarted";
                        return;
                    }
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

            bool success = applyMove(moveStr, true);

            // CRITICAL: Catch and log alternate reality desyncs
            if (!success) {
                std::cerr << "[FATAL DESYNC] MoveGenerator failed to map Nokamute's move: " << moveStr << "\n";
            }

            std::cout << generateGameString() << "\n";
        }
        std::cout << "ok\n";
    }

    void UhpHandler::cmdPass() {
        // A pass is technically a move in UHP. We apply it directly.
        applyMove("pass", true);
        std::cout << generateGameString() << "\n";
        std::cout << "ok\n";
    }

    void UhpHandler::cmdValidMoves() const {
        std::vector<Move> validMoves = MoveGenerator::generateMoves(state);
        UhpCodec codec(state, uhpBoard); // Utilize the established codec

        if (validMoves.empty() || (validMoves.size() == 1 && validMoves[0].type == Move::Pass)) {
            std::cout << "pass\n";
        } else {
            for (size_t i = 0; i < validMoves.size(); ++i) {
                if (validMoves[i].type == Move::Pass) continue;

                std::cout << codec.moveToUhpString(validMoves[i]); // Strict serialization
                if (i < validMoves.size() - 1) {
                    std::cout << ";";
                }
            }
            std::cout << "\n";
        }
        std::cout << "ok\n";
    }

    void UhpHandler::cmdBestMove(const std::vector<std::string>& chunks) const {
        // Set time value of engine to the second chunk:
        if (chunks.size() > 2 && chunks[1] == "time")
        {
            const std::string time = chunks[2];
            int hours, minutes, seconds;
            sscanf(time.c_str(), "%d:%d:%d", &hours, &minutes, &seconds);
            int totSeconds = hours * 3600 + minutes * 60 + seconds;
            engine->setTimeBudget(totSeconds*1000);        // totSecond is in sec, engines in ms
        }

        const std::vector<Move> validMoves = MoveGenerator::generateMoves(state);
        const UhpCodec codec(state, uhpBoard);

        if (validMoves.empty() || (validMoves.size() == 1 && validMoves[0].type == Move::Pass)) {
            std::cout << "pass\n";
            std::cout << "ok\n";
            return;
        }

        const Move bestMove = engine->getBestMove(state, validMoves);

        // --- Efficient Permanent Logging ---
        auto& stream = const_cast<std::ofstream&>(logStream);

        if (engine->isVerbose()) {
            // Dynamically choose the output stream: File if open, otherwise Terminal (cerr)
            std::ostream* outStream = logStream.is_open() ? &logStream : &std::cerr;
            auto statsOpt = engine->getLastSearchStats();
            if (statsOpt) {
                const auto& stats = *statsOpt;

                *outStream << "\n========== [Turn: " << moveHistory.size() / 2 + 1 << "] ==========\n";
                *outStream << "Total Nodes Explored : " << stats.nodesExplored << "\n";
                *outStream << "Max Search Depth     : " << stats.maxDepth << "\n";
                *outStream << "--- Top 5 Moves Evaluated ---\n";

                auto evals = stats.moveEvaluations;
                // size_t topN = std::min(evals.size(), size_t(5));
                size_t topN = std::min(evals.size(), size_t(40));


                std::partial_sort(evals.begin(), evals.begin() + topN, evals.end(),
                    [](const MoveEval& a, const MoveEval& b) {
                        return a.visits > b.visits;
                    });

                for (size_t i = 0; i < topN; ++i) {
                    const auto& e = evals[i];
                    if (e.visits == 0) continue;
                    *outStream << "Move: " << codec.moveToUhpString(e.move)
                               << " | Visits: " << e.visits
                               << " | Q-Value: " << e.qValue
                               << " | Prior: " << e.priorProb << "\n";
                }
                *outStream << "===============================\n\n";

                outStream->flush();
            }
        }

        std::cout << codec.moveToUhpString(bestMove) << "\n";
        std::cout << "ok\n";
    }

    void UhpHandler::cmdUndo() {
        if (!moveHistory.empty()) {
            moveHistory.pop_back();

            // Rebuild the mathematical State and textual dictionaries from scratch
            // to guarantee absolute synchronization.
            state = State();
            uhpBoard.clear();
            gameState = "NotStarted";

            // Temporarily store the history to replay it
            std::vector<std::string> tempHistory = moveHistory;
            moveHistory.clear();

            for (const std::string& moveStr : tempHistory) {
                applyMove(moveStr, false); // Replay without deep tree validation
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


    // --- Performance Test (Perft) ---

    namespace {
        // Recursive helper isolated to this compilation unit
        uint64_t runPerft(State& state, int depth) {
            if (depth == 0) return 1ULL;

            std::vector<Move> moves = MoveGenerator::generateMoves(state);

            // Micro-optimization: avoid applying/undoing at the frontier
            if (depth == 1) return moves.size();

            uint64_t nodes = 0;
            for (const Move& m : moves) {
                state.applyMove(m);
                nodes += runPerft(state, depth - 1);
                state.undoLastMove();
            }
            return nodes;
        }
    }

    void UhpHandler::cmdPerft(const std::vector<std::string>& chunks) {
        if (chunks.size() < 2) {
            std::cout << "err Usage: perft <depth>\n";
            return;
        }

        int depth = 0;
        try {
            depth = std::stoi(chunks[1]);
        } catch (...) {
            std::cout << "err Invalid depth parameter\n";
            return;
        }

        if (depth <= 0) {
            std::cout << "Nodes: 0\nok\n";
            return;
        }

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<Move> validMoves = MoveGenerator::generateMoves(state);
        uint64_t totalNodes = 0;

        UhpCodec codec(state, uhpBoard);

        // Standard "Divide" Perft implementation
        for (const Move& move : validMoves) {
            // Encode string BEFORE applying the move, so it matches the current uhpBoard text layer
            std::string moveStr = codec.moveToUhpString(move);

            state.applyMove(move);
            uint64_t nodes = runPerft(state, depth - 1);
            state.undoLastMove();

            std::cout << moveStr << ": " << nodes << "\n";
            totalNodes += nodes;
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        std::cout << "\nTotal Nodes: " << totalNodes << "\n";
        std::cout << "Elapsed Time: " << elapsed.count() << " s\n";
        if (elapsed.count() > 0) {
            std::cout << "NPS: " << static_cast<uint64_t>(totalNodes / elapsed.count()) << "\n";
        }
        std::cout << "ok\n";
    }

} // namespace Hive