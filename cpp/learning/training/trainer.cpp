#include "training/headers/trainer.h"
#include "nn/headers/state_encoder.h"
#include "nn/headers/action_encoder.h"

#include <iostream>
#include <filesystem>
#include <cmath>

namespace Hive::Learning {

    Trainer::Trainer(HiveNet model, const std::string& checkpointDir)
        : model_(std::move(model))
        , checkpointDir_(checkpointDir)
        , totalTrainSteps_(0) {

        // Clone model as best model
        bestModel_ = HiveNet();
        // Copy parameters from model_ to bestModel_
        {
            torch::NoGradGuard no_grad;
            auto src_params = model_->parameters();
            auto dst_params = bestModel_->parameters();
            for (size_t i = 0; i < src_params.size(); ++i) {
                dst_params[i].copy_(src_params[i]);
            }
        }

        // Initialize SGD optimizer
        optimizer_ = std::make_unique<torch::optim::SGD>(
            model_->parameters(),
            torch::optim::SGDOptions(LEARNING_RATE)
                .momentum(MOMENTUM)
                .weight_decay(WEIGHT_DECAY)
        );

        // Create checkpoint directory
        std::filesystem::create_directories(checkpointDir_);
    }

    float Trainer::currentLearningRate() const {
        // Cosine annealing: LR decays from LEARNING_RATE to 0
        // over TRAIN_STEPS_PER_ITER * expected_iterations steps
        int totalExpectedSteps = TRAIN_STEPS_PER_ITER * 100; // ~100 iterations
        float progress = static_cast<float>(totalTrainSteps_) / static_cast<float>(totalExpectedSteps);
        progress = std::min(progress, 1.0f);
        return LEARNING_RATE * 0.5f * (1.0f + std::cos(M_PI * progress));
    }

    TrainLoss Trainer::trainStep(ReplayBuffer& buffer) {
        model_->train();

        auto batch = buffer.sampleBatch(BATCH_SIZE);

        // Move to same device as model
        auto device = model_->parameters().front().device();
        auto states = batch.states.to(device);
        auto targetPolicies = batch.policies.to(device);
        auto targetValues = batch.values.to(device);

        // Forward pass
        auto [logits, values] = model_->forward(states);

        // Policy loss: cross-entropy with MCTS visit distribution
        auto logSoftmax = torch::log_softmax(logits, /*dim=*/1);
        auto policyLoss = -(targetPolicies * logSoftmax).sum(1).mean();

        // Value loss: MSE with game outcome
        auto valueLoss = torch::mse_loss(values, targetValues);

        // Total loss
        auto totalLoss = policyLoss + valueLoss;

        // Update learning rate
        float lr = currentLearningRate();
        for (auto& group : optimizer_->param_groups()) {
            static_cast<torch::optim::SGDOptions&>(group.options()).lr(lr);
        }

        // Backward + step
        optimizer_->zero_grad();
        totalLoss.backward();
        optimizer_->step();

        ++totalTrainSteps_;

        return TrainLoss{
            policyLoss.item<float>(),
            valueLoss.item<float>(),
            totalLoss.item<float>()
        };
    }

