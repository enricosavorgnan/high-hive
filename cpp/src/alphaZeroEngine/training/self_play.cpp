#include "alphaZeroEngine/training/headers/self_play.h"
#include "alphaZeroEngine/nn/headers/state_encoder.h"
#include "alphaZeroEngine/nn/headers/action_encoder.h"

#include <iostream>

namespace Hive::Learning {

    SelfPlay::SelfPlay(HiveNet network)
        : network_(std::move(network)) {}

    std::vector<TrainingSample> SelfPlay::playGame() {
        State state;
        MCTS mcts(network_);

        // Samples collected during the game (before outcome is known)
        struct PendingSample {
            torch::Tensor planes;
            torch::Tensor scalars;
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
                state.applyMove(passMove);
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
            auto encoded = StateEncoder::encode(state);
            PendingSample sample;
            sample.planes = std::move(encoded.planes);
            sample.scalars = std::move(encoded.scalars);
            sample.policyTensor = policyTensor;
            sample.player = currentPlayer;
            pending.push_back(std::move(sample));

            // Select action based on temperature
            float temperature = (moveCount < TEMP_THRESHOLD) ? TEMP_HIGH : TEMP_LOW;
            int selectedIdx = MCTS::selectAction(visitCounts, temperature);

            const Move& selectedMove = moveVisits[selectedIdx].first;
            state.applyMove(selectedMove);

            // Advance MCTS tree by Move identity (action encoding is lossy,
            // see MCTS::advanceTree)
            mcts.advanceTree(selectedMove);

            ++moveCount;
        }

        // Determine game outcome. Only a real win/loss (queen surrounded)
        // contributes to the value head; draws and games that hit
        // MAX_GAME_LENGTH without a winner get value_weight=0 so their
        // moves still train the policy head but don't push the value head
        // toward a meaningless 0 label.
        GameResult gr = state.result();
        bool decisive = (gr == GameResult::WhiteWin || gr == GameResult::BlackWin);
        float whiteOutcome = decisive ? state.resultForColor(Color::White) : 0.0f;
        float weight = decisive ? 1.0f : 0.0f;

        // Convert pending samples to training samples with correct outcomes
        std::vector<TrainingSample> samples;
        samples.reserve(pending.size());

        for (auto& p : pending) {
            TrainingSample s;
            s.planes = std::move(p.planes);
            s.scalars = std::move(p.scalars);
            s.policy = std::move(p.policyTensor);
            // Value from perspective of the player who was to move
            s.value = (p.player == Color::White) ? whiteOutcome : -whiteOutcome;
            s.value_weight = weight;
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
