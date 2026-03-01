#include "headers/rules.h"
#include "headers/moves.h"
#include <bitset>
#include <array>
#include <unordered_set>

namespace Hive{

    bool RuleEngine::canSlide(const Board& board, int fromIdx, int toIdx) {
        int diff = toIdx - fromIdx;
        int dir = -1;

        for (int i = 0; i < 6; ++i) {
            if (Board::NEIGHBORS[i] == diff) {
                dir = i; break;
            }
        }
        if (dir == -1) return false;

        int gate1 = fromIdx + Board::NEIGHBORS[(dir + 5) % 6];
        int gate2 = fromIdx + Board::NEIGHBORS[(dir + 1) % 6];

        // 3D Sliding Check
        int hFrom = board._grid[fromIdx].size();
        int hTo = board._grid[toIdx].size();

        // Calculate the peak transition height
        int maxHeight = std::max(hFrom, hTo + 1);

        int hGate1 = board._grid[gate1].size();
        int hGate2 = board._grid[gate2].size();

        // The slide is blocked if BOTH gates are at or above the maximum transition height
        return !(hGate1 >= maxHeight && hGate2 >= maxHeight);
    }

    bool RuleEngine::isBoardConnected(const Board& board, int idx) {
        // Stack check: If the stack height is >= 2, removing the top piece leaves a piece behind:
        // current connectivity is kept.
        if (board._grid[idx].size() >= 2) {
            return true;
        }

        // Occupied neighbors
        std::array<int, 6> neighbors;
        int neighCount = 0;
        for (int i = 0; i < 6; ++i) {
            int neighborIdx = idx + Board::NEIGHBORS[i];
            if (!board._grid[neighborIdx].empty()) {
                neighbors[neighCount++] = neighborIdx;
            }
        }
        // Leaf node check: A node with 0 or 1 neighbors is not an articulation point:
        // current connectivity is kept
        if (neighCount < 2) {
            return true;
        }

        // BFS traversal to verify if all neighbors remain connected without the piece at 'idx'.
        std::bitset<BOARD_AREA> visited;
        std::vector<int> q;
        q.reserve(32); // Pieces are 28 so 32 is ok

        q.push_back(neighbors[0]);
        visited.set(neighbors[0]);

        size_t head = 0;
        while (head < q.size()) {
            int curr = q[head++];

            for (int offset : Board::NEIGHBORS) {
                int next = curr + offset;
                // Exclude the piece being simulated for removal,
                // empty cells, and already evaluated cells.
                if (next == idx || board._grid[next].empty() || visited.test(next)) {
                    continue;
                }

                visited.set(next);
                q.push_back(next);
            }
        }

        // If any neighbor is absent from the visited set, it implies the graph has been partitioned into distinct components.
        for (int i = 1; i < neighCount; ++i) {
            if (!visited.test(neighbors[i])) {
                return false;
            }
        }

        return true;
    }

    // ---- Helper: check if coord touches a piece of given color ----
    static bool touchesColor(const Board& board, Coord coord, Color col) {
        for (const auto& n : coordNeighbors(coord)) {
            if (!Board::isValid(n)) continue;
            const Piece* p = board.top(n);
            if (p && p->color == col) return true;
        }
        return false;
    }

    static bool touchesOpponentColor(const Board& board, Coord coord, Color col) {
        for (const auto& n : coordNeighbors(coord)) {
            if (!Board::isValid(n)) continue;
            const Piece* p = board.top(n);
            if (p && p->color != col) return true;
        }
        return false;
    }

