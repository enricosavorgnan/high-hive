#pragma once

#include <string>
#include <vector>
#include <memory>
#include "utils.h"
#include "state.h"
#include "engine.h"


namespace Hive {

    class UhpHandler {
        State state;
        UhpBoard uhpBoard;

        std::string gameType = "Base+MLP";
        std::string gameState = "NotStarted";
        std::vector<std::string> moveHistory;

        std::string generateGameString() const;
        int applyMove(const std::string& moveStr, bool validate);

        std::unique_ptr<Engine> engine;

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