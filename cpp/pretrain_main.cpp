#include "nn/headers/neural_net.h"
#include "training/headers/trainer.h"
#include "data/headers/sgf_parser.h"
#include "config/headers/config.h"

#include <iostream>
#include <string>

// Supervised pre-training entry point
// Usage: ./hive_pretrain --sgf-dir DIR [--epochs N] [--checkpoint-dir DIR] [--batch-dir DIR]

int main(int argc, char* argv[]) {
    using namespace Hive::Learning;

    std::string sgfDir;
    int epochs = PRETRAIN_EPOCHS;
    std::string checkpointDir = "checkpoints/";
    std::string batchDir = "pretrain_batches/";

    // Parse CLI args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--sgf-dir" && i + 1 < argc) {
            sgfDir = argv[++i];
        } else if (arg == "--epochs" && i + 1 < argc) {
            epochs = std::stoi(argv[++i]);
        } else if (arg == "--checkpoint-dir" && i + 1 < argc) {
            checkpointDir = argv[++i];
        } else if (arg == "--batch-dir" && i + 1 < argc) {
            batchDir = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: hive_pretrain [OPTIONS]\n"
                      << "  --sgf-dir DIR       Directory containing SGF files (required)\n"
                      << "  --epochs N          Number of epochs (default: 30)\n"
                      << "  --checkpoint-dir D  Checkpoint directory (default: checkpoints/)\n"
                      << "  --batch-dir D       Directory for intermediate batch files (default: pretrain_batches/)\n";
            return 0;
        }
    }

    if (sgfDir.empty()) {
        std::cerr << "Error: --sgf-dir is required\n";
        return 1;
    }

    std::cout << "=== High-Hive Supervised Pre-Training ===\n";
    std::cout << "SGF directory: " << sgfDir << "\n";
    std::cout << "Batch directory: " << batchDir << "\n";
    std::cout << "Epochs: " << epochs << "\n\n";

    // Check for CUDA
    if (torch::cuda::is_available()) {
        std::cout << "CUDA available! Training on GPU.\n";
    } else {
        std::cout << "CUDA not available. Training on CPU (will be slow).\n";
    }

    // Step 1: Parse SGF files and save training samples to disk in batches
    std::cout << "\nStep 1: Parsing SGF games from " << sgfDir << "...\n";
    int totalSamples = SgfParser::processDirectory(sgfDir, batchDir);

    if (totalSamples == 0) {
        std::cerr << "Error: No valid training samples found\n";
        return 1;
    }

    // Step 2: Train from disk batches
    std::cout << "\nStep 2: Training...\n";
    HiveNet model;
    if (torch::cuda::is_available()) {
        model->to(torch::kCUDA);
    }

    Trainer trainer(model, checkpointDir);
    trainer.pretrainFromDisk(batchDir, epochs);

    std::cout << "\nPre-training complete. Checkpoint saved to " << checkpointDir << "\n";

    return 0;
}
