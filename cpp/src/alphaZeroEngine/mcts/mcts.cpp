#include "alphaZeroEngine/mcts/headers/mcts.h"
#include "generator.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Hive::Learning {

    namespace {
        // Move identity comparison used by advanceTree to find the cached
        // child whose move matches the one the caller actually applied.
        // Type-aware because Move's `from` is uninitialized for Place and
        // `pillbug` is uninitialized for non-Drag moves.
        bool movesEquivalent(const Move& a, const Move& b) {
            if (a.type != b.type) return false;
            if (a.type == Move::Pass) return true;
            if (!(a.piece == b.piece)) return false;
            switch (a.type) {
                case Move::Place:     return a.to == b.to;
                case Move::PieceMove: return a.from == b.from && a.to == b.to;
                case Move::Drag:      return a.from == b.from && a.to == b.to && a.pillbug == b.pillbug;
                default:              return false;
            }
        }
    }

    MCTS::MCTS(HiveNet network)
        : network_(std::move(network))
        , root_(std::make_unique<MCTSNode>())
        , rng_(std::random_device{}()) {}

    void MCTS::reset() {
        root_ = std::make_unique<MCTSNode>();
        maxSearchDepth_ = 0;
    }

    void MCTS::advanceTree(const Move& selectedMove) {
        if (!root_ || !root_->isExpanded) {
            root_ = std::make_unique<MCTSNode>();
            return;
        }

        for (auto& child : root_->children) {
            if (movesEquivalent(child->move, selectedMove)) {
                child->parent = nullptr;
                root_ = std::move(child);
                return;
            }
        }

        // Move not found in children — start fresh
        root_ = std::make_unique<MCTSNode>();
    }

    std::vector<std::pair<Move, int>> MCTS::search(State& state, bool addNoise) {
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

        // Run simulations in batched chunks so the GPU does one forward pass
        // per MCTS_BATCH leaves instead of one per leaf.
        int simsRemaining = MCTS_SIMS;
        while (simsRemaining > 0) {
            int K = std::min(simsRemaining, MCTS_BATCH);
            simulateBatch(state, K);
            simsRemaining -= K;
        }

        // Collect results from cached moves (no legalMoves() call needed)
        std::vector<std::pair<Move, int>> results;
        results.reserve(root_->children.size());
        for (const auto& child : root_->children) {
            results.emplace_back(child->move, child->visitCount);
        }
        return results;
    }

    std::vector<std::pair<Move, int>> MCTS::searchWithBudget(
            State& state,
            std::chrono::milliseconds budget,
            bool addNoise) {
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

        // Run simulations in batched chunks until deadline.
        auto deadline = std::chrono::steady_clock::now() + budget;
        while (std::chrono::steady_clock::now() < deadline) {
            simulateBatch(state, MCTS_BATCH);
        }

        // Collect results from cached moves
        std::vector<std::pair<Move, int>> results;
        results.reserve(root_->children.size());
        for (const auto& child : root_->children) {
            results.emplace_back(child->move, child->visitCount);
        }
        return results;
    }

    namespace {
        // Per-pending-sim record used by simulateBatch.
        struct PendingLeaf {
            std::vector<MCTSNode*> path;   // root → leaf
            torch::Tensor stateTensor;     // [C, H, W], CPU
            torch::Tensor mask;            // [ACTION_SPACE], CPU
            State leafState;               // copy at the leaf (used by expansion)
        };

        // Back-up `value` from leaf to root, negating at each level (alternating
        // player perspective), and remove the virtual loss that the selection
        // phase added to each non-root node on the path.
        void backpropWithVirtualLoss(const std::vector<MCTSNode*>& path, float value) {
            for (auto it = path.rbegin(); it != path.rend(); ++it) {
                MCTSNode* node = *it;
                node->visitCount += 1;
                node->totalValue += value;
                if (node != path.front() && node->virtualLoss > 0) {
                    --(node->virtualLoss);
                }
                value = -value;
            }
        }

        // Best-effort undo: pop the moves applied during a single selection
        // traversal so the next selection starts from the actual root state.
        void rewindPath(State& state, std::size_t pathSize) {
            // path[0] is the root and corresponds to no applied move
            for (std::size_t i = 1; i < pathSize; ++i) {
                state.undoLastMove();
            }
        }
    } // namespace

    void MCTS::simulateBatch(State& state, int K) {
        if (K <= 0) return;

        std::vector<PendingLeaf> pending;
        pending.reserve(static_cast<std::size_t>(K));

        // Phase 1: K diversified selection traversals.
        // Virtual loss applied along each path keeps subsequent traversals
        // from collapsing onto the same leaf.
        for (int k = 0; k < K; ++k) {
            MCTSNode* node = root_.get();
            std::vector<MCTSNode*> path{node};

            while (node->isExpanded && !node->isTerminal) {
                MCTSNode* child = node->selectChild();
                if (!child) break;
                child->virtualLoss += 1;
                state.applyMove(child->move);
                path.push_back(child);
                node = child;
            }

            // The depth is exactly the number of nodes in the path minus the root
            maxSearchDepth_ = std::max(maxSearchDepth_, static_cast<int>(path.size()) - 1);

            // Resolve terminal status without an NN call when possible.
            bool resolved = false;
            float immediate = 0.0f;
            if (node && node->isTerminal) {
                immediate = node->terminalValue;
                resolved = true;
            } else if (node && state.isTerminal()) {
                node->isTerminal = true;
                Color prevPlayer = rival(state.toMove());
                node->terminalValue = state.resultForColor(prevPlayer);
                immediate = node->terminalValue;
                resolved = true;
            }

            if (resolved || !node) {
                backpropWithVirtualLoss(path, resolved ? immediate : 0.0f);
                rewindPath(state, path.size());
                continue;
            }

            // Defer to batched forward pass. Snapshot encode/mask + state copy
            // now while the board reflects the leaf position.
            PendingLeaf leaf;
            leaf.path = std::move(path);
            leaf.stateTensor = StateEncoder::encode(state);
            leaf.mask = ActionEncoder::legalMask(state);
            leaf.leafState = state; // copy at leaf
            rewindPath(state, leaf.path.size());
            pending.push_back(std::move(leaf));
        }

        if (pending.empty()) return;

        // // Phase 2: one batched NN forward pass.
        // torch::NoGradGuard no_grad;
        // std::vector<torch::Tensor> stateTensors;
        // std::vector<torch::Tensor> masks;
        // stateTensors.reserve(pending.size());
        // masks.reserve(pending.size());
        // for (auto& p : pending) {
        //     stateTensors.push_back(p.stateTensor);
        //     masks.push_back(p.mask);
        // }
        // auto batchState = torch::stack(stateTensors);                // [B, C, H, W]
        // auto batchMask = torch::stack(masks);                        // [B, ACTION_SPACE]
        //
        // auto device = network_->parameters().front().device();
        // auto dtype = network_->parameters().front().dtype();
        // batchState = batchState.to(device, dtype);
        // batchMask = batchMask.to(device, dtype);
        //
        // auto [batchPolicy, batchValue] = network_->forward_masked(batchState, batchMask);
        // auto policyCpu = batchPolicy.to(torch::kFloat32).cpu();      // [B, ACTION_SPACE]
        // auto valueCpu = batchValue.to(torch::kFloat32).cpu();        // [B, 1]
        // auto valueAcc = valueCpu.accessor<float, 2>();


        // Phase 2: Batched random evaluation.
        if (pending.empty()) return;

        int64_t B = pending.size();

        // 1. Extract and stack masks to zero out illegal moves
        std::vector<torch::Tensor> masks;
        masks.reserve(B);
        for (auto& p : pending) {
            masks.push_back(p.mask);
        }
        auto batchMask = torch::stack(masks).to(torch::kFloat32); // [B, ACTION_SPACE]
        int64_t actionSpace = batchMask.size(1);

        // 2. Generate batched random policies [0, 1) and values [-1, 1)
        torch::Tensor batchPolicy = torch::rand({B, actionSpace}, torch::kFloat32);
        torch::Tensor batchValue = (torch::rand({B, 1}, torch::kFloat32) * 2.0f) - 1.0f;

        // 3. Mask illegal moves
        batchPolicy = batchPolicy * batchMask;

        // 4. Normalize the policy distributions across dimension 1 (action space)
        auto policySums = batchPolicy.sum(/*dim=*/1, /*keepdim=*/true); // [B, 1]
        auto maskSums = batchMask.sum(/*dim=*/1, /*keepdim=*/true);     // [B, 1]

        // Create a uniform fallback for any batch item where the random values were near zero
        auto uniformFallback = batchMask / (maskSums + 1e-8f);

        // If sum > 1e-6, normalize by the sum. Otherwise, apply the uniform fallback.
        batchPolicy = torch::where(policySums > 1e-6f, batchPolicy / policySums, uniformFallback);

        // 5. Map directly to CPU output variables for Phase 3
        auto policyCpu = batchPolicy;                              // [B, ACTION_SPACE]
        auto valueCpu = batchValue;                                // [B, 1]
        auto valueAcc = valueCpu.accessor<float, 2>();

        // Phase 3: expand each leaf (skipping duplicates already expanded by
        // an earlier item in the same batch) and back-propagate the NN value.
        for (std::size_t i = 0; i < pending.size(); ++i) {
            MCTSNode* leafNode = pending[i].path.back();
            float value = valueAcc[i][0];

            if (!leafNode->isExpanded) {
                leafNode->isExpanded = true;

                auto legalMoves = MoveGenerator::generateMoves(pending[i].leafState);
                if (!legalMoves.empty()) {
                    auto policySlice = policyCpu[i];                 // [ACTION_SPACE]
                    auto policyAcc = policySlice.accessor<float, 1>();
                    leafNode->children.reserve(legalMoves.size());
                    for (const auto& move : legalMoves) {
                        int action = ActionEncoder::moveToAction(move, pending[i].leafState);
                        auto child = std::make_unique<MCTSNode>();
                        child->parent = leafNode;
                        child->action = action;
                        child->move = move;
                        child->prior = policyAcc[action];
                        leafNode->children.push_back(std::move(child));
                    }
                }
                // If legalMoves is empty, treat as a pass-only node with value
                // coming from the NN estimate. Children stay empty; a future
                // selection that lands here will see isExpanded=true and
                // selectChild() returning null, which terminates the descent.
            }

            backpropWithVirtualLoss(pending[i].path, value);
        }
    }

    float MCTS::expand(MCTSNode* node, State& state) {
        node->isExpanded = true;

        // Check terminal state
        if (state.isTerminal()) {
            node->isTerminal = true;
            // Value from perspective of the player who just moved (parent's player)
            Color prevPlayer = rival(state.toMove());
            node->terminalValue = state.resultForColor(prevPlayer);
            return node->terminalValue;
        }

        auto legalMoves = MoveGenerator::generateMoves(state);
        if (legalMoves.empty()) {
            // No legal moves — pass
            node->isTerminal = false;
            return 0.0f;
        }

        // Neural network evaluation (device/dtype-aware for FP16 support)
        // torch::NoGradGuard no_grad;
        // auto stateTensor = StateEncoder::encode(state).unsqueeze(0); // [1, C, H, W]
        // auto mask = ActionEncoder::legalMask(state).unsqueeze(0);    // [1, ACTION_SPACE]
        //
        // auto device = network_->parameters().front().device();
        // auto dtype = network_->parameters().front().dtype();
        // stateTensor = stateTensor.to(device, dtype);
        // mask = mask.to(device, dtype);
        //
        // auto [policy, value] = network_->forward_masked(stateTensor, mask);
        //
        // float nodeValue = value.to(torch::kFloat32).item<float>();
        //
        // // Create children with priors from policy, caching the Move
        // auto policyAcc = policy.squeeze(0).to(torch::kFloat32).cpu(); // [ACTION_SPACE]


        // 1. Generate a random uniform policy tensor [1, ACTION_SPACE] in range [0, 1)
        torch::Tensor policy = torch::rand({1, ACTION_SPACE}, torch::kFloat32);

        // 2. Generate a random state value [1, 1] mapped to the range [-1, 1)
        torch::Tensor value = (torch::rand({1, 1}, torch::kFloat32) * 2.0f) - 1.0f;

        // 3. Mask out illegal moves to ensure they receive 0 probability
        auto mask = ActionEncoder::legalMask(state).unsqueeze(0);
        policy = policy * mask;

        // 4. Normalize the policy so the priors of legal moves sum to 1.0
        float policySum = policy.sum().item<float>();
        if (policySum > 1e-6f) {
            policy = policy / policySum;
        } else {
            // Fallback: Uniform distribution over legal moves if random values were near 0
            policy = mask / mask.sum();
        }

        float nodeValue = value.to(torch::kFloat32).item<float>();

        // Create children with priors from policy, caching the Move
        auto policyAcc = policy.squeeze(0).to(torch::kFloat32).cpu(); // [ACTION_SPACE]



        node->children.reserve(legalMoves.size());

        for (const auto& move : legalMoves) {
            int action = ActionEncoder::moveToAction(move, state);

            auto child = std::make_unique<MCTSNode>();
            child->parent = node;
            child->action = action;
            child->move = move;
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


    EngineStats MCTS::getStats() const {
        EngineStats stats;
        stats.nodesExplored = root_ ? root_->visitCount : 0;
        stats.maxDepth = maxSearchDepth_;

        if (root_) {
            // Extract statistics for every evaluated move from the root
            for (const auto& child : root_->children) {
                stats.moveEvaluations.push_back({
                    child->move,
                    child->visitCount,
                    child->qValue(),  // Safely uses the new effectiveVisits internal logic
                    child->prior      // Correct variable mapping for the new struct
                });
            }
        }
        return stats;
    }

} // namespace Hive::Learning