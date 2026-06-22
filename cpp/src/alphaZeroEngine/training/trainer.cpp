#include "alphaZeroEngine/training/headers/trainer.h"
#include "alphaZeroEngine/nn/headers/state_encoder.h"
#include "alphaZeroEngine/nn/headers/action_encoder.h"

#include <iostream>
#include <filesystem>
#include <cmath>
#include <limits>

namespace Hive::Learning {

    namespace {
        // Weighted mean squared error for the value head.
        //
        // Standard MSE averages the per-sample squared errors uniformly. We
        // instead want to ignore samples whose value label is meaningless
        // (no-result SGF games, max-length self-play cutoffs, draws): they
        // arrive in the batch with weight=0 so the policy head still trains
        // from them while their value contribution is dropped.
        //
        // Implementation: sum(weight * (pred - target)^2) / max(sum(weight), eps).
        // The clamp on the denominator keeps the loss finite (and the gradient
        // zero) on the degenerate case where every sample in the batch is
        // masked.
        torch::Tensor weightedValueLoss(const torch::Tensor& pred,
                                        const torch::Tensor& target,
                                        const torch::Tensor& weight) {
            auto diff = pred - target;
            auto sq = diff * diff;
            auto weighted = sq * weight;
            auto denom = weight.sum().clamp_min(1e-6f);
            return weighted.sum() / denom;
        }
    }

