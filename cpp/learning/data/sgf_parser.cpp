#include "data/headers/sgf_parser.h"
#include "nn/headers/state_encoder.h"
#include "utils.h"
#include "rules.h"
#include "moves.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <regex>
#include <algorithm>

namespace Hive::Learning {

    SgfGameInfo SgfParser::parseFile(const std::string& filepath) {
        SgfGameInfo info;

        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Cannot open SGF file: " << filepath << "\n";
            return info;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

        // Extract properties using regex
        auto extractProp = [&](const std::string& prop) -> std::string {
            std::regex re(prop + "\\[([^\\]]*)\\]");
            std::smatch match;
            if (std::regex_search(content, match, re)) {
                return match[1].str();
            }
            return "";
        };

        info.white = extractProp("PW");
        info.black = extractProp("PB");
        info.result = extractProp("RE");
        info.gameType = extractProp("GN");

        // Extract moves: look for W[] and B[] properties
        // SGF for Hive uses move nodes like ;W[wA1 /wG1] ;B[bQ -wA1]
        std::regex moveRe(";(W|B)\\[([^\\]]*)\\]");
        auto it = std::sregex_iterator(content.begin(), content.end(), moveRe);
        auto end = std::sregex_iterator();

        for (; it != end; ++it) {
            std::string moveStr = (*it)[2].str();
            if (!moveStr.empty()) {
                info.moves.push_back(moveStr);
            }
        }

        return info;
    }

    std::string SgfParser::sgfMoveToUhp(const std::string& sgfMove) {
        // SGF moves from boardspace.net are largely UHP compatible
        // Main differences:
        //   - "dropb" prefix for drops might occur in some formats
        //   - Direction notation may differ slightly
        // For most boardspace SGFs, the move string is already UHP-format
        std::string move = sgfMove;

        // Trim whitespace
        auto start = move.find_first_not_of(" \t\n\r");
        auto stop = move.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        move = move.substr(start, stop - start + 1);

        // Handle "pass"
        if (move == "pass" || move == "PASS") return "pass";

        return move;
    }

    float SgfParser::parseResult(const std::string& result) {
        if (result.empty()) return 0.0f;

        std::string lower = result;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.find("white") != std::string::npos || lower.find("w+") != std::string::npos) {
            return 1.0f;
        }
        if (lower.find("black") != std::string::npos || lower.find("b+") != std::string::npos) {
            return -1.0f;
        }
        // Draw or unknown
        return 0.0f;
    }

    std::vector<TrainingSample> SgfParser::processGame(const SgfGameInfo& game) {
        std::vector<TrainingSample> samples;

        if (game.moves.empty()) return samples;

        // Parse game outcome
        float whiteOutcome = parseResult(game.result);
        if (whiteOutcome == 0.0f && game.result.empty()) {
            return samples; // Skip games with unknown result
        }

        GameState state;

        for (const auto& sgfMove : game.moves) {
            std::string uhpMove = sgfMoveToUhp(sgfMove);
            if (uhpMove.empty()) continue;

            // Handle pass
            if (uhpMove == "pass") {
                Move passMove;
                passMove.type = Move::Pass;

                // Record sample before applying
                TrainingSample sample;
                sample.state = StateEncoder::encode(state);
                // For supervised training, policy is one-hot on the actual move played
                sample.policy = torch::zeros({ACTION_SPACE});
                // Pass has no meaningful action encoding; skip policy for pass
                sample.value = (state.toMove() == Color::White) ? whiteOutcome : -whiteOutcome;
                samples.push_back(std::move(sample));

                state.apply(passMove);
                continue;
            }

            // Get legal moves to validate
            auto legalMoves = state.legalMoves();

            // Parse the move
            Move move;
            try {
                move = StringToMove(uhpMove, state.board());
            } catch (...) {
                // Parse error — discard entire game
                return {};
            }

            // Validate: check if this move is in the legal moves
            bool isLegal = false;
            for (const auto& lm : legalMoves) {
                if (lm.type == move.type && lm.to == move.to) {
                    if (move.type == Move::Place && lm.piece.bug == move.piece.bug) {
                        isLegal = true;
                        move = lm; // Use the legal move's exact piece (correct id)
                        break;
                    } else if (move.type == Move::PieceMove && lm.from == move.from) {
                        isLegal = true;
                        move = lm;
                        break;
                    }
                }
            }

            if (!isLegal) {
                // Illegal move — discard entire game
                return {};
            }

            // Record training sample
            TrainingSample sample;
            sample.state = StateEncoder::encode(state);

            // One-hot policy on the played move
            sample.policy = torch::zeros({ACTION_SPACE});
            int action = ActionEncoder::moveToAction(move, state);
            if (action >= 0 && action < ACTION_SPACE) {
                sample.policy[action] = 1.0f;
            }

            sample.value = (state.toMove() == Color::White) ? whiteOutcome : -whiteOutcome;
            samples.push_back(std::move(sample));

            // Apply the move
            state.apply(move);
        }

        return samples;
    }

    std::vector<TrainingSample> SgfParser::processDirectory(const std::string& dirPath) {
        std::vector<TrainingSample> allSamples;
        int totalGames = 0, validGames = 0, totalSamples = 0;

        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (entry.path().extension() == ".sgf") {
                ++totalGames;

                auto gameInfo = parseFile(entry.path().string());

                // Filter: only Base+MLP games
                // (Relaxed check: accept if game type contains relevant keywords or is empty)
                auto samples = processGame(gameInfo);

                if (!samples.empty()) {
                    ++validGames;
                    totalSamples += static_cast<int>(samples.size());
                    allSamples.insert(allSamples.end(),
                        std::make_move_iterator(samples.begin()),
                        std::make_move_iterator(samples.end()));
                }

                if (totalGames % 100 == 0) {
                    std::cout << "Processed " << totalGames << " games ("
                              << validGames << " valid, "
                              << totalSamples << " samples)\n";
                }
            }
        }

        std::cout << "SGF processing complete: " << totalGames << " total, "
                  << validGames << " valid (" << (100.0f * validGames / std::max(1, totalGames))
                  << "%), " << totalSamples << " training samples\n";

        return allSamples;
    }

} // namespace Hive::Learning
