#include "nn/headers/neural_net.h"
#include "training/headers/trainer.h"
#include "config/headers/config.h"

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

    // Create model
    HiveNet model;
    if (torch::cuda::is_available()) {
        model->to(torch::kCUDA);
    }

    // Create trainer
    Trainer trainer(model, checkpointDir);

    // Resume from checkpoint if specified
    if (!resumeFrom.empty()) {
        trainer.loadCheckpoint(resumeFrom);
        std::cout << "Resumed from checkpoint: " << resumeFrom << "\n";
    }

    // Run training
    trainer.train(numIterations);

    return 0;
}
