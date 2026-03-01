#include "nn/headers/state_encoder.h"
#include "board.h"
#include "coords.h"
#include "pieces.h"
#include "rules.h"
#include "moves.h"

#include <cmath>

namespace Hive::Learning {

    std::pair<int, int> StateEncoder::computeCentroid(const GameState& state) {
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

    std::pair<int, int> StateEncoder::axialToGrid(Coord coord, int centQ, int centR) {
        // Center on centroid, then offset to middle of grid
        int gx = (coord.q - centQ) + GRID_SIZE / 2;
        int gy = (coord.r - centR) + GRID_SIZE / 2;
        return {gx, gy};
    }

    torch::Tensor StateEncoder::encode(const GameState& state) {
        auto tensor = torch::zeros({NUM_CHANNELS, GRID_SIZE, GRID_SIZE});
        auto acc = tensor.accessor<float, 3>();

        const Board& board = state.board();
        const Color me = state.toMove();
        const Color opp = rival(me);

        auto [centQ, centR] = computeCentroid(state);

        const auto& occupied = board.occupiedCoords();

        // Channels 0-15: Piece presence by bug type for each player
        // Channel 16: Stack height
        // Channel 17: Color of top piece
        for (const auto& coord : occupied) {
            auto [gx, gy] = axialToGrid(coord, centQ, centR);
            if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) continue;

            int idx = Board::AxToIndex(coord);
            int height = board.height(coord);

            // Stack height (channel 16), normalized
            acc[16][gy][gx] = static_cast<float>(height) / 6.0f;

            // Process each piece in the stack
            for (int h = 0; h < height; ++h) {
                const Piece& p = board._grid[idx]._data[h];
                int bugIdx = bugIndex(p.bug);

                if (p.color == me) {
                    // Channels 0-7: my pieces
                    acc[bugIdx][gy][gx] = 1.0f;
                } else {
                    // Channels 8-15: opponent pieces
                    acc[8 + bugIdx][gy][gx] = 1.0f;
                }
            }

            // Color of top piece (channel 17)
            const Piece* top = board.top(coord);
            if (top) {
                acc[17][gy][gx] = (top->color == me) ? 1.0f : -1.0f;
            }
        }

        // Channel 18: Legal placement targets
        {
            std::vector<Piece> hand = state.getHand(me);
            std::vector<Move> placements = RuleEngine::generateMoves(board, me, hand);
            for (const auto& m : placements) {
                if (m.type == Move::Place) {
                    auto [gx, gy] = axialToGrid(m.to, centQ, centR);
                    if (gx >= 0 && gx < GRID_SIZE && gy >= 0 && gy < GRID_SIZE) {
                        acc[18][gy][gx] = 1.0f;
                    }
                }
            }
        }

        // Channels 19-20: Queen adjacency (fraction of occupied neighbors)
        auto encodeQueenAdj = [&](Color c, int channel) {
            if (!state.queenPlaced(c)) return;

            // Find queen
            for (const auto& coord : occupied) {
                int idx = Board::AxToIndex(coord);
                for (int h = 0; h < board.height(coord); ++h) {
                    const Piece& p = board._grid[idx]._data[h];
                    if (p.color == c && p.bug == Bug::Queen) {
                        auto neighbors = coordNeighbors(coord);
                        int occCount = 0;
                        for (const auto& n : neighbors) {
                            if (!board.empty(n)) ++occCount;
                        }
                        // Fill entire plane with the fraction
                        float frac = static_cast<float>(occCount) / 6.0f;
                        for (int y = 0; y < GRID_SIZE; ++y)
                            for (int x = 0; x < GRID_SIZE; ++x)
                                acc[channel][y][x] = frac;
                        return;
                    }
                }
            }
        };
        encodeQueenAdj(me, 19);
        encodeQueenAdj(opp, 20);

        // Channel 21: Articulation points (pieces that can't be lifted)
        for (const auto& coord : occupied) {
            int idx = Board::AxToIndex(coord);
            if (!RuleEngine::isBoardConnected(board, idx)) {
                auto [gx, gy] = axialToGrid(coord, centQ, centR);
                if (gx >= 0 && gx < GRID_SIZE && gy >= 0 && gy < GRID_SIZE) {
                    acc[21][gy][gx] = 1.0f;
                }
            }
        }

        // Channel 22: Turn indicator (always 1 since we encode from current player's perspective)
        for (int y = 0; y < GRID_SIZE; ++y)
            for (int x = 0; x < GRID_SIZE; ++x)
                acc[22][y][x] = 1.0f;

        // Channel 23: Hand fullness (fraction of pieces remaining)
        {
            int totalPieces = 14; // standard hand total
            int remaining = 0;
            for (int bi = 0; bi < NUM_BUG_TYPES; ++bi) {
                remaining += state.remaining(me, bugFromIndex(bi));
            }
            float frac = static_cast<float>(remaining) / static_cast<float>(totalPieces);
            for (int y = 0; y < GRID_SIZE; ++y)
                for (int x = 0; x < GRID_SIZE; ++x)
                    acc[23][y][x] = frac;
        }

        return tensor;
    }

} // namespace Hive::Learning