    void Trainer::runIteration(int iterationNum, ReplayBuffer& buffer) {
        std::cout << "\n=== Iteration " << iterationNum << " ===\n";

        // 1. Self-play with best model
        std::cout << "Generating " << SELF_PLAY_GAMES << " self-play games...\n";
        {
            bestModel_->eval();
            SelfPlay selfPlay(bestModel_);
            selfPlay.playGames(SELF_PLAY_GAMES, buffer);
        }
        std::cout << "Buffer size: " << buffer.size() << " samples\n";

        // 2. Training
        std::cout << "Training " << TRAIN_STEPS_PER_ITER << " steps...\n";
        float avgPolicyLoss = 0.0f, avgValueLoss = 0.0f;
        for (int step = 0; step < TRAIN_STEPS_PER_ITER; ++step) {
            auto loss = trainStep(buffer);
            avgPolicyLoss += loss.policyLoss;
            avgValueLoss += loss.valueLoss;

            if ((step + 1) % 100 == 0) {
                std::cout << "  Step " << (step + 1) << "/" << TRAIN_STEPS_PER_ITER
                          << " | Policy: " << (avgPolicyLoss / (step + 1))
                          << " | Value: " << (avgValueLoss / (step + 1))
                          << " | LR: " << currentLearningRate() << "\n";
            }
        }
        avgPolicyLoss /= TRAIN_STEPS_PER_ITER;
        avgValueLoss /= TRAIN_STEPS_PER_ITER;
        std::cout << "Average loss - Policy: " << avgPolicyLoss
                  << " | Value: " << avgValueLoss << "\n";

        // 3. Evaluation: new model vs best model
        std::cout << "Evaluating new model vs best model (" << EVAL_GAMES << " games)...\n";
        model_->eval();
        float winRate = evaluate(model_, bestModel_, EVAL_GAMES);
        std::cout << "New model win rate: " << (winRate * 100.0f) << "%\n";

        // 4. Promote if good enough
        if (winRate >= EVAL_THRESHOLD) {
            std::cout << "Promoting new model as best!\n";
            // Copy parameters to best model
            torch::NoGradGuard no_grad;
            auto src_params = model_->parameters();
            auto dst_params = bestModel_->parameters();
            for (size_t i = 0; i < src_params.size(); ++i) {
                dst_params[i].copy_(src_params[i]);
            }
            saveCheckpoint("best_iter_" + std::to_string(iterationNum));
        } else {
            std::cout << "New model did not reach threshold. Keeping best model.\n";
            // Revert to best model parameters
            torch::NoGradGuard no_grad;
            auto src_params = bestModel_->parameters();
            auto dst_params = model_->parameters();
            for (size_t i = 0; i < src_params.size(); ++i) {
                dst_params[i].copy_(src_params[i]);
            }
        }

        saveCheckpoint("latest_iter_" + std::to_string(iterationNum));
    }

    void Trainer::train(int numIterations) {
        ReplayBuffer buffer;

        for (int iter = 1; iter <= numIterations; ++iter) {
            runIteration(iter, buffer);
        }

        std::cout << "\nTraining complete after " << numIterations << " iterations.\n";
    }

    void Trainer::pretrain(const std::vector<TrainingSample>& data, int epochs) {
        std::cout << "Pre-training on " << data.size() << " samples for "
                  << epochs << " epochs...\n";

        model_->train();

        // Create temporary replay buffer with supervised data
        ReplayBuffer buffer(static_cast<int>(data.size()));
        buffer.addBatch(data);

        // Use higher learning rate for pre-training
        auto pretrainOptimizer = torch::optim::SGD(
            model_->parameters(),
            torch::optim::SGDOptions(PRETRAIN_LR)
                .momentum(MOMENTUM)
                .weight_decay(WEIGHT_DECAY)
        );

        for (int epoch = 0; epoch < epochs; ++epoch) {
            int stepsPerEpoch = std::max(1, static_cast<int>(data.size()) / BATCH_SIZE);
            float epochPolicyLoss = 0.0f, epochValueLoss = 0.0f;

            for (int step = 0; step < stepsPerEpoch; ++step) {
                auto batch = buffer.sampleBatch(BATCH_SIZE);

                auto device = model_->parameters().front().device();
                auto states = batch.states.to(device);
                auto targetPolicies = batch.policies.to(device);
                auto targetValues = batch.values.to(device);

                auto [logits, values] = model_->forward(states);

                auto logSoftmax = torch::log_softmax(logits, /*dim=*/1);
                auto policyLoss = -(targetPolicies * logSoftmax).sum(1).mean();
                auto valueLoss = torch::mse_loss(values, targetValues);
                auto totalLoss = policyLoss + valueLoss;

                pretrainOptimizer.zero_grad();
                totalLoss.backward();
                pretrainOptimizer.step();

                epochPolicyLoss += policyLoss.item<float>();
                epochValueLoss += valueLoss.item<float>();
            }

            epochPolicyLoss /= stepsPerEpoch;
            epochValueLoss /= stepsPerEpoch;

            std::cout << "Epoch " << (epoch + 1) << "/" << epochs
                      << " | Policy: " << epochPolicyLoss
                      << " | Value: " << epochValueLoss << "\n";
        }

        // Copy trained parameters to best model
        {
            torch::NoGradGuard no_grad;
            auto src_params = model_->parameters();
            auto dst_params = bestModel_->parameters();
            for (size_t i = 0; i < src_params.size(); ++i) {
                dst_params[i].copy_(src_params[i]);
            }
        }

        saveCheckpoint("pretrained");
        std::cout << "Pre-training complete.\n";
    }

