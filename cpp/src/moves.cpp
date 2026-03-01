#include "headers/moves.h"
#include "headers/rules.h"
#include <unordered_set>
#include <deque>
#include <algorithm>

namespace Hive::Moves {

    // Contact Rule
    // Keep physical contact 
    static bool touchesHive(const Board& board, Coord target, Coord prop) {
        // If we are leaving a stack, the underlying piece remains a valid hive connection
        bool propRemainsOccupied = (board.height(prop) > 1);

        for (const auto& n : coordNeighbors(target)) {
            if (n == prop) {
                if (propRemainsOccupied) return true;
                continue;
            }
            if (!board.empty(n)) return true;
        }
        return false;
    }

    // --- Implementations ---
    void getAntMoves(const Board& board, Coord prop, std::vector<Coord>& targets) {
        std::unordered_set<Coord, CoordHash> visited;
        std::deque<Coord> queue;

        visited.insert(prop);
        queue.push_back(prop);

        while (!queue.empty()) {
            Coord curr = queue.front();
            queue.pop_front();

            if (curr != prop) {
                targets.push_back(curr);
            }

            auto neighbors = coordNeighbors(curr);
            int currIdx = Board::AxToIndex(curr);

            for (const auto& n : neighbors) {
                if (board.empty(n) && visited.find(n) == visited.end()) {
                    if (RuleEngine::canSlide(board, currIdx, Board::AxToIndex(n)) && touchesHive(board, n, curr)) {
                        visited.insert(n);
                        queue.push_back(n);
                    }
                }
            }
        }
    }

    
    void getBeetleMoves(const Board& board, Coord prop, std::vector<Coord>& targets) {
        auto neighbors = coordNeighbors(prop);
        int propIdx = Board::AxToIndex(prop);

        for (const auto& n : neighbors) {
            int nIdx = Board::AxToIndex(n);

            // The 3D slide rule natively handles climbing up, moving on top, and stepping down.
            if (RuleEngine::canSlide(board, propIdx, nIdx)) {
                if (touchesHive(board, n, prop)) {
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
        std::vector<Coord> step1;
        for (const auto& n : coordNeighbors(prop)) {
            if (!board.empty(n)) step1.push_back(n);
        }

        std::vector<Coord> step2;
        for (const auto& s1 : step1) {
            for (const auto& n : coordNeighbors(s1)) {
                if (!board.empty(n)) step2.push_back(n);
            }
        }

        std::unordered_set<Coord, CoordHash> uniqueTargets;
        for (const auto& s2 : step2) {
            for (const auto& n : coordNeighbors(s2)) {
                if (board.empty(n) && n != prop) {
                    uniqueTargets.insert(n);
                }
            }
        }

        targets.assign(uniqueTargets.begin(), uniqueTargets.end());
    }


    void getMosquitoMoves(const Board& board, Coord prop, std::vector<Coord>& targets) {
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

            if (neighborPiece && neighborPiece->bug != Bug::Mosquito) {
                int bugTypeIdx = static_cast<int>(neighborPiece->bug);

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
                        case Bug::Pillbug:     getPillbugMoves(board, prop, tempTargets); break;
                        default: break;
                    }
                }
            }
        }

        if (tempTargets.empty()) return;

        // Cache-friendly coordinate deduplication
        // (A Beetle and a Queen might yield the exact same adjacent destination)
        std::sort(tempTargets.begin(), tempTargets.end(), [](const Coord& a, const Coord& b) {
            if (a.q != b.q) return a.q < b.q;
            return a.r < b.r;
        });

        tempTargets.erase(std::unique(tempTargets.begin(), tempTargets.end()), tempTargets.end());

        // Transfer unique coordinates to the main output buffer
        targets.insert(targets.end(), tempTargets.begin(), tempTargets.end());
    }


    void getPillbugMoves(const Board &board, Coord prop, std::vector<Coord> &targets) {
        // The Pillbug's standard movement is exactly identical to the Queen (1 step, slide).
        getQueenMoves(board, prop, targets);
    }


    void getQueenMoves(const Board& board, Coord prop, std::vector<Coord>& targets) {
        auto neighbors = coordNeighbors(prop);
        int propIdx = Board::AxToIndex(prop);

        for (const auto& n : neighbors) {
            if (board.empty(n)) {
                if (RuleEngine::canSlide(board, propIdx, Board::AxToIndex(n)) && touchesHive(board, n, prop)) {
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
                if (std::find(targets.begin(), targets.end(), current.c) == targets.end()) {
                    targets.push_back(current.c);
                }
                continue;
            }

            auto neighbors = coordNeighbors(current.c);
            int currIdx = Board::AxToIndex(current.c);

            for (const auto& n : neighbors) {
                if (!board.empty(n)) continue;

                bool visited = false;
                for(const auto& p : current.path) if(p == n) visited = true;
                if(visited) continue;

                if (!RuleEngine::canSlide(board, currIdx, Board::AxToIndex(n))) continue;
                if (!touchesHive(board, n, current.c)) continue;

                std::vector<Coord> nextPath = current.path;
                nextPath.push_back(n);
                stack.push_back({n, current.depth + 1, nextPath});
            }
        }
    }


} // namespace Hive::Moves