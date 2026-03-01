#pragma once

#include "nn/headers/neural_net.h"
#include "mcts/headers/mcts.h"
#include "training/headers/replay_buffer.h"
#include "state.h"
#include "config/headers/config.h"

#include <vector>

// SELF-PLAY
// Generates complete games using MCTS + neural network evaluation.
// Each position generates a training sample: (state, MCTS policy, game outcome).

namespace Hive::Learning {

    class SelfPlay {
    public:
        explicit SelfPlay(HiveNet network);

        // Play a single complete game and return training samples
        std::vector<TrainingSample> playGame();

        // Play multiple games and add results to replay buffer
        void playGames(int numGames, ReplayBuffer& buffer);

    private:
        HiveNet network_;
    };

} // namespace Hive::Learning
