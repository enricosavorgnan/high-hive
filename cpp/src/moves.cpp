#include "headers/moves.h"
#include <unordered_set>
#include <deque>
#include <algorithm>

#include "rules.h"

namespace Hive::Moves {

    // --- Implementations ---
    void getAntMoves(const Board& board, const Coord prop, std::vector<Coord>& targets) {
        std::unordered_set<Coord, CoordHash> visited;
        std::deque<Coord> queue;
        visited.reserve(64);

        visited.insert(prop);
        queue.push_back(prop);

        while (!queue.empty()) {
            Coord curr = queue.front();
            queue.pop_front();

            if (curr != prop) {
                targets.push_back(curr);
            }

            auto neighbors = coordNeighbors(curr);

            for (const auto& n : neighbors) {
                if (board.empty(n) && !visited.contains(n)) {
                    if (RuleEngine::canSlide(board, curr, n, prop) && RuleEngine::touchesHive(board, n, prop)) {
                        visited.insert(n);
                        queue.push_back(n);
                    }
                }
            }
        }
    }

    
    void getBeetleMoves(const Board& board, Coord prop, std::vector<Coord>& targets) {
        auto neighbors = coordNeighbors(prop);

        for (const auto& n : neighbors) {

            // The 3D slide rule natively handles climbing up, moving on top, and stepping down.
            if (RuleEngine::canSlide(board, prop, n, prop)) {
                if (RuleEngine::touchesHive(board, n, prop)) {
                    targets.push_back(n);
                }
            }
        }
    }


    void getGrasshopperMoves(const Board& board, Coord prop, std::vector<Coord>& targets) {
        for (const auto& dir : DIRECTIONS) {
            Coord curr = prop + dir;
            
            if (board.empty(curr)) continue;

            while (!board.empty(curr)) {
                curr = curr + dir;
            }
            targets.push_back(curr);
        }
    }


    void getLadybugMoves(const Board& board, Coord prop, std::vector<Coord>& targets) {
        // Lambda to calculate height safely ignoring the origin piece
        auto vHeight = [&](Coord c) {
            return board.height(c) - (c == prop ? 1 : 0);
        };

        std::vector<Coord> step1;
        for (const auto& n : coordNeighbors(prop)) {
            if (vHeight(n)>0 && RuleEngine::canSlide(board, prop, n, prop)) step1.push_back(n);
        }

        std::vector<Coord> step2;
        for (const auto& s1 : step1) {
            for (const auto& n : coordNeighbors(s1)) {
                if (vHeight(n) > 0 && RuleEngine::canSlide(board, s1, n, prop)) step2.push_back(n);
            }
        }

        std::unordered_set<Coord, CoordHash> uniqueTargets;
        for (const auto& s2 : step2) {
            for (const auto& n : coordNeighbors(s2)) {
                if (vHeight(n) == 0 && n != prop) {
                    if (RuleEngine::canSlide(board, s2, n, prop) && RuleEngine::touchesHive(board, n, prop) ){
                    uniqueTargets.insert(n);
                    }
                }
            }
        }

        targets.assign(uniqueTargets.begin(), uniqueTargets.end());
    }


    void getMosquitoMoves(const Board& board, Coord prop, std::vector<Coord>& targets, std::optional<Coord> lastMovedPieceCoord, std::vector<std::pair<Coord, Coord>>& dragTargets, const std::unordered_set<Coord, CoordHash>& articulationPoints) {
        if (board.height(prop) > 1) {
            getBeetleMoves(board, prop, targets);
            return;
        }

        // Allocate contiguous memory for intermediate accumulation
        std::vector<Coord> tempTargets;
        tempTargets.reserve(64); // Pre-allocate to prevent dynamic resizing overhead

        // A lightweight array to track which bug behaviors have already been copied.
        // This prevents running getAntMoves() multiple times if touching multiple Ants.
        bool copiedBehaviors[8] = {false};

        auto neighbors = coordNeighbors(prop);

        for (const auto& n : neighbors) {
            const Piece* neighborPiece = board.top(n);

            if (neighborPiece) {
                Bug targetBug = neighborPiece->bug;

                // Mathematical correction for Mosquito elevation mirroring
                if (targetBug == Bug::Mosquito) {
                    if (board.height(n) > 1) {
                        targetBug = Bug::Beetle; // Elevated Mosquito acts as a Beetle
                    } else {
                        continue; // Ground-level Mosquito provides no movement capabilities
                    }
                }

                int bugTypeIdx = static_cast<int>(targetBug);

                // If we have not already copied this bug's movement type
                if (!copiedBehaviors[bugTypeIdx]) {
                    copiedBehaviors[bugTypeIdx] = true;

                    switch (neighborPiece->bug) {
                        case Bug::Queen:       getQueenMoves(board, prop, tempTargets); break;
                        case Bug::Beetle:      getBeetleMoves(board, prop, tempTargets); break;
                        case Bug::Spider:      getSpiderMoves(board, prop, tempTargets); break;
                        case Bug::Grasshopper: getGrasshopperMoves(board, prop, tempTargets); break;
                        case Bug::Ant:         getAntMoves(board, prop, tempTargets); break;
                        case Bug::Ladybug:     getLadybugMoves(board, prop, tempTargets); break;
                        case Bug::Pillbug:     getPillbugMoves(board, prop, tempTargets,lastMovedPieceCoord, dragTargets, articulationPoints); break;
                        default: break;
                    }
                }
            }
        }

        if (tempTargets.empty()) return;

        // Cache-friendly coordinate deduplication
        // (A Beetle and a Queen might yield the exact same adjacent destination)
        std::ranges::sort(tempTargets, [](const Coord& a, const Coord& b) {
            if (a.q != b.q) return a.q < b.q;
            return a.r < b.r;
        });

        tempTargets.erase(std::unique(tempTargets.begin(), tempTargets.end()), tempTargets.end());

        // Transfer unique coordinates to the main output buffer
        targets.insert(targets.end(), tempTargets.begin(), tempTargets.end());
    }


