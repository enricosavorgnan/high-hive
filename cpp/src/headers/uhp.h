#pragma once

#include <string>
#include <vector>
#include <memory>  // Required for std::unique_ptr
#include "board.h"
#include "utils.h"
#include "rules.h"
#include "engine.h" // Include the new engine header

namespace Hive {

    class UhpHandler {
    private:
        Board board;

        std::string gameType = "Base+MLP";
        std::string gameState = "NotStarted";
        int turnNumber = 1;
        Color turnPlayer = Color::White;
        std::vector<std::string> moveHistory;

        std::string generateGameString() const;
        void applyMove(const std::string& moveStr);

        // The polymorphic engine instance, initialized as RandomEngine
        std::unique_ptr<Engine> engine = std::make_unique<RandomEngine>();

        std::vector<Piece> getHand(Color player) const;

    public:
        UhpHandler() = default;
        void loop();

    private:
        void cmdU1();
        void cmdInfo();
        void cmdNewGame(const std::vector<std::string>& chunks, const std::string& line);
        void cmdPlay(const std::vector<std::string>& chunks, const std::string& line);
        void cmdPass();
        void cmdValidMoves();
        void cmdBestMove(const std::vector<std::string>& chunks) const;

        static void cmdUndo();

        static void cmdOptions();
    };

} // namespace Hive