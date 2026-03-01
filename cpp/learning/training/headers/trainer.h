#pragma once

#include <torch/torch.h>
#include "nn/headers/neural_net.h"
#include "training/headers/replay_buffer.h"
#include "training/headers/self_play.h"
#include "config/headers/config.h"

#include <string>

// TRAINER
// Training loop for AlphaZero:
// - trainStep(): single gradient update on batch from replay buffer
// - runIteration(): complete iteration of self-play + training + evaluation
// - pretrain(): supervised pre-training from SGF data

namespace Hive::Learning {

    struct TrainLoss {
        float policyLoss;
        float valueLoss;
        float totalLoss;
    };

    class Trainer {
    public:
        Trainer(HiveNet model, const std::string& checkpointDir = "checkpoints/");

        // Single training step on batch from replay buffer
        TrainLoss trainStep(ReplayBuffer& buffer);

        // Run a complete AlphaZero iteration:
        // 1. Generate SELF_PLAY_GAMES games of self-play with best model
        // 2. Add samples to replay buffer
        // 3. Train TRAIN_STEPS_PER_ITER steps
        // 4. Evaluate new model vs best model
        // 5. If new model wins >= EVAL_THRESHOLD, promote it
        void runIteration(int iterationNum, ReplayBuffer& buffer);

        // Run the full training loop for numIterations
        void train(int numIterations);

        // Supervised pre-training from training samples
        void pretrain(const std::vector<TrainingSample>& data, int epochs = PRETRAIN_EPOCHS);

        // Save model checkpoint
        void saveCheckpoint(const std::string& name);

        // Load model checkpoint
        void loadCheckpoint(const std::string& name);

        // Evaluate model A vs model B. Returns win rate of A.
        static float evaluate(HiveNet modelA, HiveNet modelB, int numGames = EVAL_GAMES);

    private:
        HiveNet model_;
        HiveNet bestModel_; // Best model so far (for evaluation)
        std::string checkpointDir_;
        std::unique_ptr<torch::optim::SGD> optimizer_;
        int totalTrainSteps_;

        // Get current learning rate with cosine annealing
        float currentLearningRate() const;
    };

} // namespace Hive::Learning
