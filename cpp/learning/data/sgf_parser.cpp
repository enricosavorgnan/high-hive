#include "data/headers/sgf_parser.h"
#include "nn/headers/state_encoder.h"
#include "utils.h"
#include "rules.h"
#include "moves.h"
#include "generator.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <regex>
#include <algorithm>
#include <cctype>
#include <map>

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

        // Resolve result: boardspace uses localized "won by PlayerName" style.
        // First check for draw keywords, then try to match player names.
        // Bot names may have numeric suffixes (Dumbot0/Dumbot1) not in the RE tag.
        if (!info.result.empty()) {
            std::string lower = info.result;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            // Check draw in any language
            if (lower.find("draw") != std::string::npos ||
                lower.find("nulle") != std::string::npos ||
                lower.find("nul") != std::string::npos ||
                lower.find("empate") != std::string::npos ||
                lower.find("remis") != std::string::npos ||
                lower.find("unentschieden") != std::string::npos) {
                info.result = "draw";
            } else {
                // Try to find player name in result string.
                // Strip trailing digits from P0/P1 names for fuzzy match
                // (boardspace appends 0/1 to bot names: "Dumbot0" but RE says "Dumbot")
                auto baseName = [](const std::string& name) -> std::string {
                    std::string base = name;
                    while (!base.empty() && std::isdigit(static_cast<unsigned char>(base.back())))
                        base.pop_back();
                    return base;
                };

                std::string wBase = baseName(info.white);
                std::string bBase = baseName(info.black);

                bool whiteWon = false, blackWon = false;
                // Exact name match first
                if (!info.white.empty() && info.result.find(info.white) != std::string::npos)
                    whiteWon = true;
                else if (!info.black.empty() && info.result.find(info.black) != std::string::npos)
                    blackWon = true;
                // Fuzzy: base name without trailing digits
                else if (!wBase.empty() && wBase != info.white && info.result.find(wBase) != std::string::npos)
                    whiteWon = true;
                else if (!bBase.empty() && bBase != info.black && info.result.find(bBase) != std::string::npos)
                    blackWon = true;

                if (whiteWon) info.result = "white wins";
                else if (blackWon) info.result = "black wins";
            }
        }

        // --- Move extraction ---
        // Boardspace allows players to undo moves within a turn (pickb = pick piece
        // back from board). We buffer the move for each turn and only commit it when
        // "done" is reached, so only the final placement/movement counts.
        //
        // Grid position tracker: maintain a map from boardspace grid (col, row) to
        // a stack of piece names. This is needed to resolve beetle stacking moves
        // where the reference is "." (climb on top of piece at destination).

        std::regex actionRe(R"(P([01])\[([^\]]*)\])");
        auto it = std::sregex_iterator(content.begin(), content.end(), actionRe);
        auto end = std::sregex_iterator();

        // Grid tracker: (col_index, row) → stack of piece names
        std::map<std::pair<int,int>, std::vector<std::string>> gridStacks;
        std::map<std::string, std::pair<int,int>> pieceGrid; // piece name → grid pos

        auto gridPlace = [&](const std::string& piece, int col, int row) {
            auto key = std::make_pair(col, row);
            gridStacks[key].push_back(piece);
            pieceGrid[piece] = key;
        };
        auto gridRemove = [&](const std::string& piece) {
            auto pit = pieceGrid.find(piece);
            if (pit == pieceGrid.end()) return;
            auto key = pit->second;
            auto& stk = gridStacks[key];
            // Remove piece from stack (should be on top)
            for (auto sit = stk.rbegin(); sit != stk.rend(); ++sit) {
                if (*sit == piece) { stk.erase(std::next(sit).base()); break; }
            }
            if (stk.empty()) gridStacks.erase(key);
            pieceGrid.erase(pit);
        };
        auto gridTop = [&](int col, int row) -> std::string {
            auto key = std::make_pair(col, row);
            auto git = gridStacks.find(key);
            if (git != gridStacks.end() && !git->second.empty())
                return git->second.back();
            return "";
        };
        auto parseCol = [](const std::string& s) -> int {
            if (s.empty()) return 0;
            return static_cast<int>(std::toupper(static_cast<unsigned char>(s[0]))) - 'A';
        };

        std::string turnMove;
        int lastCommitPlayer = -1;
        bool firstPlacement = true;

        // Track grid changes within a turn so we can undo them on pickb
        struct GridOp { std::string piece; int col; int row; bool isAdd; };
        std::vector<GridOp> turnGridOps;

        for (; it != end; ++it) {
            int player = ((*it)[1].str() == "0") ? 0 : 1;
            std::string actionStr = (*it)[2].str();
            auto tokens = tokenize(actionStr);
            if (tokens.size() < 2) continue;

            std::string action = toLowerStr(tokens[1]);

            if (action == "start" || action == "edit") {
                continue;
            }

            if (action == "done") {
                if (!turnMove.empty()) {
                    if (lastCommitPlayer >= 0 && player == lastCommitPlayer) {
                        info.moves.push_back("pass");
                    }
                    info.moves.push_back(turnMove);
                    lastCommitPlayer = player;
                    turnMove.clear();
                }
                turnGridOps.clear();
                continue;
            }

            if (action == "resign") {
                break;
            }

            if (action == "pass") {
                turnMove = "pass";
                continue;
            }

            if (action == "pick") {
                continue;
            }

            // Pickb: pick piece back from board
            if (action == "pickb") {
                // tokens: [seq, "pickb", col, row, piece]
                if (tokens.size() >= 5) {
                    std::string piece = addColorPrefix(tokens[4], player);
                    gridRemove(piece);
                    turnGridOps.push_back({piece, parseCol(tokens[2]), std::stoi(tokens[3]), false});
                }
                turnMove.clear();
                continue;
            }

            // Dropb / Pdropb: [seq, "dropb", piece, col, row, ref]
            if (action == "dropb" || action == "pdropb") {
                std::string piece = addColorPrefix(tokens[2], player);
                int col = (tokens.size() >= 4) ? parseCol(tokens[3]) : 0;
                int row = (tokens.size() >= 5) ? std::stoi(tokens[4]) : 0;
                std::string ref = (tokens.size() >= 6) ? tokens[5] : ".";

                if (ref == "." && !firstPlacement) {
                    // Beetle stacking: find what's at (col, row)
                    std::string topPiece = gridTop(col, row);
                    if (!topPiece.empty()) {
                        turnMove = piece + " " + topPiece;
                    } else {
                        turnMove = piece; // fallback
                    }
                } else if (ref == ".") {
                    turnMove = piece; // first placement
                    firstPlacement = false;
                } else {
                    turnMove = convertToUhp(piece, ref);
                    if (firstPlacement) firstPlacement = false;
                }

                gridPlace(piece, col, row);
                turnGridOps.push_back({piece, col, row, true});
                continue;
            }

            // Move / PMove: [seq, "move"/"pmove", color, piece, col, row, ref]
            if (action == "move" || action == "pmove") {
                if (tokens.size() >= 7) {
                    std::string piece = addColorPrefix(tokens[3], player);
                    int col = parseCol(tokens[4]);
                    int row = std::stoi(tokens[5]);
                    std::string ref = tokens[6];

                    // Remove piece from old position if it was on the board
                    gridRemove(piece);

                    if (ref == "." && !firstPlacement) {
                        std::string topPiece = gridTop(col, row);
                        if (!topPiece.empty()) {
                            turnMove = piece + " " + topPiece;
                        } else {
                            turnMove = piece;
                        }
                    } else if (ref == ".") {
                        turnMove = piece;
                        firstPlacement = false;
                    } else {
                        turnMove = convertToUhp(piece, ref);
                        if (firstPlacement) firstPlacement = false;
                    }

                    gridPlace(piece, col, row);
                    turnGridOps.push_back({piece, col, row, true});
                }
                continue;
            }
        }

        if (!turnMove.empty()) {
            info.moves.push_back(turnMove);
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

        // Parse game outcome from result string
        float whiteOutcome = parseResult(game.result);
        bool needInferResult = (whiteOutcome == 0.0f && game.result.empty());

        // Pass 1: replay all moves, validate legality, record (state, policy) pairs.
        // If no result string, we'll infer the outcome from the final board state.
        struct PendingSample {
            torch::Tensor state;
            torch::Tensor policy;
            Color toMove;
        };
        std::vector<PendingSample> pending;
        State state;

        for (const auto& sgfMove : game.moves) {
            std::string uhpMove = sgfMoveToUhp(sgfMove);
            if (uhpMove.empty()) continue;

            // Handle pass
            if (uhpMove == "pass") {
                Move passMove;
                passMove.type = Move::Pass;

                PendingSample ps;
                ps.state = StateEncoder::encode(state);
                ps.policy = torch::zeros({ACTION_SPACE});
                ps.toMove = state.toMove();
                pending.push_back(std::move(ps));

                state.applyMove(passMove);
                continue;
            }

            // Get legal moves to validate
            auto legalMoves = MoveGenerator::generateMoves(state);

            // Parse the move
            Move move;
            try {
                move = StringToMove(uhpMove, state.board());
            } catch (...) {
                return {};
            }

            // Validate: check if this move is in the legal moves
            bool isLegal = false;
            for (const auto& lm : legalMoves) {
                if (lm.to == move.to) {
                    if (lm.type == Move::Place && move.type == Move::Place && lm.piece.bug == move.piece.bug) {
                        isLegal = true;
                        move = lm;
                        break;
                    } else if (lm.type == Move::PieceMove && move.type == Move::PieceMove && lm.from == move.from) {
                        isLegal = true;
                        move = lm;
                        break;
                    } else if (lm.type == Move::Drag && move.type == Move::PieceMove && lm.from == move.from) {
                        // Pillbug drag: SGF shows as regular move, engine uses Drag type
                        isLegal = true;
                        move = lm;
                        break;
                    }
                }
            }

            if (!isLegal) {
                return {};
            }

            // Record pending sample
            PendingSample ps;
            ps.state = StateEncoder::encode(state);
            ps.policy = torch::zeros({ACTION_SPACE});
            int action = ActionEncoder::moveToAction(move, state);
            if (action >= 0 && action < ACTION_SPACE) {
                ps.policy[action] = 1.0f;
            }
            ps.toMove = state.toMove();
            pending.push_back(std::move(ps));

            state.applyMove(move);
        }

        // If no RE tag, infer result from final board state (queen surrounded)
        if (needInferResult) {
            auto gr = state.result();
            if (gr == GameResult::WhiteWin)
                whiteOutcome = 1.0f;
            else if (gr == GameResult::BlackWin)
                whiteOutcome = -1.0f;
            else if (gr == GameResult::Draw)
                whiteOutcome = 0.0f;  // draw is valid
            else
                return {};  // game didn't end in a decisive result, skip
        }

        // Pass 2: assign value labels and build final samples
        samples.reserve(pending.size());
        for (auto& ps : pending) {
            TrainingSample sample;
            sample.state = std::move(ps.state);
            sample.policy = std::move(ps.policy);
            sample.value = (ps.toMove == Color::White) ? whiteOutcome : -whiteOutcome;
            samples.push_back(std::move(sample));
        }

        return samples;
    }

    std::vector<TrainingSample> SgfParser::processDirectory(const std::string& dirPath) {
        std::vector<TrainingSample> allSamples;
        int totalGames = 0, validGames = 0, totalSamples = 0;
        int noMoves = 0, illegalMove = 0;
        int resultFromTag = 0, resultInferred = 0, resultUnknown = 0;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
            if (entry.path().extension() == ".sgf") {
                ++totalGames;

                auto gameInfo = parseFile(entry.path().string());

                if (gameInfo.moves.empty()) {
                    ++noMoves;
                    continue;
                }

                bool hasResultTag = !gameInfo.result.empty();

                auto samples = processGame(gameInfo);

                if (!samples.empty()) {
                    ++validGames;
                    if (hasResultTag) ++resultFromTag;
                    else ++resultInferred;
                    totalSamples += static_cast<int>(samples.size());
                    allSamples.insert(allSamples.end(),
                        std::make_move_iterator(samples.begin()),
                        std::make_move_iterator(samples.end()));
                } else {
                    if (!hasResultTag) ++resultUnknown;
                    else ++illegalMove;
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
                  << "    - result from RE: " << resultFromTag << "\n"
                  << "    - result inferred:" << resultInferred
                  << " (from final board state)\n"
                  << "  Discarded:          " << discarded
                  << " (" << discardPct << "%)\n"
                  << "    - no moves:       " << noMoves << "\n"
                  << "    - no result:      " << resultUnknown
                  << " (no RE tag + no queen surrounded)\n"
                  << "    - illegal/parse:  " << illegalMove << "\n"
                  << "  Training samples:   " << totalSamples << "\n"
                  << "  Avg samples/game:   "
                  << (validGames > 0 ? static_cast<float>(totalSamples) / validGames : 0.0f)
                  << "\n"
                  << "=============================================\n";

        return allSamples;
    }

} // namespace Hive::Learning
