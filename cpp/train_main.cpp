#include "alphaZeroEngine/nn/headers/neural_net.h"
#include "alphaZeroEngine/training/headers/trainer.h"
#include "alphaZeroEngine/config/headers/config.h"

#include <iostream>
#include <string>

// Self-play training entry point
// Usage: ./hive_train [--iterations N] [--checkpoint-dir DIR] [--resume PATH]

int main(int argc, char* argv[]) {
    using namespace Hive::Learning;

    int numIterations = 50;
    std::string checkpointDir = "checkpoints/";
    std::string resumeFrom;

    // Parse CLI args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--iterations" && i + 1 < argc) {
            numIterations = std::stoi(argv[++i]);
        } else if (arg == "--checkpoint-dir" && i + 1 < argc) {
            checkpointDir = argv[++i];
        } else if (arg == "--resume" && i + 1 < argc) {
            resumeFrom = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: hive_train [OPTIONS]\n"
                      << "  --iterations N      Number of training iterations (default: 50)\n"
                      << "  --checkpoint-dir D  Checkpoint directory (default: checkpoints/)\n"
                      << "  --resume PATH       Resume from checkpoint\n";
            return 0;
        }
    }

    std::cout << "=== High-Hive AlphaZero Self-Play Training ===\n";
    std::cout << "Iterations: " << numIterations << "\n";
    std::cout << "MCTS simulations: " << MCTS_SIMS << "\n";
    std::cout << "Batch size: " << BATCH_SIZE << "\n";
    std::cout << "Self-play games per iteration: " << SELF_PLAY_GAMES << "\n\n";

    // Check for CUDA
    if (torch::cuda::is_available()) {
        std::cout << "CUDA available! Training on GPU.\n";
    } else {
        std::cout << "CUDA not available. Training on CPU (will be slow).\n";
    }

    // Create model and load checkpoint BEFORE constructing the Trainer.
    // The Trainer constructor snapshots model_ into bestModel_; if the load
    // happens after construction, bestModel_ stays as random init while
    // model_ has the pretrained weights, and the first self-play iteration
    // generates garbage games against a random opponent.
    HiveNet model;

    if (!resumeFrom.empty()) {
        // Load to CPU first, so checkpoints saved on CUDA can also be loaded
        // on CPU-only hosts. The to(kCUDA) below moves to GPU if available.
        torch::serialize::InputArchive archive;
        archive.load_from(resumeFrom, torch::kCPU);
        model->load(archive);
        std::cout << "Resumed from checkpoint: " << resumeFrom << "\n";
    }

    if (torch::cuda::is_available()) {
        model->to(torch::kCUDA);
    }

    // Create trainer (snapshots the loaded model into bestModel_)
    Trainer trainer(model, checkpointDir);

    // Run training
    trainer.train(numIterations);

    return 0;
}
