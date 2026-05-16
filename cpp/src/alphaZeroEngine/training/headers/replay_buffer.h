#pragma once

#include <torch/torch.h>
#include "alphaZeroEngine/config/headers/config.h"

#include <vector>
#include <mutex>
#include <random>

// REPLAY BUFFER
// Circular buffer of training samples for the AlphaZero pipeline.
// Thread-safe for concurrent self-play and training.

namespace Hive::Learning {

    struct TrainingSample {
        torch::Tensor planes;   // [NUM_CHANNELS, GRID_SIZE, GRID_SIZE]
        torch::Tensor scalars;  // [NUM_SCALAR_FEATURES]
        torch::Tensor policy;   // [ACTION_SPACE] - MCTS visit distribution
        float value;            // Game outcome from this position (+1/-1/0)
    };

    struct TrainingBatch {
        torch::Tensor planes;   // [B, NUM_CHANNELS, GRID_SIZE, GRID_SIZE]
        torch::Tensor scalars;  // [B, NUM_SCALAR_FEATURES]
        torch::Tensor policies; // [B, ACTION_SPACE]
        torch::Tensor values;   // [B, 1]
    };

    class ReplayBuffer {
    public:
        explicit ReplayBuffer(int capacity = REPLAY_BUFFER_SIZE);

        // Add a single sample
        void add(TrainingSample sample);

        // Add multiple samples from a game
        void addBatch(const std::vector<TrainingSample>& samples);

        // Sample a random training batch
        TrainingBatch sampleBatch(int batchSize = BATCH_SIZE);

        // Current number of samples in the buffer
        int size() const;

        // Clear the buffer
        void clear();

    private:
        std::vector<TrainingSample> buffer_;
        int capacity_;
        int writePos_;
        int currentSize_;
        mutable std::mutex mutex_;
        std::mt19937 rng_;
    };

} // namespace Hive::Learning
