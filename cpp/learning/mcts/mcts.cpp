#include "mcts/headers/mcts.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Hive::Learning {

    MCTS::MCTS(HiveNet network)
        : network_(std::move(network))
        , root_(std::make_unique<MCTSNode>())
        , rng_(std::random_device{}()) {}

    void MCTS::reset() {
        root_ = std::make_unique<MCTSNode>();
    }

    void MCTS::advanceTree(int action) {
        if (!root_ || !root_->isExpanded) {
            root_ = std::make_unique<MCTSNode>();
            return;
        }

        for (auto& child : root_->children) {
            if (child->action == action) {
                child->parent = nullptr;
                root_ = std::move(child);
                return;
            }
        }

        // Action not found in children — start fresh
        root_ = std::make_unique<MCTSNode>();
    }

    std::vector<std::pair<Move, int>> MCTS::search(GameState& state, bool addNoise) {
        // Ensure root is initialized
        if (!root_) {
            root_ = std::make_unique<MCTSNode>();
        }

        // If root hasn't been expanded yet, expand it
        if (!root_->isExpanded) {
            expand(root_.get(), state);
        }

        // Add Dirichlet noise to root for exploration
        if (addNoise) {
            addDirichletNoise(root_.get());
        }

        // Run simulations
        for (int sim = 0; sim < MCTS_SIMS; ++sim) {
            simulate(state);
        }

        // Collect results: (move, visit_count) for each legal action
        auto legalMoves = state.legalMoves();
        std::vector<std::pair<Move, int>> results;
        results.reserve(root_->children.size());

        // Build action-to-move mapping
        std::unordered_map<int, Move> actionToMoveMap;
        for (const auto& m : legalMoves) {
            int a = ActionEncoder::moveToAction(m, state);
            actionToMoveMap[a] = m;
        }

        for (const auto& child : root_->children) {
            auto it = actionToMoveMap.find(child->action);
            if (it != actionToMoveMap.end()) {
                results.emplace_back(it->second, child->visitCount);
            }
        }

        return results;
    }

    void MCTS::simulate(GameState& state) {
        // 1. SELECT: traverse tree following PUCT until we reach a leaf
        MCTSNode* node = root_.get();
        std::vector<UndoInfo> undos;

        while (node->isExpanded && !node->isTerminal) {
            node = node->selectChild();
            if (!node) break;

            // Find the move corresponding to this action and apply it
            auto legalMoves = state.legalMoves();
            bool found = false;
            for (const auto& m : legalMoves) {
                int a = ActionEncoder::moveToAction(m, state);
                if (a == node->action) {
                    undos.push_back(state.apply(m));
                    found = true;
                    break;
                }
            }
            if (!found) {
                // If action not in legal moves, this shouldn't happen but handle gracefully
                // Apply pass as fallback
                Move passMove;
                passMove.type = Move::Pass;
                undos.push_back(state.apply(passMove));
            }
        }

        // 2. EXPAND & EVALUATE
        float value;
        if (node && node->isTerminal) {
            value = node->terminalValue;
        } else if (node) {
            value = expand(node, state);
        } else {
            value = 0.0f;
        }

        // 3. BACKPROP: propagate value up, negating at each level
        backpropagate(node, value);

        // Undo all moves
        for (auto it = undos.rbegin(); it != undos.rend(); ++it) {
            state.undo(*it);
        }
    }

    float MCTS::expand(MCTSNode* node, GameState& state) {
        node->isExpanded = true;

        // Check terminal state
        if (state.isTerminal()) {
            node->isTerminal = true;
            // Value from perspective of the player who just moved (parent's player)
            Color prevPlayer = rival(state.toMove());
            node->terminalValue = state.resultForColor(prevPlayer);
            return node->terminalValue;
        }

        auto legalMoves = state.legalMoves();
        if (legalMoves.empty()) {
            // No legal moves — pass
            node->isTerminal = false;
            return 0.0f;
        }

        // Neural network evaluation
        torch::NoGradGuard no_grad;
        auto stateTensor = StateEncoder::encode(state).unsqueeze(0); // [1, C, H, W]
        auto mask = ActionEncoder::legalMask(state).unsqueeze(0);    // [1, ACTION_SPACE]

        auto [policy, value] = network_->forward_masked(stateTensor, mask);

        float nodeValue = value.item<float>();

        // Create children with priors from policy
        auto policyAcc = policy.squeeze(0); // [ACTION_SPACE]
        node->children.reserve(legalMoves.size());

        for (const auto& move : legalMoves) {
            int action = ActionEncoder::moveToAction(move, state);

            auto child = std::make_unique<MCTSNode>();
            child->parent = node;
            child->action = action;
            child->prior = policyAcc[action].item<float>();
            node->children.push_back(std::move(child));
        }

        return nodeValue;
    }

    void MCTS::backpropagate(MCTSNode* node, float value) {
        while (node != nullptr) {
            node->visitCount += 1;
            node->totalValue += value;
            value = -value; // Negate at each level (opponent's perspective)
            node = node->parent;
        }
    }

    void MCTS::addDirichletNoise(MCTSNode* node) {
        if (node->children.empty()) return;

        int n = static_cast<int>(node->children.size());
        std::gamma_distribution<float> gamma(DIRICHLET_ALPHA, 1.0f);

        std::vector<float> noise(n);
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) {
            noise[i] = gamma(rng_);
            sum += noise[i];
        }
        // Normalize
        if (sum > 0.0f) {
            for (int i = 0; i < n; ++i) {
                noise[i] /= sum;
            }
        }

        // Mix noise with prior: P'(a) = (1 - ε) * P(a) + ε * noise(a)
        for (int i = 0; i < n; ++i) {
            node->children[i]->prior =
                (1.0f - DIRICHLET_EPSILON) * node->children[i]->prior +
                DIRICHLET_EPSILON * noise[i];
        }
    }

    int MCTS::selectAction(const std::vector<int>& visitCounts, float temperature) {
        if (visitCounts.empty()) return 0;

        if (temperature < 1e-6f) {
            // Argmax
            return static_cast<int>(
                std::distance(visitCounts.begin(),
                    std::max_element(visitCounts.begin(), visitCounts.end())));
        }

        // Sample proportional to N^(1/temp)
        std::vector<double> probs(visitCounts.size());
        double sum = 0.0;
        for (size_t i = 0; i < visitCounts.size(); ++i) {
            probs[i] = std::pow(static_cast<double>(visitCounts[i]), 1.0 / temperature);
            sum += probs[i];
        }

        if (sum <= 0.0) return 0;

        for (auto& p : probs) p /= sum;

        std::discrete_distribution<int> dist(probs.begin(), probs.end());
        static std::mt19937 rng(std::random_device{}());
        return dist(rng);
    }

} // namespace Hive::Learning