    std::vector<Move> RuleEngine::generatePlacements(const Board& board, Color player, const std::vector<Piece>& hand) {
        std::vector<Move> moves;
        if (hand.empty()) return moves;

        const auto& occupied = board.occupiedCoords();

        // Count the player's ply (number of pieces this player has on the board)
        int playerPly = 0;
        for (const auto& c : occupied) {
            // Count all pieces of this color in the stack at this coordinate
            int idx = Board::AxToIndex(c);
            for (int i = 0; i < board._grid[idx].size(); ++i) {
                if (board._grid[idx]._data[i].color == player) {
                    ++playerPly;
                }
            }
        }

        // Determine valid placement targets
        std::vector<Coord> targets;

        if (occupied.empty()) {
            // Very first move of the game: place at origin
            targets.push_back(Coord{0, 0});
        } else if (playerPly == 0) {
            // First move of this player (second player's first turn):
            // can place adjacent to any occupied cell (no color restriction)
            std::unordered_set<Coord, CoordHash> candidates;
            for (const auto& oc : occupied) {
                for (const auto& n : coordNeighbors(oc)) {
                    if (Board::isValid(n) && board.empty(n)) {
                        candidates.insert(n);
                    }
                }
            }
            targets.assign(candidates.begin(), candidates.end());
        } else {
            // Standard placement: adjacent to friendly pieces, NOT adjacent to enemy pieces
            std::unordered_set<Coord, CoordHash> candidates;
            for (const auto& oc : occupied) {
                for (const auto& n : coordNeighbors(oc)) {
                    if (Board::isValid(n) && board.empty(n)) {
                        candidates.insert(n);
                    }
                }
            }
            for (const auto& c : candidates) {
                if (touchesColor(board, c, player) && !touchesOpponentColor(board, c, player)) {
                    targets.push_back(c);
                }
            }
        }

        if (targets.empty()) return moves;

        // Determine which bugs can be placed
        // Queen Rule: if playerPly == 3 and queen is still in hand, MUST place queen
        bool queenInHand = false;
        for (const auto& p : hand) {
            if (p.bug == Bug::Queen) { queenInHand = true; break; }
        }

        bool mustPlaceQueen = (playerPly == 3 && queenInHand);

        // Determine which pieces can be placed
        std::vector<const Piece*> placeablePieces;
        if (mustPlaceQueen) {
            // Queen Rule: must place queen on 4th move
            for (const auto& p : hand) {
                if (p.bug == Bug::Queen) {
                    placeablePieces.push_back(&p);
                    break;
                }
            }
        } else {
            for (const auto& p : hand) {
                // Tournament rule: no queen on first move
                if (playerPly == 0 && p.bug == Bug::Queen) continue;
                placeablePieces.push_back(&p);
            }
        }

        moves.reserve(targets.size() * placeablePieces.size());
        for (const auto& target : targets) {
            for (const auto* piece : placeablePieces) {
                Move m;
                m.type = Move::Place;
                m.piece = *piece;
                m.to = target;
                m.from = Coord{0, 0}; // unused for Place
                moves.push_back(m);
            }
        }

        return moves;
    }

    std::vector<Move> RuleEngine::generateMovements(const Board& board, Color player) {
        std::vector<Move> moves;

        // Check if this player's queen is on the board. If not, no movements allowed.
        bool queenPlaced = false;
        const auto& occupied = board.occupiedCoords();
        for (const auto& c : occupied) {
            const Piece* p = board.top(c);
            if (p && p->color == player && p->bug == Bug::Queen) {
                queenPlaced = true;
                break;
            }
            // Also check in stacks (queen could be under a beetle)
            int idx = Board::AxToIndex(c);
            for (int i = 0; i < board._grid[idx].size(); ++i) {
                if (board._grid[idx]._data[i].color == player && board._grid[idx]._data[i].bug == Bug::Queen) {
                    queenPlaced = true;
                    break;
                }
            }
            if (queenPlaced) break;
        }

        if (!queenPlaced) return moves;

        // For each occupied cell with a piece of this player's color on top
        for (const auto& coord : occupied) {
            const Piece* topPiece = board.top(coord);
            if (!topPiece || topPiece->color != player) continue;

            int idx = Board::AxToIndex(coord);

            // One Hive Rule: check if removing this piece disconnects the hive
            if (!isBoardConnected(board, idx)) continue;

            // Generate target coordinates based on bug type
            std::vector<Coord> targets;
            switch (topPiece->bug) {
                case Bug::Queen:
                    Moves::getQueenMoves(board, coord, targets);
                    break;
                case Bug::Ant:
                    Moves::getAntMoves(board, coord, targets);
                    break;
                case Bug::Beetle:
                    Moves::getBeetleMoves(board, coord, targets);
                    break;
                case Bug::Spider:
                    Moves::getSpiderMoves(board, coord, targets);
                    break;
                case Bug::Grasshopper:
                    Moves::getGrasshopperMoves(board, coord, targets);
                    break;
                case Bug::Ladybug:
                    Moves::getLadybugMoves(board, coord, targets);
                    break;
                case Bug::Mosquito:
                    Moves::getMosquitoMoves(board, coord, targets);
                    break;
                case Bug::Pillbug:
                    Moves::getPillbugMoves(board, coord, targets);
                    break;
            }

            // Create Move objects for each valid target
            for (const auto& target : targets) {
                Move m;
                m.type = Move::PieceMove;
                m.piece = *topPiece;
                m.from = coord;
                m.to = target;
                moves.push_back(m);
            }
        }

        return moves;
    }

    std::vector<Move> RuleEngine::generateMoves(const Board& board, Color turnPlayer, const std::vector<Piece>& hand) {
        std::vector<Move> placements = RuleEngine::generatePlacements(board, turnPlayer, hand);

        std::vector<Move> movements = RuleEngine::generateMovements(board, turnPlayer);

        placements.insert(placements.end(), movements.begin(), movements.end());

        return placements;
    }
}