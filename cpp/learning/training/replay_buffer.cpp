#include "training/headers/replay_buffer.h"

#include <algorithm>
#include <numeric>

namespace Hive::Learning {

    ReplayBuffer::ReplayBuffer(int capacity)
        : capacity_(capacity)
        , writePos_(0)
        , currentSize_(0)
        , rng_(std::random_device{}()) {
        buffer_.resize(capacity);
    }

    void ReplayBuffer::add(TrainingSample sample) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_[writePos_] = std::move(sample);
        writePos_ = (writePos_ + 1) % capacity_;
        if (currentSize_ < capacity_) ++currentSize_;
    }

    void ReplayBuffer::addBatch(const std::vector<TrainingSample>& samples) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& sample : samples) {
            buffer_[writePos_] = sample;
            writePos_ = (writePos_ + 1) % capacity_;
            if (currentSize_ < capacity_) ++currentSize_;
        }
    }

    TrainingBatch ReplayBuffer::sampleBatch(int batchSize) {
        std::lock_guard<std::mutex> lock(mutex_);

        int n = std::min(batchSize, currentSize_);

        // Generate random indices
        std::vector<int> indices(currentSize_);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng_);
        indices.resize(n);

        // Stack into tensors
        std::vector<torch::Tensor> states, policies, values;
        states.reserve(n);
        policies.reserve(n);
        values.reserve(n);

        for (int idx : indices) {
            states.push_back(buffer_[idx].state);
            policies.push_back(buffer_[idx].policy);
            values.push_back(torch::tensor({buffer_[idx].value}));
        }

        TrainingBatch batch;
        batch.states = torch::stack(states);
        batch.policies = torch::stack(policies);
        batch.values = torch::stack(values);

        return batch;
    }

    int ReplayBuffer::size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentSize_;
    }

    void ReplayBuffer::clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        writePos_ = 0;
        currentSize_ = 0;
    }

} // namespace Hive::Learning