    void Trainer::pretrainFromDisk(const std::string& batchDir, int epochs) {
        // Collect all batch file prefixes
        std::vector<std::string> batchPrefixes;
        for (const auto& entry : std::filesystem::directory_iterator(batchDir)) {
            auto fname = entry.path().filename().string();
            // Look for *_states.pt files to identify batch prefixes
            if (fname.size() > 10 && fname.substr(fname.size() - 10) == "_states.pt") {
                std::string prefix = entry.path().string();
                prefix = prefix.substr(0, prefix.size() - 10); // strip "_states.pt"
                batchPrefixes.push_back(prefix);
            }
        }
        std::sort(batchPrefixes.begin(), batchPrefixes.end());

        if (batchPrefixes.empty()) {
            std::cerr << "No batch files found in " << batchDir << "\n";
            return;
        }

        // Count total samples across all batches
        int totalSamples = 0;
        for (const auto& prefix : batchPrefixes) {
            torch::Tensor s;
            torch::load(s, prefix + "_states.pt");
            totalSamples += static_cast<int>(s.size(0));
        }

        std::cout << "Pre-training from " << batchPrefixes.size() << " batch files, "
                  << totalSamples << " total samples, " << epochs << " epochs\n";

        model_->train();

        auto pretrainOptimizer = torch::optim::SGD(
            model_->parameters(),
            torch::optim::SGDOptions(PRETRAIN_LR)
                .momentum(MOMENTUM)
                .weight_decay(WEIGHT_DECAY)
        );

        auto device = model_->parameters().front().device();

        for (int epoch = 0; epoch < epochs; ++epoch) {
            float epochPolicyLoss = 0.0f, epochValueLoss = 0.0f;
            int epochSteps = 0;

            for (const auto& prefix : batchPrefixes) {
                // Load one batch file into memory
                torch::Tensor states, policies, values;
                torch::load(states, prefix + "_states.pt");
                torch::load(policies, prefix + "_policies.pt");
                torch::load(values, prefix + "_values.pt");

                int n = static_cast<int>(states.size(0));

                // Shuffle indices for this batch file
                auto perm = torch::randperm(n, torch::kLong);

                // Mini-batch loop over this file
                for (int offset = 0; offset + BATCH_SIZE <= n; offset += BATCH_SIZE) {
                    auto idx = perm.slice(0, offset, offset + BATCH_SIZE);
                    auto batchStates = states.index_select(0, idx).to(device);
                    auto batchPolicies = policies.index_select(0, idx).to(device);
                    auto batchValues = values.index_select(0, idx).to(device);

                    auto [logits, vals] = model_->forward(batchStates);

                    auto logSoftmax = torch::log_softmax(logits, 1);
                    auto policyLoss = -(batchPolicies * logSoftmax).sum(1).mean();
                    auto valueLoss = torch::mse_loss(vals, batchValues);
                    auto totalLoss = policyLoss + valueLoss;

                    pretrainOptimizer.zero_grad();
                    totalLoss.backward();
                    pretrainOptimizer.step();

                    epochPolicyLoss += policyLoss.item<float>();
                    epochValueLoss += valueLoss.item<float>();
                    ++epochSteps;
                }
            }

            if (epochSteps > 0) {
                epochPolicyLoss /= epochSteps;
                epochValueLoss /= epochSteps;
            }
            std::cout << "Epoch " << (epoch + 1) << "/" << epochs
                      << " | Policy: " << epochPolicyLoss
                      << " | Value: " << epochValueLoss
                      << " | Steps: " << epochSteps << "\n";
        }

        // Copy trained parameters to best model
        {
            torch::NoGradGuard no_grad;
            auto src_params = model_->parameters();
            auto dst_params = bestModel_->parameters();
            for (size_t i = 0; i < src_params.size(); ++i) {
                dst_params[i].copy_(src_params[i]);
            }
        }

        saveCheckpoint("pretrained");
        std::cout << "Pre-training complete.\n";
    }