    void getPillbugDragMoves(const Board &board, const Coord prop, std::optional<Coord> lastMovedPieceCoord, std::vector<std::pair<Coord, Coord>> &dragTargets, const std::unordered_set<Coord, CoordHash>& articulationPoints) {
        std::vector<Coord> validSources;
        std::vector<Coord> validDestinations;
        std::array<Coord, 6> neighbors = coordNeighbors(prop);

        // Phase 1. Find valid pieces to drag
        for (const auto& src: neighbors) {
            if (board.empty(src)) continue;
            if (board.height(src) > 1) continue;
            if (lastMovedPieceCoord && src == *lastMovedPieceCoord) continue;
            if (!RuleEngine::canLiftPiece(board, src, articulationPoints)) continue;
            if (!RuleEngine::canSlide(board, src, prop, src)) continue;
            validSources.push_back(src);
        }

        // Phase 2. Find valid spots
        for (const auto &dst : neighbors) {
            if (board.empty(dst)) {
                validDestinations.push_back(dst);
            }
        }

        // Phase 3. Apply Cartesian combination and validation
        for (const auto& src : validSources) {
            for (const auto& dst : validDestinations) {
                if (src == dst) continue;

                if (RuleEngine::canSlide(board, prop, dst, src)) {
                    dragTargets.push_back({src, dst});
                }
            }
        }
    }


    void getPillbugMoves(const Board &board, Coord prop, std::vector<Coord> &targets, std::optional<Coord> lastMovedPieceCoord, std::vector<std::pair<Coord, Coord>> &dragTargets, const std::unordered_set<Coord, CoordHash>& articulationPoints) {
        // The Pillbug's standard movement is exactly identical to the Queen (1 step, slide).
        getQueenMoves(board, prop, targets);
        // Special Drag move
        getPillbugDragMoves(board, prop, lastMovedPieceCoord, dragTargets, articulationPoints);
    }


    void getQueenMoves(const Board& board, Coord prop, std::vector<Coord>& targets) {
        auto neighbors = coordNeighbors(prop);

        for (const auto& n : neighbors) {
            if (board.empty(n)) {
                if (RuleEngine::canSlide(board, prop, n, prop) && RuleEngine::touchesHive(board, n, prop)) {
                    targets.push_back(n);
                }
            }
        }
    }


    void getSpiderMoves(const Board& board, Coord prop, std::vector<Coord>& targets) {
        struct State {
            Coord c;
            int depth;
            std::vector<Coord> path;
        };

        std::vector<State> stack;
        stack.push_back({prop, 0, {prop}});

        while (!stack.empty()) {
            State current = stack.back();
            stack.pop_back();

            if (current.depth == 3) {
                if (std::ranges::find(targets.begin(), targets.end(), current.c) == targets.end()) {
                    targets.push_back(current.c);
                }
                continue;
            }

            auto neighbors = coordNeighbors(current.c);

            for (const auto& n : neighbors) {
                if (!board.empty(n)) continue;

                bool visited = false;
                for(const auto& p : current.path) if(p == n) visited = true;
                if(visited) continue;

                if (!RuleEngine::canSlide(board, current.c, n, prop)) continue;
                if (!RuleEngine::touchesHive(board, n, prop)) continue;

                std::vector<Coord> nextPath = current.path;
                nextPath.push_back(n);
                stack.push_back({n, current.depth + 1, nextPath});
            }
        }
    }


} // namespace Hive::Moves