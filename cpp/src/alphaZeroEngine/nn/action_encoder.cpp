#include "alphaZeroEngine/nn/headers/action_encoder.h"
#include "alphaZeroEngine/nn/headers/state_encoder.h"
#include "generator.h"

namespace Hive::Learning {

    int ActionEncoder::pieceToPlane(const Piece& piece) {
        switch (piece.bug) {
            case Bug::Queen:       return 0;
            case Bug::Beetle:      return 1 + (piece.id - 1); // 1-2
            case Bug::Spider:      return 3 + (piece.id - 1); // 3-4
            case Bug::Grasshopper: return 5 + (piece.id - 1); // 5-7
            case Bug::Ant:         return 8 + (piece.id - 1); // 8-10
            case Bug::Ladybug:     return 11;
            case Bug::Mosquito:    return 12;
            case Bug::Pillbug:     return 13;
        }
        return 0;
    }

    int ActionEncoder::moveToAction(const Move& move, const State& state) {
        if (move.type == Move::Pass) {
            return PASS_ACTION_INDEX;
        }

        int basePlane = pieceToPlane(move.piece);
        if (move.type == Move::Drag) {
            basePlane += DRAG_PLANE_OFFSET;
        }

        auto [centQ, centR] = StateEncoder::computeCentroid(state);
        auto grid = StateEncoder::axialToGrid(move.to, centQ, centR);
        if (!grid) {
            // Destination falls outside the centered 26x26 box. Should be
            // extremely rare; we collapse to the pass slot so callers don't
            // index out of bounds. Track frequency during pretraining audit.
            return PASS_ACTION_INDEX;
        }
        auto [gx, gy] = *grid;
        return encodeAction(basePlane, gy, gx);
    }

    Move ActionEncoder::actionToMove(int action, const State& state) {
        if (action == PASS_ACTION_INDEX) {
            return Move{Move::Pass, {}, {0, 0}, {0, 0}, {0, 0}};
        }

        auto moves = MoveGenerator::generateMoves(state);
        for (const auto& m : moves) {
            if (moveToAction(m, state) == action) {
                return m;
            }
        }
        return Move{Move::Pass, {}, {0, 0}, {0, 0}, {0, 0}};
    }

    torch::Tensor ActionEncoder::legalMask(const State& state) {
        return legalMaskFromMoves(MoveGenerator::generateMoves(state), state);
    }

    torch::Tensor ActionEncoder::legalMaskFromMoves(
            const std::vector<Move>& moves,
            const State& state) {
        auto mask = torch::zeros({ACTION_SPACE});
        auto acc = mask.accessor<float, 1>();

        for (const auto& move : moves) {
            int action = moveToAction(move, state);
            if (action >= 0 && action < ACTION_SPACE) {
                acc[action] = 1.0f;
            }
        }

        if (moves.empty()) {
            acc[PASS_ACTION_INDEX] = 1.0f;
        }

        return mask;
    }

} // namespace Hive::Learning
