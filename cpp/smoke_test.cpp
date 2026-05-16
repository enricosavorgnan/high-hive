#include "alphaZeroEngine/nn/headers/neural_net.h"
#include "alphaZeroEngine/nn/headers/state_encoder.h"
#include "alphaZeroEngine/nn/headers/action_encoder.h"
#include "alphaZeroEngine/mcts/headers/mcts.h"
#include "alphaZeroEngine/config/headers/config.h"
#include "state.h"

#include <torch/torch.h>
#include <chrono>
#include <iostream>

// hive_smoke: end-to-end sanity check for the rewritten model.
//   1. Build a random-init HiveNet
//   2. Encode an empty State, check planes/scalars shapes
//   3. Forward pass, check policy/value shapes
//   4. Run a short MCTS search and report nodes/sec on this machine
//   5. Optionally save the random-init weights to a checkpoint path
//
// Usage:
//   ./hive_smoke [--save PATH] [--budget MS]
//
// On CUDA-available systems the model is moved to GPU + FP16 for the MCTS
// timing leg (mirrors what AlphaZeroEngine does at runtime).

int main(int argc, char* argv[]) {
    using namespace Hive;
    using namespace Hive::Learning;

    std::string savePath;
    int budgetMs = 5000;
    bool useCuda = torch::cuda::is_available();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--save" && i + 1 < argc) savePath = argv[++i];
        else if (arg == "--budget" && i + 1 < argc) budgetMs = std::stoi(argv[++i]);
        else if (arg == "--cpu") useCuda = false;
    }

    std::cout << "=== HiveNet smoke test ===\n"
              << "Device: " << (useCuda ? "CUDA + FP16" : "CPU FP32") << "\n"
              << "Body: " << NUM_RESIDUAL_BLOCKS << "x" << NUM_FILTERS << "\n"
              << "Channels: " << NUM_CHANNELS << " planes + " << NUM_SCALAR_FEATURES << " scalars\n"
              << "Action space: " << ACTION_SPACE
              << " (planes " << POLICY_PLANES
              << " x " << GRID_SIZE << "x" << GRID_SIZE
              << " + 1 pass)\n\n";

    torch::manual_seed(42);
    HiveNet net;
    net->eval();

    // Encode an initial state and a state with one piece placed.
    State state;
    {
        Move first;
        first.type = Move::Place;
        first.piece = Piece{Color::White, Bug::Ant, 1};
        first.to = Coord{0, 0};
        state.applyMove(first);
    }

    auto encoded = StateEncoder::encode(state);
    std::cout << "Encoded planes shape:  [" << encoded.planes.size(0)
              << ", " << encoded.planes.size(1)
              << ", " << encoded.planes.size(2) << "]\n";
    std::cout << "Encoded scalars shape: [" << encoded.scalars.size(0) << "]\n";

    auto mask = ActionEncoder::legalMask(state);
    int legalCount = static_cast<int>(mask.sum().item<float>());
    std::cout << "Legal mask sum:        " << legalCount
              << " (out of " << ACTION_SPACE << " actions)\n\n";

    // Forward pass on a batch of 1.
    {
        torch::NoGradGuard ng;
        auto planes  = encoded.planes.unsqueeze(0);
        auto scalars = encoded.scalars.unsqueeze(0);
        auto maskB   = mask.unsqueeze(0);
        auto [policy, value] = net->forward_masked(planes, scalars, maskB);
        std::cout << "Forward policy shape:  [" << policy.size(0) << ", " << policy.size(1) << "]\n";
        std::cout << "Forward value shape:   [" << value.size(0) << ", " << value.size(1) << "]\n";
        std::cout << "Value (random init):   " << value.item<float>() << "\n\n";
    }

    // Save checkpoint if requested (used downstream to feed AlphaZeroEngine).
    if (!savePath.empty()) {
        torch::save(net, savePath);
        std::cout << "Saved random-init checkpoint to " << savePath << "\n\n";
    }

    // MCTS timing run. Mirror AlphaZeroEngine's device/dtype path.
    if (useCuda) {
        net->to(torch::kCUDA);
        net->to(torch::kHalf);
    }

    MCTS mcts(net);
    auto start = std::chrono::steady_clock::now();
    auto results = mcts.searchWithBudget(state, std::chrono::milliseconds(budgetMs), /*addNoise=*/false);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    int totalVisits = 0;
    for (const auto& [m, v] : results) totalVisits += v;

    std::cout << "MCTS search:\n"
              << "  budget:        " << budgetMs << " ms\n"
              << "  elapsed:       " << elapsed << " ms\n"
              << "  root children: " << results.size() << "\n"
              << "  total visits:  " << totalVisits << "\n"
              << "  visits/sec:    "
              << (elapsed > 0 ? (1000LL * totalVisits / elapsed) : 0) << "\n";

    return 0;
}
