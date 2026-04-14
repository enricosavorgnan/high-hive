#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "utils.h"
#include "engine.h"
#include "state.h"

// ----------------- !!!!!! -----------------
// Uncomment the following for using Random Engine as active engine for the bot
//
// #define USE_RANDOM_ENGINE
//
// ----------------- !!!!!! -----------------

namespace Hive {

    class UhpHandler {
        State state;
        UhpBoard uhpBoard;

        std::string gameType = "Base+MLP";
        std::string gameState = "NotStarted";
        std::vector<std::string> moveHistory;

        std::string generateGameString() const;
        int applyMove(const std::string& moveStr, bool validate);

        // Polymorphic engine instance
        #ifdef USE_RANDOM_ENGINE
            std::unique_ptr<Engine> engine = std::make_unique<RandomEngine>();
        #else
            std::string modelPath = "alphaZeroEngine/checkpoints/pretrained_best.pt";
            int timeBudget = 4800;          // In milliseconds (ms)
            std::unique_ptr<Engine> engine = std::make_unique<AlphaZeroEngine>(modelPath, timeBudget);
        #endif

        public:
            UhpHandler() = default;
            explicit UhpHandler(std::unique_ptr<Engine> eng) : engine(std::move(eng)) {}
            void loop();
            static void cmdU1();
            void cmdInfo() const;
            static void cmdOptions();

        private:
            void cmdNewGame(const std::vector<std::string>& chunks, const std::string& line);
            void cmdPlay(const std::vector<std::string>& chunks, const std::string& line);
            void cmdPass();
            void cmdValidMoves() const;
            void cmdBestMove(const std::vector<std::string>& chunks) const;
            void cmdUndo();
        };

} // namespace Hive