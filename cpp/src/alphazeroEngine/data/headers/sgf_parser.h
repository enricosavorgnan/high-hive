#pragma once

#include "alphazeroEngine/training/headers/replay_buffer.h"

#include <string>
#include <vector>

// SGF PARSER
// Parses SGF game files from boardspace.net and converts them to training samples.
// Invalid games (illegal moves, wrong variants) are discarded.

namespace Hive::Learning {

    struct SgfGameInfo {
        std::string white;      // White player name
        std::string black;      // Black player name
        std::string result;     // Game result string
        std::string gameType;   // Game variant
        std::vector<std::string> moves; // Sequence of moves in SGF notation
    };

    class SgfParser {
    public:
        // Parse a single SGF file and extract game info
        static SgfGameInfo parseFile(const std::string& filepath);

        // Convert SGF move notation to UHP-compatible move string
        static std::string sgfMoveToUhp(const std::string& sgfMove);

        // Process a single game: replay moves, validate, generate training samples
        // Returns empty vector if the game is invalid
        static std::vector<TrainingSample> processGame(const SgfGameInfo& game);

        // Process all SGF files in a directory, saving samples to disk in batches.
        // skipFiles: skip the first N SGF files (for resuming after a crash).
        // Returns total number of samples saved.
        static int processDirectory(const std::string& dirPath, const std::string& outputDir, int skipFiles = 0);

        // Parse the result string to get outcome (+1 white wins, -1 black wins, 0 draw)
        static float parseResult(const std::string& result);

    private:
        // Convert boardspace piece + reference to UHP move string
        static std::string convertToUhp(const std::string& piece, const std::string& ref);
    };

} // namespace Hive::Learning
