#include "training/headers/self_play.h"
#include "nn/headers/state_encoder.h"
#include "nn/headers/action_encoder.h"

#include <iostream>

namespace Hive::Learning {

    SelfPlay::SelfPlay(HiveNet network)
        : network_(std::move(network)) {}

    std::vector<TrainingSample> SelfPlay::playGame() {
        GameState state;
        MCTS mcts(network_);

        // Samples collected during the game (before outcome is known)
        struct PendingSample {
            torch::Tensor stateTensor;
            torch::Tensor policyTensor;
            Color player; // Who was to move at this position
        };

        std::vector<PendingSample> pending;
        pending.reserve(MAX_GAME_LENGTH);

        int moveCount = 0;

        while (!state.isTerminal() && moveCount < MAX_GAME_LENGTH) {
            Color currentPlayer = state.toMove();

            // MCTS search
            auto moveVisits = mcts.search(state, /*addNoise=*/true);

            if (moveVisits.empty()) {
                // No legal moves — pass
                Move passMove;
                passMove.type = Move::Pass;
                state.apply(passMove);
                mcts.reset();
                ++moveCount;
                continue;
            }

            // Build policy distribution from visit counts
            auto policyTensor = torch::zeros({ACTION_SPACE});
            auto policyAcc = policyTensor.accessor<float, 1>();
            int totalVisits = 0;
            std::vector<int> visitCounts;
            visitCounts.reserve(moveVisits.size());

            for (const auto& [move, visits] : moveVisits) {
                totalVisits += visits;
                visitCounts.push_back(visits);
            }

            for (size_t i = 0; i < moveVisits.size(); ++i) {
                int action = ActionEncoder::moveToAction(moveVisits[i].first, state);
                if (action >= 0 && action < ACTION_SPACE) {
                    policyAcc[action] = static_cast<float>(moveVisits[i].second)
                                        / static_cast<float>(totalVisits);
                }
            }

            // Record sample
            PendingSample sample;
            sample.stateTensor = StateEncoder::encode(state);
            sample.policyTensor = policyTensor;
            sample.player = currentPlayer;
            pending.push_back(std::move(sample));

            // Select action based on temperature
            float temperature = (moveCount < TEMP_THRESHOLD) ? TEMP_HIGH : TEMP_LOW;
            int selectedIdx = MCTS::selectAction(visitCounts, temperature);

            // Apply selected move
            const Move& selectedMove = moveVisits[selectedIdx].first;
            state.apply(selectedMove);

            // Advance MCTS tree
            int selectedAction = ActionEncoder::moveToAction(selectedMove, state);
            mcts.advanceTree(selectedAction);

            ++moveCount;
        }

        // Determine game outcome
        float whiteOutcome = 0.0f;
        if (state.isTerminal()) {
            whiteOutcome = state.resultForColor(Color::White);
        }
        // If game hit max length, outcome = 0 (draw)

        // Convert pending samples to training samples with correct outcomes
        std::vector<TrainingSample> samples;
        samples.reserve(pending.size());

        for (auto& p : pending) {
            TrainingSample s;
            s.state = std::move(p.stateTensor);
            s.policy = std::move(p.policyTensor);
            // Value from perspective of the player who was to move
            s.value = (p.player == Color::White) ? whiteOutcome : -whiteOutcome;
            samples.push_back(std::move(s));
        }

        return samples;
    }

    void SelfPlay::playGames(int numGames, ReplayBuffer& buffer) {
        for (int i = 0; i < numGames; ++i) {
            auto samples = playGame();
            buffer.addBatch(samples);

            if ((i + 1) % 10 == 0) {
                std::cout << "Self-play: completed " << (i + 1) << "/" << numGames
                          << " games (" << buffer.size() << " samples in buffer)\n";
            }
        }
    }

} // namespace Hive::Learning
