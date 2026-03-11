#pragma once

#include <string>
#include <vector>
#include <memory>
#include "utils.h"
#include "engine.h"
#include "state.h"

namespace Hive {

    class UhpHandler {
        State state;

        std::string gameType = "Base+MLP";
        std::string gameState = "NotStarted";
        std::vector<std::string> moveHistory;

        std::string generateGameString() const;
        void applyMove(const std::string& moveStr);

        // The polymorphic engine instance, initialized as RandomEngine
        std::unique_ptr<Engine> engine = std::make_unique<RandomEngine>();

    public:
        UhpHandler() = default;
        void loop();

    private:
        static void cmdU1();
        static void cmdInfo();
        void cmdNewGame(const std::vector<std::string>& chunks, const std::string& line);
        void cmdPlay(const std::vector<std::string>& chunks, const std::string& line);
        void cmdPass();
        void cmdValidMoves() const;
        void cmdBestMove(const std::vector<std::string>& chunks) const;
        void cmdUndo();
        static void cmdOptions();
    };

} // namespace Hive