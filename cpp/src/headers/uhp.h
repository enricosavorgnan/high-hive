#pragma once

#include <string>
#include <vector>
#include <memory>
#include "utils.h"
#include "engine.h"
#include "state.h"

namespace Hive {

    class UhpBoard {
        private:
            std::unordered_map<Coord, std::vector<std::string, CoordHash>> stacks {};
            std::unordered_map<std::string, Coord> piecePos;

            static int maxIndexForBug(Bug bug);
            static std::string basePieceString(Color color, Bug bug);

        public:
            void clear();

            [[nodiscard]] bool occupied(const Coord &coord) const;
            [[nodiscard]] std::optional<std::string> topName(const Coord& coord) const;
            [[nodiscard]] bool hasPiece(const std::string& pieceName) const;
            [[nodiscard]] std::optional<Coord> whereIs(const std::string& pieceName) const;

            void push(const Coord& coord, const std::string& pieceName);
            std::optional<std::string> pop(const Coord& coord);
            bool moveTop(const Coord& from, const Coord& to);

            [[nodiscard]] std::string nextPieceName(Color color, Bug bug) const;
    };

    class UhpHandler {
        State state;
        UhpBoard uhpBoard;

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