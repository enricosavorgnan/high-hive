#include "alphaZeroEngine/data/headers/sgf_parser.h"
#include "alphaZeroEngine/nn/headers/state_encoder.h"
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

#include "alphaZeroEngine/nn/headers/action_encoder.h"

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

    // Add color prefix if missing (old board-space format: "S1" → "wS1")
    static std::string addColorPrefix(const std::string& piece, int player) {
        if (!piece.empty() && (piece[0] == 'w' || piece[0] == 'b')) return piece;
        return (player == 0 ? "w" : "b") + piece;
    }

    // Normalize a board-space reference to UHP format:
    //  - strip trailing '.' (stacking notation)
    //  - add "1" to single-instance expansion pieces (P, L, M) missing a digit
    static std::string normalizeRef(const std::string& ref) {
        std::string r = ref;

        // Strip trailing '.' (board-space stacking → UHP bare piece name)
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

        // Parse game outcome from result string. We track two things:
        //   - whiteOutcome:  the value label to attach to the samples
        //   - outcomeKnown:  whether that label is a real win/loss signal we
        //                    want the value head to fit. Draws and aborted
        //                    games get outcomeKnown=false so their samples
        //                    still feed the policy head (we know which move
        //                    was played) but contribute zero gradient to the
        //                    value loss via the value_weight mask.
        float whiteOutcome = 0.0f;
        bool outcomeKnown = false;
        if (!game.result.empty()) {
            std::string lower = game.result;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("white") != std::string::npos || lower.find("w+") != std::string::npos) {
                whiteOutcome = 1.0f;
                outcomeKnown = true;
            } else if (lower.find("black") != std::string::npos || lower.find("b+") != std::string::npos) {
                whiteOutcome = -1.0f;
                outcomeKnown = true;
            }
            // Note: draws and unrecognised RE strings leave outcomeKnown=false.
            // We deliberately do not let draws train the value head, per the
            // current dataset-quality preference.
        }
        bool tryInferFromBoard = !outcomeKnown;

        // Pass 1: replay all moves, validate legality, record (state, policy) pairs.
        struct PendingSample {
            torch::Tensor planes;
            torch::Tensor scalars;
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

                auto encoded = StateEncoder::encode(state);
                PendingSample ps;
                ps.planes = std::move(encoded.planes);
                ps.scalars = std::move(encoded.scalars);
                ps.policy = torch::zeros({ACTION_SPACE});
                ps.policy[PASS_ACTION_INDEX] = 1.0f;
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
            auto encoded = StateEncoder::encode(state);
            PendingSample ps;
            ps.planes = std::move(encoded.planes);
            ps.scalars = std::move(encoded.scalars);
            ps.policy = torch::zeros({ACTION_SPACE});
            int action = ActionEncoder::moveToAction(move, state);
            if (action >= 0 && action < ACTION_SPACE) {
                ps.policy[action] = 1.0f;
            }
            ps.toMove = state.toMove();
            pending.push_back(std::move(ps));

            state.applyMove(move);
        }

        // If we still don't know the outcome, try to read it from the final
        // board state. A surrounded queen is the only "true" signal here;
        // anything else (draws, aborted games, incomplete replays) leaves
        // outcomeKnown=false and the samples will be policy-only.
        if (tryInferFromBoard) {
            auto gr = state.result();
            if (gr == GameResult::WhiteWin) {
                whiteOutcome = 1.0f;
                outcomeKnown = true;
            } else if (gr == GameResult::BlackWin) {
                whiteOutcome = -1.0f;
                outcomeKnown = true;
            }
            // GameResult::Draw and ::None: leave outcomeKnown=false on purpose.
        }

        // Pass 2: build final samples. Every replayed move becomes a sample;
        // the value_weight mask decides whether each sample feeds the value
        // head or only the policy head.
        const float value_weight = outcomeKnown ? 1.0f : 0.0f;
        samples.reserve(pending.size());
        for (auto& ps : pending) {
            TrainingSample sample;
            sample.planes = std::move(ps.planes);
            sample.scalars = std::move(ps.scalars);
            sample.policy = std::move(ps.policy);
            sample.value = (ps.toMove == Color::White) ? whiteOutcome : -whiteOutcome;
            sample.value_weight = value_weight;
            samples.push_back(std::move(sample));
        }

        return samples;
    }

    // Flush accumulated samples to a quintuple of .pt files on disk, then
    // clear the buffer. The trainer's pretrainFromDisk reads the same set.
    // _weights.pt carries the per-sample value-loss mask (1.0 = the value
    // label is a real signal, 0.0 = mask the value loss for this sample).
    static void flushBatch(std::vector<TrainingSample>& pending, const std::string& outputDir, int batchIdx) {
        if (pending.empty()) return;

        int n = static_cast<int>(pending.size());
        auto planes = torch::zeros({n, NUM_CHANNELS, GRID_SIZE, GRID_SIZE});
        auto scalars = torch::zeros({n, NUM_SCALAR_FEATURES});
        auto policies = torch::zeros({n, ACTION_SPACE});
        auto values = torch::zeros({n, 1});
        auto value_weights = torch::zeros({n, 1});

        for (int i = 0; i < n; ++i) {
            planes[i] = pending[i].planes;
            scalars[i] = pending[i].scalars;
            policies[i] = pending[i].policy;
            values[i][0] = pending[i].value;
            value_weights[i][0] = pending[i].value_weight;
        }

        std::string prefix = outputDir + "/batch_" + std::to_string(batchIdx);
        torch::save(planes, prefix + "_planes.pt");
        torch::save(scalars, prefix + "_scalars.pt");
        torch::save(policies, prefix + "_policies.pt");
        torch::save(values, prefix + "_values.pt");
        torch::save(value_weights, prefix + "_weights.pt");

        pending.clear();
    }

    int SgfParser::processDirectory(const std::string& dirPath, const std::string& outputDir, int skipFiles) {
        std::filesystem::create_directories(outputDir);

        int totalGames = 0, validGames = 0, totalSamples = 0;
        int noMoves = 0, illegalMove = 0;
        // Of the games we kept, how many have a real win/loss signal vs how
        // many feed only the policy head (value loss masked).
        int gamesValueKnown = 0, gamesValueMasked = 0;
        int valueKnownSamples = 0, valueMaskedSamples = 0;
        int skipped = 0;

        const int FLUSH_THRESHOLD = 10000; // samples per batch file
        std::vector<TrainingSample> pending;

        // Auto-detect next batch index from existing _planes.pt files in outputDir
        int batchIdx = 0;
        for (const auto& f : std::filesystem::directory_iterator(outputDir)) {
            auto fname = f.path().filename().string();
            if (fname.find("batch_") == 0 && fname.find("_planes.pt") != std::string::npos) {
                auto numStr = fname.substr(6, fname.find("_planes.pt") - 6);
                int idx = std::stoi(numStr) + 1;
                if (idx > batchIdx) batchIdx = idx;
            }
        }
        if (skipFiles > 0) {
            std::cout << "[SGF] Resuming: skipping first " << skipFiles
                      << " files, starting at batch " << batchIdx << "\n";
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
            if (entry.path().extension() == ".sgf") {
                ++totalGames;

                // Skip already-processed files when resuming
                if (skipped < skipFiles) {
                    ++skipped;
                    continue;
                }

                auto gameInfo = parseFile(entry.path().string());

                if (gameInfo.moves.empty()) {
                    ++noMoves;
                    continue;
                }

                auto samples = processGame(gameInfo);

                if (!samples.empty()) {
                    ++validGames;
                    // All samples in a game share the same value_weight
                    // (set inside processGame), so the first one is enough
                    // to classify the game.
                    bool valueKnown = samples.front().value_weight > 0.5f;
                    int nSamples = static_cast<int>(samples.size());
                    totalSamples += nSamples;
                    if (valueKnown) {
                        ++gamesValueKnown;
                        valueKnownSamples += nSamples;
                    } else {
                        ++gamesValueMasked;
                        valueMaskedSamples += nSamples;
                    }

                    pending.insert(pending.end(),
                        std::make_move_iterator(samples.begin()),
                        std::make_move_iterator(samples.end()));

                    if (static_cast<int>(pending.size()) >= FLUSH_THRESHOLD) {
                        flushBatch(pending, outputDir, batchIdx++);
                        std::cout << "[SGF] Saved batch " << batchIdx
                                  << " (" << totalSamples << " samples so far)\n";
                    }
                } else {
                    // Empty samples now only happens when the replay fails
                    // (illegal/unparseable move). No-result games are kept
                    // with value_weight=0 instead of being discarded.
                    ++illegalMove;
                }

                if (totalGames % 500 == 0) {
                    std::cout << "[SGF] " << totalGames << " files scanned, "
                              << validGames << " valid, "
                              << totalSamples << " samples so far\n";
                }
            }
        }

        // Flush remaining samples
        if (!pending.empty()) {
            flushBatch(pending, outputDir, batchIdx++);
        }

        int discarded = totalGames - validGames;
        float keepPct = 100.0f * validGames / std::max(1, totalGames);
        float discardPct = 100.0f * discarded / std::max(1, totalGames);

        std::cout << "\n========== SGF PROCESSING SUMMARY ==========\n"
                  << "  Total SGF files:    " << totalGames << "\n"
                  << "  Kept (valid):       " << validGames
                  << " (" << keepPct << "%)\n"
                  << "    - value known:    " << gamesValueKnown
                  << " (" << valueKnownSamples << " samples - feed both heads)\n"
                  << "    - value masked:   " << gamesValueMasked
                  << " (" << valueMaskedSamples << " samples - policy head only)\n"
                  << "  Discarded:          " << discarded
                  << " (" << discardPct << "%)\n"
                  << "    - no moves:       " << noMoves << "\n"
                  << "    - illegal/parse:  " << illegalMove << "\n"
                  << "  Training samples:   " << totalSamples
                  << " (in " << batchIdx << " batch files)\n"
                  << "  Avg samples/game:   "
                  << (validGames > 0 ? static_cast<float>(totalSamples) / validGames : 0.0f)
                  << "\n"
                  << "  Output directory:   " << outputDir << "\n"
                  << "=============================================\n";

        return totalSamples;
    }

} // namespace Hive::Learning