    float Trainer::evaluate(HiveNet modelA, HiveNet modelB, int numGames) {
        modelA->eval();
        modelB->eval();

        int winsA = 0, draws = 0;

        for (int game = 0; game < numGames; ++game) {
            State state;
            // Alternate colors: even games modelA=White, odd games modelA=Black
            bool aIsWhite = (game % 2 == 0);

            MCTS mctsA(modelA);
            MCTS mctsB(modelB);

            int moveCount = 0;
            while (!state.isTerminal() && moveCount < MAX_GAME_LENGTH) {
                bool isModelATurn = (state.toMove() == Color::White) == aIsWhite;
                MCTS& activeMcts = isModelATurn ? mctsA : mctsB;

                auto moveVisits = activeMcts.search(state, /*addNoise=*/false);

                if (moveVisits.empty()) {
                    Move passMove;
                    passMove.type = Move::Pass;
                    state.applyMove(passMove);
                    activeMcts.reset();
                } else {
                    // Select best move (no exploration during eval)
                    std::vector<int> visits;
                    visits.reserve(moveVisits.size());
                    for (const auto& [m, v] : moveVisits) {
                        visits.push_back(v);
                    }
                    int bestIdx = MCTS::selectAction(visits, /*temperature=*/0.0f);
                    const Move& bestMove = moveVisits[bestIdx].first;
                    int bestAction = ActionEncoder::moveToAction(bestMove, state);
                    state.applyMove(bestMove);

                    // Advance both trees (action computed before apply)
                    mctsA.advanceTree(bestAction);
                    mctsB.advanceTree(bestAction);
                }

                ++moveCount;
            }

            // Score the game
            if (!state.isTerminal()) {
                ++draws;
            } else {
                float aOutcome;
                if (aIsWhite) {
                    aOutcome = state.resultForColor(Color::White);
                } else {
                    aOutcome = state.resultForColor(Color::Black);
                }
                if (aOutcome > 0.0f) ++winsA;
                else if (aOutcome == 0.0f) ++draws;
            }

            if ((game + 1) % 50 == 0) {
                std::cout << "  Eval: " << (game + 1) << "/" << numGames
                          << " (wins: " << winsA << ", draws: " << draws << ")\n";
            }
        }

        return (static_cast<float>(winsA) + 0.5f * static_cast<float>(draws))
               / static_cast<float>(numGames);
    }

    void Trainer::saveCheckpoint(const std::string& name) {
        std::string path = checkpointDir_ + name + ".pt";
        torch::save(model_, path);
        std::cout << "Saved checkpoint: " << path << "\n";
    }

    void Trainer::loadCheckpoint(const std::string& name) {
        std::string path = checkpointDir_ + name + ".pt";
        torch::load(model_, path);
        std::cout << "Loaded checkpoint: " << path << "\n";
    }

} // namespace Hive::Learning
