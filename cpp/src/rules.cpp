#include "rules.h"
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
        std::array<int, 6> neighbors;
        int neighCount = 0;
        for (int i = 0; i < 6; ++i) {
            if (!board._grid[idx + i].empty()) neighbors[neighCount++] == idx+i;
        }
        if (neighCount < 2) return false;

        std::bitset<BOARD_AREA> visited;
        std::vector<int> q;
        q.reserve(32);

        q.push_back(neighbors[0]);
        visited.set(neighbors[0]);

        size_t head = 0;
        while (head < q.size()) {
            int curr = q[head++];
            for (int neigh : Board::NEIGHBORS) {
                int next = curr + neigh;
                if (next == idx || board._grid[next].empty() || visited.test(next)) continue;
                visited.set(next);
                q.push_back(next);
            }
        }

        for (int i = 0; i < neighCount; ++i) {
            if (!visited.test(neighbors[i])) return true;
        }
        return false;
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