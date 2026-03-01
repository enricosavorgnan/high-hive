#pragma once

#include <torch/torch.h>
#include "state.h"
#include "moves.h"
#include "nn/headers/neural_net.h"
#include "nn/headers/state_encoder.h"
#include "nn/headers/action_encoder.h"
#include "config/headers/config.h"

#include <memory>
#include <unordered_map>
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

        float prior = 0.0f;     // P(a) from NN policy
        int visitCount = 0;     // N(s,a)
        float totalValue = 0.0f;// W(s,a) - accumulated value

        bool isExpanded = false;
        bool isTerminal = false;
        float terminalValue = 0.0f; // Only valid if isTerminal

        std::vector<std::unique_ptr<MCTSNode>> children;

        // Q(s,a) = W(s,a) / N(s,a)
        float qValue() const {
            return (visitCount > 0) ? totalValue / static_cast<float>(visitCount) : 0.0f;
        }

        // PUCT selection score
        float puctScore(int parentVisits) const {
            float exploration = C_PUCT * prior *
                std::sqrt(static_cast<float>(parentVisits)) /
                (1.0f + static_cast<float>(visitCount));
            return qValue() + exploration;
        }

        // Select child with highest PUCT score
        MCTSNode* selectChild() {
            MCTSNode* best = nullptr;
            float bestScore = -1e9f;

            for (auto& child : children) {
                float score = child->puctScore(visitCount);
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
        std::vector<std::pair<Move, int>> search(GameState& state, bool addNoise = true);

        // Select action based on visit counts and temperature
        // temp=1.0: sample proportional to N^(1/temp)
        // temp→0: argmax
        static int selectAction(const std::vector<int>& visitCounts, float temperature);

        // Advance the tree by reusing the subtree for the chosen action
        void advanceTree(int action);

        // Reset the tree
        void reset();

    private:
        HiveNet network_;
        std::unique_ptr<MCTSNode> root_;
        std::mt19937 rng_;

        // Single simulation: select → expand → backprop
        void simulate(GameState& state);

        // Expand a leaf node using the neural network
        float expand(MCTSNode* node, GameState& state);

        // Backpropagate a value up the tree
        static void backpropagate(MCTSNode* node, float value);

        // Add Dirichlet noise to root priors
        void addDirichletNoise(MCTSNode* node);
    };

} // namespace Hive::Learning
