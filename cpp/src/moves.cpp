#include "headers/moves.h"
#include <unordered_set>
#include <deque>
#include <algorithm>

namespace Hive::Moves {

    // Contact Rule
    // Keep physical contact 
    static bool touchesHive(const Board& board, Coord target, Coord exclude) {
        auto neighbors = coordNeighbors(target);
        for (const auto& n : neighbors) {
            if (n == exclude) continue;
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
        int currentHeight = board.height(prop);
        int propIdx = Board::AxToIndex(prop);

        for (const auto& n : neighbors) {
            if (!board.empty(n)) {
                // Climb up is always physically unobstructed
                targets.push_back(n);
            } else {
                // If moving on ground (height == 1), sliding rules apply.
                // If stepping down from the hive (height > 1), the slide is unconstrained by ground gates.
                bool validSlide = (currentHeight > 1) ? true : RuleEngine::canSlide(board, propIdx, Board::AxToIndex(n));
                
                if (validSlide && touchesHive(board, n, prop)) {
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

        std::unordered_set<Coord, CoordHash> calculatedTargets;
        std::vector<Coord> tempTargets;
        
        auto neighbors = coordNeighbors(prop);

        for (const auto& n : neighbors) {
            const Piece* neighborPiece = board.top(n);
            if (neighborPiece) {
                tempTargets.clear();
                
                switch (neighborPiece->bug) {
                    case Bug::Queen:       getQueenMoves(board, prop, tempTargets); break;
                    case Bug::Beetle:      getBeetleMoves(board, prop, tempTargets); break;
                    case Bug::Spider:      getSpiderMoves(board, prop, tempTargets); break;
                    case Bug::Grasshopper: getGrasshopperMoves(board, prop, tempTargets); break;
                    case Bug::Ant:         getAntMoves(board, prop, tempTargets); break;
                    case Bug::Ladybug:     getLadybugMoves(board, prop, tempTargets); break;
                    case Bug::Pillbug:     getPillbugMoves(board, prop, tempTargets); break;
                    case Bug::Mosquito:    break; 
                }

                for (const auto& t : tempTargets) calculatedTargets.insert(t);
            }
        }

        targets.assign(calculatedTargets.begin(), calculatedTargets.end());
    }


    void getPillbugMoves(const Board& board, Coord prop, std::vector<Coord>& targets) {
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