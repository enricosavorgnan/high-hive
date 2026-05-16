#include "alphaZeroEngine/nn/headers/state_encoder.h"
#include "board.h"
#include "coords.h"
#include "pieces.h"
#include "rules.h"

namespace Hive::Learning {

    std::pair<int, int> StateEncoder::computeCentroid(const State& state) {
        const auto& occupied = state.board().occupiedCoords();
        if (occupied.empty()) return {0, 0};

        long sumQ = 0, sumR = 0;
        for (const auto& c : occupied) {
            sumQ += c.q;
            sumR += c.r;
        }
        int n = static_cast<int>(occupied.size());
        return {static_cast<int>(sumQ / n), static_cast<int>(sumR / n)};
    }

    std::optional<std::pair<int, int>> StateEncoder::axialToGrid(
            Coord coord, int centQ, int centR) {
        int gx = (coord.q - centQ) + GRID_SIZE / 2;
        int gy = (coord.r - centR) + GRID_SIZE / 2;
        if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) {
            return std::nullopt;
        }
        return std::make_pair(gx, gy);
    }

    EncodedState StateEncoder::encode(
            const State& state,
            const std::unordered_set<Coord, CoordHash>* artPointsCache) {
        auto planes = torch::zeros({NUM_CHANNELS, GRID_SIZE, GRID_SIZE});
        auto scalars = torch::zeros({NUM_SCALAR_FEATURES});
        auto planeAcc = planes.accessor<float, 3>();
        auto scalarAcc = scalars.accessor<float, 1>();

        const Board& board = state.board();
        const Color me = state.toMove();
        const Color opp = rival(me);

        auto [centQ, centR] = computeCentroid(state);
        const auto& occupied = board.occupiedCoords();

        // Planes 0-15 (per-bug-type presence) + 16 (stack height) + 17 (top color)
        for (const auto& coord : occupied) {
            auto grid = axialToGrid(coord, centQ, centR);
            if (!grid) continue;
            auto [gx, gy] = *grid;

            int idx = Board::AxToIndex(coord);
            int height = board.height(coord);

            planeAcc[16][gy][gx] = static_cast<float>(height) / 6.0f;

            for (int h = 0; h < height; ++h) {
                const Piece& p = board.cellAt(idx)._data[h];
                int bugIdx = bugIndex(p.bug);
                int channel = (p.color == me) ? bugIdx : (8 + bugIdx);
                planeAcc[channel][gy][gx] = 1.0f;
            }

            const Piece* top = board.top(coord);
            if (top) {
                planeAcc[17][gy][gx] = (top->color == me) ? 1.0f : -1.0f;
            }
        }

        // Plane 18: articulation points. Reuses the caller's Tarjan result if
        // one is supplied so that we don't double-compute alongside the move
        // generator (which itself runs Tarjan on the same board).
        const auto& artPoints = artPointsCache
            ? *artPointsCache
            : RuleEngine::getArticulationPoints(board);
        for (const auto& coord : artPoints) {
            auto grid = axialToGrid(coord, centQ, centR);
            if (!grid) continue;
            auto [gx, gy] = *grid;
            planeAcc[18][gy][gx] = 1.0f;
        }

        // Scalar 0/1: queen adjacency (fraction of 6 neighbors that are occupied).
        // The queen's own coord on the board is the answer to "where is the queen",
        // so we scan the occupied set for each color's queen and count neighbors.
        auto queenAdj = [&](Color c) -> float {
            if (!state.isQueenPlaced(c)) return 0.0f;
            for (const auto& coord : occupied) {
                int idx = Board::AxToIndex(coord);
                int height = board.height(coord);
                for (int h = 0; h < height; ++h) {
                    const Piece& p = board.cellAt(idx)._data[h];
                    if (p.color == c && p.bug == Bug::Queen) {
                        int occCount = 0;
                        for (const auto& n : coordNeighbors(coord)) {
                            if (!board.empty(n)) ++occCount;
                        }
                        return static_cast<float>(occCount) / 6.0f;
                    }
                }
            }
            return 0.0f;
        };
        scalarAcc[0] = queenAdj(me);
        scalarAcc[1] = queenAdj(opp);

        // Scalar 2: my hand fullness (fraction of pieces still in hand out of 14).
        {
            int remaining = 0;
            for (int bi = 0; bi < NUM_BUG_TYPES; ++bi) {
                remaining += state.remaining(me, bugFromIndex(bi));
            }
            scalarAcc[2] = static_cast<float>(remaining) / 14.0f;
        }

        return EncodedState{std::move(planes), std::move(scalars)};
    }

} // namespace Hive::Learning
