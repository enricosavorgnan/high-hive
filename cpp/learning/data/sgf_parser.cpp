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
#include <cctype>

namespace Hive::Learning {

    // Split a string by whitespace into tokens
    static std::vector<std::string> tokenize(const std::string& s) {
        std::vector<std::string> tokens;
        std::istringstream iss(s);
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);
        return tokens;
    }

    // Unescape SGF backslash escaping: \\\\ → backslash
    static std::string sgfUnescape(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size() && s[i+1] == '\\') {
                result += '\\';
                ++i;
            } else {
                result += s[i];
            }
        }
        return result;
    }

    static std::string toLowerStr(const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), ::tolower);
        return r;
    }

    // Add color prefix if missing (old boardspace format: "S1" → "wS1")
    static std::string addColorPrefix(const std::string& piece, int player) {
        if (!piece.empty() && (piece[0] == 'w' || piece[0] == 'b')) return piece;
        return (player == 0 ? "w" : "b") + piece;
    }

    // Normalize a boardspace reference to UHP format:
    //  - strip trailing '.' (stacking notation)
    //  - add "1" to single-instance expansion pieces (P, L, M) missing a digit
    static std::string normalizeRef(const std::string& ref) {
        std::string r = ref;

        // Strip trailing '.' (boardspace stacking → UHP bare piece name)
        if (!r.empty() && r.back() == '.') {
            r.pop_back();
        }

        // Add "1" to single-instance expansion pieces: wP→wP1, bL→bL1, wM→wM1
        for (size_t i = 0; i + 1 < r.size(); ++i) {
            if ((r[i] == 'w' || r[i] == 'b') &&
                (r[i+1] == 'P' || r[i+1] == 'L' || r[i+1] == 'M')) {
                size_t after = i + 2;
                if (after >= r.size() || !std::isdigit(static_cast<unsigned char>(r[after]))) {
                    r.insert(after, "1");
                }
            }
        }

        return r;
    }

    SgfGameInfo SgfParser::parseFile(const std::string& filepath) {
        SgfGameInfo info;

        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Cannot open SGF file: " << filepath << "\n";
            return info;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

        // --- Header extraction ---

        // Player names: P0[id "Name"] and P1[id "Name"]
        std::regex p0Re(R"re(P0\[id "([^"]+)"\])re");
        std::regex p1Re(R"re(P1\[id "([^"]+)"\])re");
        std::smatch match;
        if (std::regex_search(content, match, p0Re)) info.white = match[1].str();
        if (std::regex_search(content, match, p1Re)) info.black = match[1].str();

        // Result: RE[...]
        std::regex reRe(R"(RE\[([^\]]*)\])");
        if (std::regex_search(content, match, reRe)) info.result = match[1].str();

        // Game type: SU[...]
        std::regex suRe(R"(SU\[([^\]]*)\])");
        if (std::regex_search(content, match, suRe)) info.gameType = match[1].str();

        // Resolve result: boardspace uses "X won by PlayerName" style
        // Convert to "white wins" / "black wins" for parseResult()
        {
            std::string lower = info.result;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("won") != std::string::npos || lower.find("wygrana") != std::string::npos) {
                // Check if the winning player is P0 (white) or P1 (black)
                if (!info.white.empty() && info.result.find(info.white) != std::string::npos) {
                    info.result = "white wins";
                } else if (!info.black.empty() && info.result.find(info.black) != std::string::npos) {
                    info.result = "black wins";
                }
                // If neither name matched, leave as-is for parseResult() fallback
            }
        }

        // --- Move extraction ---

        // Capture player index (0/1) and action content
        std::regex actionRe(R"(P([01])\[([^\]]*)\])");
        auto it = std::sregex_iterator(content.begin(), content.end(), actionRe);
        auto end = std::sregex_iterator();

        std::string pendingPiece;

        for (; it != end; ++it) {
            int player = ((*it)[1].str() == "0") ? 0 : 1;
            std::string actionStr = (*it)[2].str();
            auto tokens = tokenize(actionStr);
            if (tokens.size() < 2) continue;

            // Case-insensitive action matching (old format uses lowercase)
            std::string action = toLowerStr(tokens[1]);

            if (action == "start" || action == "done" || action == "edit") {
                pendingPiece.clear();
                continue;
            }

            if (action == "resign") {
                break;
            }

            if (action == "pass") {
                info.moves.push_back("pass");
                continue;
            }

            // Pick: [seq, "pick", color, handIdx, pieceId]
            if (action == "pick") {
                if (tokens.size() >= 5) {
                    pendingPiece = addColorPrefix(tokens[4], player);
                }
                continue;
            }

            // Pickb: [seq, "pickb", col, row, piece]
            if (action == "pickb") {
                if (tokens.size() >= 5) {
                    pendingPiece = addColorPrefix(tokens[4], player);
                }
                continue;
            }

            // Dropb / Pdropb: [seq, "dropb"/"pdropb", piece, col, row, ref]
            if (action == "dropb" || action == "pdropb") {
                std::string piece = addColorPrefix(tokens[2], player);
                if (tokens.size() >= 6) {
                    std::string uhp = convertToUhp(piece, tokens[5]);
                    if (!uhp.empty()) info.moves.push_back(uhp);
                } else if (tokens.size() >= 5) {
                    // No ref — first piece placement
                    std::string uhp = convertToUhp(piece, ".");
                    if (!uhp.empty()) info.moves.push_back(uhp);
                }
                pendingPiece.clear();
                continue;
            }

            // Move / PMove: [seq, "move"/"pmove", color, piece, col, row, ref]
            if (action == "move" || action == "pmove") {
                if (tokens.size() >= 7) {
                    std::string piece = addColorPrefix(tokens[3], player);
                    std::string uhp = convertToUhp(piece, tokens[6]);
                    if (!uhp.empty()) info.moves.push_back(uhp);
                }
                pendingPiece.clear();
                continue;
            }

            // Skip unknown actions
        }

        return info;
    }

    std::string SgfParser::convertToUhp(const std::string& piece, const std::string& ref) {
        if (ref == "." || ref.empty()) return piece;  // first placement
        std::string norm = normalizeRef(sgfUnescape(ref));
        return piece + " " + norm;
    }

    std::string SgfParser::sgfMoveToUhp(const std::string& sgfMove) {
        // Moves are already in UHP format from parseFile()
        // Just trim whitespace
        std::string move = sgfMove;
        auto start = move.find_first_not_of(" \t\n\r");
        auto stop = move.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        move = move.substr(start, stop - start + 1);

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
        if (lower.find("draw") != std::string::npos) {
            return 0.0f;
        }
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
        int noResult = 0, noMoves = 0, parseError = 0, illegalMove = 0;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
            if (entry.path().extension() == ".sgf") {
                ++totalGames;

                auto gameInfo = parseFile(entry.path().string());

                if (gameInfo.moves.empty()) {
                    ++noMoves;
                    continue;
                }

                float outcome = parseResult(gameInfo.result);
                if (outcome == 0.0f && gameInfo.result.empty()) {
                    ++noResult;
                    continue;
                }

                auto samples = processGame(gameInfo);

                if (!samples.empty()) {
                    ++validGames;
                    totalSamples += static_cast<int>(samples.size());
                    allSamples.insert(allSamples.end(),
                        std::make_move_iterator(samples.begin()),
                        std::make_move_iterator(samples.end()));
                } else {
                    // processGame returned empty → parse error or illegal move
                    ++illegalMove;
                }

                if (totalGames % 500 == 0) {
                    std::cout << "[SGF] " << totalGames << " files scanned, "
                              << validGames << " valid, "
                              << totalSamples << " samples so far\n";
                }
            }
        }

        int discarded = totalGames - validGames;
        float keepPct = 100.0f * validGames / std::max(1, totalGames);
        float discardPct = 100.0f * discarded / std::max(1, totalGames);

        std::cout << "\n========== SGF PROCESSING SUMMARY ==========\n"
                  << "  Total SGF files:    " << totalGames << "\n"
                  << "  Kept (valid):       " << validGames
                  << " (" << keepPct << "%)\n"
                  << "  Discarded:          " << discarded
                  << " (" << discardPct << "%)\n"
                  << "    - no moves:       " << noMoves << "\n"
                  << "    - no result:      " << noResult << "\n"
                  << "    - illegal/parse:  " << illegalMove << "\n"
                  << "  Training samples:   " << totalSamples << "\n"
                  << "  Avg samples/game:   "
                  << (validGames > 0 ? static_cast<float>(totalSamples) / validGames : 0.0f)
                  << "\n"
                  << "=============================================\n";

        return allSamples;
    }

} // namespace Hive::Learning
