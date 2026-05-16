#pragma once

#include "utils.h"
#include "state.h"
#include "engine.h"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>


namespace Hive {

    class UhpHandler {
        State state;
        UhpBoard uhpBoard;

        std::string gameType = "Base+MLP";
        std::string gameState = "NotStarted";
        std::vector<std::string> moveHistory;

        mutable std::ofstream logStream;

        std::string generateGameString() const;
        int applyMove(const std::string& moveStr, bool validate);

        std::unique_ptr<Engine> engine;

        public:
            UhpHandler() = default;
            explicit UhpHandler(std::unique_ptr<Engine> eng, const std::string& logPath = "") : engine(std::move(eng)) {
                if (!logPath.empty()) {
                    logStream.open(logPath, std::ios_base::app);
                    if (!logStream.is_open()) {
                        std::cerr << "err Failed to open log file: " << logPath << "\n";
                    }
                }
            }
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
            void cmdPerft(const std::vector<std::string>& chunks);
            void cmdUndo();
        };

} // namespace Hive