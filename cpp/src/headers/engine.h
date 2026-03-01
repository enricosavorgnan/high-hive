#pragma once

#include "board.h"
#include "moves.h"
#include "pieces.h"
#include <vector>
#include <string>

namespace Hive {

    // Abstract base class for all game engines (Random, Minimax, AlphaZero, etc.)
    class Engine {
    public:
        virtual ~Engine() = default;

        // The core method every engine must implement
        virtual Move getBestMove(const Board& board, Color turnPlayer, const std::vector<Piece>& hand, const std::vector<Move>& validMoves) = 0;
    };

    // A purely random mover for baseline testing
    class RandomEngine : public Engine {
    public:
        Move getBestMove(const Board& board, Color turnPlayer, const std::vector<Piece>& hand, const std::vector<Move>& validMoves) override;
    };

} // namespace Hive

// AlphaZeroEngine is defined in learning/alphazero_engine.h
// and only available when compiled with ENABLE_LEARNING