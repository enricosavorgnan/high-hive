#pragma once

#include <torch/torch.h>
#include "state.h"
#include "moves.h"
#include "alphaZeroEngine/nn/headers/neural_net.h"
#include "alphaZeroEngine/nn/headers/state_encoder.h"
#include "alphaZeroEngine/nn/headers/action_encoder.h"
#include "alphaZeroEngine/config/headers/config.h"

#include <chrono>
#include <memory>
#include <vector>
#include <random>

// MCTS (Monte Carlo Tree Search) for AlphaZero
//
// Each node stores:
//   - visit count N
//   - total value W (sum of backed-up values)
//   - prior probability P (from neural network policy)
//   - children (one per legal action)
//
// Search loop (800 simulations):
//   1. SELECT: follow PUCT from root to leaf
//   2. EXPAND: evaluate leaf with neural net, create children
//   3. BACKPROP: propagate value up, negating at each level

namespace Hive::Learning {

    struct MCTSNode {
        MCTSNode* parent = nullptr;
        int action = -1;  // Action that led to this node
        Move move{Move::Pass, {}, {0,0}, {0,0}};  // Cached move (avoids regenerating legalMoves)

        float prior = 0.0f;     // P(a) from NN policy
        int visitCount = 0;     // N(s,a)
        float totalValue = 0.0f;// W(s,a) - accumulated value
        int virtualLoss = 0;    // Pending sims that selected this node but haven't backed up yet

        bool isExpanded = false;
        bool isTerminal = false;
        float terminalValue = 0.0f; // Only valid if isTerminal

        std::vector<std::unique_ptr<MCTSNode>> children;

        // Effective N and W with virtual loss folded in (pessimistic estimate for
        // any path currently in flight, which pushes sibling selections elsewhere).
        float effectiveVisits() const {
            return static_cast<float>(visitCount + virtualLoss);
        }
        float effectiveValue() const {
            return totalValue - static_cast<float>(virtualLoss) * VIRTUAL_LOSS;
        }

        // Q(s,a) = W(s,a) / N(s,a)
        float qValue() const {
            float n = effectiveVisits();
            return (n > 0.0f) ? effectiveValue() / n : 0.0f;
        }

        // PUCT selection score
        float puctScore(int parentVisits) const {
            float exploration = C_PUCT * prior *
                std::sqrt(static_cast<float>(parentVisits)) /
                (1.0f + effectiveVisits());
            return qValue() + exploration;
        }

        // Select child with highest PUCT score
        MCTSNode* selectChild() {
            MCTSNode* best = nullptr;
            float bestScore = -1e9f;
            int parentN = visitCount + virtualLoss;

            for (auto& child : children) {
                float score = child->puctScore(parentN);
                if (score > bestScore) {
                    bestScore = score;
                    best = child.get();
                }
            }
            return best;
        }
    };

    class MCTS {
    public:
        explicit MCTS(HiveNet network);

        // Run MCTS search from current state. Returns visit counts per legal move.
        // add_noise: whether to add Dirichlet noise at root (for exploration during training)
        std::vector<std::pair<Move, int>> search(State& state, bool addNoise = true);

        // Time-budgeted search: runs simulations until the deadline.
        // For tournament inference — keeps search() with fixed sim count for training.
        std::vector<std::pair<Move, int>> searchWithBudget(
            State& state,
            std::chrono::milliseconds budget,
            bool addNoise = false);

        // Select action based on visit counts and temperature
        // temp=1.0: sample proportional to N^(1/temp)
        // temp→0: argmax
        static int selectAction(const std::vector<int>& visitCounts, float temperature);

        // Advance the tree by reusing the subtree for the chosen move.
        // Matches on Move identity (not action index), because the action
        // encoding is lossy: multiple distinct legal moves can share the
        // same action int, so action-only matching can promote the wrong
        // subtree and cache moves against a phantom board state.
        void advanceTree(const Move& selectedMove);

        // Reset the tree
        void reset();

    private:
        HiveNet network_;
        std::unique_ptr<MCTSNode> root_;
        std::mt19937 rng_;

        // Run a batch of `K` simulations in parallel: descend K times with
        // virtual loss, then evaluate all leaves in a single batched NN
        // forward pass, then expand and backprop each. Saturates the GPU.
        void simulateBatch(State& state, int K);

        // Expand the root using a single-sample NN forward pass.
        // Used once before the simulation loop; the batched leaf expansion
        // path inside simulateBatch handles every other expansion.
        float expand(MCTSNode* node, State& state);

        // Backpropagate a value up the tree (used for root-only path that
        // doesn't carry virtual loss).
        static void backpropagate(MCTSNode* node, float value);

        // Add Dirichlet noise to root priors
        void addDirichletNoise(MCTSNode* node);
    };

} // namespace Hive::Learning