    Trainer::Trainer(HiveNet model, const std::string& checkpointDir)
        : model_(std::move(model))
        , checkpointDir_(checkpointDir)
        , totalTrainSteps_(0) {

        // Clone model as best model
        bestModel_ = HiveNet();
        // Mirror model_'s device/dtype so self-play inference runs where
        // model_ lives (CUDA when available). Without this, bestModel_ stays
        // on CPU and self-play silently falls back to CPU inference — no
        // crash, just orders of magnitude slower.
        auto srcDevice = model_->parameters().front().device();
        bestModel_->to(srcDevice);
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
        auto planes = batch.planes.to(device);
        auto scalars = batch.scalars.to(device);
        auto targetPolicies = batch.policies.to(device);
        auto targetValues = batch.values.to(device);
        auto valueWeights = batch.value_weights.to(device);

        // Forward pass
        auto [logits, values] = model_->forward(planes, scalars);

        // Policy loss: cross-entropy with MCTS visit distribution
        auto logSoftmax = torch::log_softmax(logits, /*dim=*/1);
        auto policyLoss = -(targetPolicies * logSoftmax).sum(1).mean();

        // Value loss: MSE masked by per-sample weight (see weightedValueLoss).
        auto valueLoss = weightedValueLoss(values, targetValues, valueWeights);

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

        // 2b. Crash-safe checkpoint of the trained model BEFORE eval.
        // Eval can take many hours and SLURM may kill the job mid-eval; this
        // ensures the next sbatch resumes from the trained weights instead of
        // re-running self-play from scratch. The end-of-iteration save below
        // overwrites this with the final (promoted-or-reverted) model.
        saveCheckpoint("latest_iter_" + std::to_string(iterationNum));
        std::cout << "Post-training checkpoint saved (will be overwritten after eval).\n";

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
                auto planes = batch.planes.to(device);
                auto scalars = batch.scalars.to(device);
                auto targetPolicies = batch.policies.to(device);
                auto targetValues = batch.values.to(device);
                auto valueWeights = batch.value_weights.to(device);

                auto [logits, values] = model_->forward(planes, scalars);

                auto logSoftmax = torch::log_softmax(logits, /*dim=*/1);
                auto policyLoss = -(targetPolicies * logSoftmax).sum(1).mean();
                auto valueLoss = weightedValueLoss(values, targetValues, valueWeights);
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

    void Trainer::pretrainFromDisk(const std::string& batchDir, int epochs, int startEpoch) {
        // Each batch is a quintuple: _planes.pt, _scalars.pt, _policies.pt,
        // _values.pt and (since the masked-value-loss change) _weights.pt.
        // We anchor discovery on _planes.pt and require the other four to
        // exist before keeping the prefix; _weights.pt is treated as optional
        // for backward compatibility with batches generated before the mask
        // was introduced (we default the weight to 1.0 in that case below).
        std::vector<std::string> batchPrefixes;
        for (const auto& entry : std::filesystem::directory_iterator(batchDir)) {
            auto fname = entry.path().filename().string();
            constexpr const char* SUFFIX = "_planes.pt";
            constexpr size_t SUFFIX_LEN = 10;
            if (fname.size() > SUFFIX_LEN &&
                fname.substr(fname.size() - SUFFIX_LEN) == SUFFIX) {
                std::string prefix = entry.path().string();
                prefix = prefix.substr(0, prefix.size() - SUFFIX_LEN);
                if (std::filesystem::exists(prefix + "_scalars.pt") &&
                    std::filesystem::exists(prefix + "_policies.pt") &&
                    std::filesystem::exists(prefix + "_values.pt")) {
                    batchPrefixes.push_back(prefix);
                } else {
                    std::cerr << "[WARN] Skipping incomplete batch: " << prefix << "\n";
                }
            }
        }
        std::sort(batchPrefixes.begin(), batchPrefixes.end());

        if (batchPrefixes.empty()) {
            std::cerr << "No batch files found in " << batchDir << "\n";
            return;
        }

        int totalBatches = static_cast<int>(batchPrefixes.size());
        std::cout << "Pre-training from " << totalBatches << " batch files, "
                  << "epochs " << (startEpoch + 1) << " to " << epochs << std::endl;

        // Resume from checkpoint if requested
        if (startEpoch > 0) {
            std::string ckptName = "pretrained_epoch_" + std::to_string(startEpoch);
            try {
                loadCheckpoint(ckptName);
                std::cout << "Resumed from " << ckptName << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error: cannot load checkpoint '" << ckptName << "': " << e.what() << "\n";
                return;
            }
        }

        model_->train();
        auto device = model_->parameters().front().device();

        constexpr int PRETRAIN_BATCH = 64;

        // Create optimizer AFTER potential checkpoint load
        auto pretrainOptimizer = torch::optim::SGD(
            model_->parameters(),
            torch::optim::SGDOptions(PRETRAIN_LR)
                .momentum(MOMENTUM)
                .weight_decay(WEIGHT_DECAY)
        );

        // Early stopping state
        float bestLoss = std::numeric_limits<float>::max();
        int patienceCounter = 0;
        constexpr int PATIENCE = 5;

        for (int epoch = startEpoch; epoch < epochs; ++epoch) {
            float epochPolicyLoss = 0.0f, epochValueLoss = 0.0f;
            int epochSteps = 0;
            int batchesProcessed = 0;
            std::vector<std::string> skippedBatches;

            for (const auto& prefix : batchPrefixes) {
                try {
                    torch::Tensor planes, scalars, policies, values, valueWeights;
                    torch::load(planes, prefix + "_planes.pt");
                    torch::load(scalars, prefix + "_scalars.pt");
                    torch::load(policies, prefix + "_policies.pt");
                    torch::load(values, prefix + "_values.pt");
                    // _weights.pt is optional: batches produced before the
                    // masked-value-loss change don't include it, and we
                    // treat every sample as fully trusted (weight=1.0) in
                    // that case.
                    if (std::filesystem::exists(prefix + "_weights.pt")) {
                        torch::load(valueWeights, prefix + "_weights.pt");
                    } else {
                        valueWeights = torch::ones_like(values);
                    }

                    int n = static_cast<int>(planes.size(0));
                    auto perm = torch::randperm(n, torch::kLong);

                    for (int offset = 0; offset + PRETRAIN_BATCH <= n; offset += PRETRAIN_BATCH) {
                        auto idx = perm.slice(0, offset, offset + PRETRAIN_BATCH);
                        auto batchPlanes = planes.index_select(0, idx).to(device);
                        auto batchScalars = scalars.index_select(0, idx).to(device);
                        auto batchPolicies = policies.index_select(0, idx).to(device);
                        auto batchValues = values.index_select(0, idx).to(device);
                        auto batchValueWeights = valueWeights.index_select(0, idx).to(device);

                        auto [logits, vals] = model_->forward(batchPlanes, batchScalars);

                        auto logSoftmax = torch::log_softmax(logits, 1);
                        auto policyLoss = -(batchPolicies * logSoftmax).sum(1).mean();
                        auto valueLoss = weightedValueLoss(vals, batchValues, batchValueWeights);
                        auto totalLoss = policyLoss + valueLoss;

                        pretrainOptimizer.zero_grad();
                        totalLoss.backward();
                        pretrainOptimizer.step();

                        epochPolicyLoss += policyLoss.item<float>();
                        epochValueLoss += valueLoss.item<float>();
                        ++epochSteps;
                    }

                    ++batchesProcessed;

                    // Progress every 50 batch files
                    if (batchesProcessed % 50 == 0) {
                        int pct = 100 * batchesProcessed / totalBatches;
                        float avgP = epochSteps > 0 ? epochPolicyLoss / epochSteps : 0;
                        float avgV = epochSteps > 0 ? epochValueLoss / epochSteps : 0;
                        std::cout << "  [Epoch " << (epoch + 1) << "/" << epochs << "] "
                                  << batchesProcessed << "/" << totalBatches
                                  << " batches (" << pct << "%) "
                                  << "| Policy: " << avgP << " | Value: " << avgV
                                  << std::endl;
                    }
                } catch (const std::exception& e) {
                    skippedBatches.push_back(
                        std::filesystem::path(prefix).filename().string() + ": " + e.what());
                }
            }

            // Epoch summary
            float avgPolicyLoss = epochSteps > 0 ? epochPolicyLoss / epochSteps : 0;
            float avgValueLoss = epochSteps > 0 ? epochValueLoss / epochSteps : 0;
            float totalLoss = avgPolicyLoss + avgValueLoss;

            std::cout << ">>> Epoch " << (epoch + 1) << "/" << epochs
                      << " DONE | Policy: " << avgPolicyLoss
                      << " | Value: " << avgValueLoss
                      << " | Total: " << totalLoss
                      << " | Steps: " << epochSteps << std::endl;

            if (!skippedBatches.empty()) {
                std::cout << "  Skipped " << skippedBatches.size() << " batches:\n";
                for (const auto& s : skippedBatches) {
                    std::cout << "    - " << s << "\n";
                }
            }

            // Save checkpoint after each epoch
            saveCheckpoint("pretrained_epoch_" + std::to_string(epoch + 1));

            // Early stopping
            if (totalLoss < bestLoss) {
                bestLoss = totalLoss;
                patienceCounter = 0;
                saveCheckpoint("pretrained_best");
            } else {
                ++patienceCounter;
                std::cout << "  No improvement (" << patienceCounter << "/" << PATIENCE << ")" << std::endl;
                if (patienceCounter >= PATIENCE) {
                    std::cout << "Early stopping after " << PATIENCE
                              << " epochs without improvement. Best total loss: " << bestLoss << std::endl;
                    loadCheckpoint("pretrained_best");
                    break;
                }
            }
        }

        // Copy final model to bestModel_
        {
            torch::NoGradGuard no_grad;
            auto src_params = model_->parameters();
            auto dst_params = bestModel_->parameters();
            for (size_t i = 0; i < src_params.size(); ++i) {
                dst_params[i].copy_(src_params[i]);
            }
        }

        std::cout << "Pre-training complete." << std::endl;
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
                    state.applyMove(bestMove);

                    // Advance both trees by Move identity (see MCTS::advanceTree)
                    mctsA.advanceTree(bestMove);
                    mctsB.advanceTree(bestMove);
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
        auto path = std::filesystem::path(checkpointDir_) / (name + ".pt");
        torch::save(model_, path.string());
        std::cout << "Saved checkpoint: " << path.string() << std::endl;
    }

    void Trainer::loadCheckpoint(const std::string& name) {
        auto path = std::filesystem::path(checkpointDir_) / (name + ".pt");
        torch::load(model_, path.string());
        std::cout << "Loaded checkpoint: " << path.string() << std::endl;
    }

} // namespace Hive::Learning
