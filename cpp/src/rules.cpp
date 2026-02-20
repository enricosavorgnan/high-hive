#include "headers/rules.h"
#include <bitset>
#include <array>

namespace Hive{

    bool RuleEngine::canSlide(const Board& board, int fromIdx, int toIdx) {
        int diff = toIdx - fromIdx;
        int dir = -1;

        // FROM and TO are neighbours
        for (int i = 0; i < 6; ++i) {
            if (Board::NEIGHBORS[i] == diff) {
                dir = i; break;
            }
        }
        if (dir == -1) return false;

        int gate1 = fromIdx + Board::NEIGHBORS[(dir + 5) % 6];
        int gate2 = fromIdx + Board::NEIGHBORS[(dir + 1) % 6];

        return !(!board._grid[gate1].empty() && !board._grid[gate2].empty());
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

    std::vector<Move> generatePlacements(const Board& board, Color player, const std::vector<Piece>& hand) {
        return;
        // TODO: write function
    }

    std::vector<Move> generateMovements(const Board& board, Color player) {
        return;
        // TODO: write function
    }

    std::vector<Move> generateMoves(const Board& board, Color turnPlayer, const std::vector<Piece>& hand) {
        std::vector<Move> placements = generatePlacements(board, turnPlayer, hand);

        std::vector<Move> movements = generateMovements(board, turnPlayer);

        placements.insert(placements.end(), movements.begin(), movements.end());

        return placements;
    }
}