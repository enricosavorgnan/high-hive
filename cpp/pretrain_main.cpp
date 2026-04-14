#include "alphaZeroEngine/nn/headers/neural_net.h"
#include "alphaZeroEngine/training/headers/trainer.h"
#include "alphaZeroEngine/data/headers/sgf_parser.h"
#include "alphaZeroEngine/config/headers/config.h"

#include <iostream>
#include <string>

// Supervised pre-training entry point
// Usage: ./hive_pretrain --sgf-dir DIR [--epochs N] [--checkpoint-dir DIR] [--batch-dir DIR] [--skip-files N] [--parse-only]

int main(int argc, char* argv[]) {
    using namespace Hive::Learning;

    std::string sgfDir;
    int epochs = PRETRAIN_EPOCHS;
    std::string checkpointDir = "checkpoints/";
    std::string batchDir = "pretrain_batches/";
    int skipFiles = 0;
    int startEpoch = 0;
    bool parseOnly = false;
    bool trainOnly = false;

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
        } else if (arg == "--skip-files" && i + 1 < argc) {
            skipFiles = std::stoi(argv[++i]);
        } else if (arg == "--start-epoch" && i + 1 < argc) {
            startEpoch = std::stoi(argv[++i]);
        } else if (arg == "--parse-only") {
            parseOnly = true;
        } else if (arg == "--train-only") {
            trainOnly = true;
        } else if (arg == "--help") {
            std::cout << "Usage: hive_pretrain [OPTIONS]\n"
                      << "  --sgf-dir DIR       Directory containing SGF files (required unless --train-only)\n"
                      << "  --epochs N          Number of epochs (default: 30)\n"
                      << "  --checkpoint-dir D  Checkpoint directory (default: checkpoints/)\n"
                      << "  --batch-dir D       Directory for batch files (default: pretrain_batches/)\n"
                      << "  --skip-files N      Skip first N SGF files (for resuming)\n"
                      << "  --start-epoch N     Resume training from epoch N (loads checkpoint)\n"
                      << "  --parse-only        Only parse SGF files, don't train\n"
                      << "  --train-only        Only train from existing batch files, don't parse\n";
            return 0;
        }
    }

    std::cout << "=== High-Hive Supervised Pre-Training ===\n";

    // Check for CUDA
    if (torch::cuda::is_available()) {
        std::cout << "CUDA available! Training on GPU.\n";
    } else {
        std::cout << "CUDA not available. Training on CPU (will be slow).\n";
    }

    // Step 1: Parse SGF files (unless --train-only)
    if (!trainOnly) {
        if (sgfDir.empty()) {
            std::cerr << "Error: --sgf-dir is required (unless --train-only)\n";
            return 1;
        }
        std::cout << "\nStep 1: Parsing SGF games from " << sgfDir << "...\n";
        int totalSamples = SgfParser::processDirectory(sgfDir, batchDir, skipFiles);

        if (totalSamples == 0 && !parseOnly) {
            std::cerr << "Error: No valid training samples found\n";
            return 1;
        }

        if (parseOnly) {
            std::cout << "\nParsing complete. Batch files saved to " << batchDir << "\n";
            return 0;
        }
    }

    // Step 2: Train from disk batches
    std::cout << "\nStep 2: Training from " << batchDir << "...\n";
    HiveNet model;
    if (torch::cuda::is_available()) {
        model->to(torch::kCUDA);
    }

    Trainer trainer(model, checkpointDir);
    trainer.pretrainFromDisk(batchDir, epochs, startEpoch);

    std::cout << "\nPre-training complete. Checkpoint saved to " << checkpointDir << "\n";

    return 0;
}
