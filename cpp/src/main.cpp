#include <iostream>
#include <string>
#include <sstream>
#include <vector>

// Include necessary headers
#include "headers/board.h"
#include "headers/rules.h"
#include "headers/utils.h"
#include "headers/moves.h"

using namespace Hive;

// Helper to split string by spaces to mimic Python's line.split()
std::vector<std::string> splitCommand(const std::string& line) {
    std::vector<std::string> chunks;
    std::istringstream stream(line);
    std::string chunk;
    while (stream >> chunk) {
        chunks.push_back(chunk);
    }
    return chunks;
}

int main() {
    // Disable synchronization between C and C++ standard streams for performance
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    Board board;
    // Note: You will eventually need a 'State' or 'GameManager' class here 
    // to track player turn, hand counts, and history (like HiveBoard in Python).

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::vector<std::string> chunks = splitCommand(line);
        if (chunks.empty()) continue;

        const std::string& cmd = chunks[0];

        if (cmd == "u1") {
            std::cout << "ok\n";
        } 
        else if (cmd == "info") {
            std::cout << "id high-hive-engine v0.1\n";
            std::cout << "Mosquito;Ladybug;Pillbug;\n";
            std::cout << "ok\n";
        } 
        else if (cmd == "newgame") {
            board = Board(); // Reset board state
            
            // Extract gamestring if provided
            std::string gameString = "";
            if (chunks.size() > 1) {
                gameString = line.substr(line.find(chunks[1]));
            }

            // TODO: Implement parsing logic for the gameString to populate the board
            // e.g., State::parseGameString(gameString, board);
            
            std::cout << "Base+MLP;NotStarted;White[1]\n"; 
            std::cout << "ok\n";
        } 
        else if (cmd == "play") {
            std::string moveStr = "";
            if (chunks.size() > 1) {
                moveStr = line.substr(line.find(chunks[1]));
            }
            
            // TODO: Convert UHP moveStr to Hive::Move and apply to board
            
            std::cout << moveStr << "\n";
            std::cout << "ok\n";
        } 
        else if (cmd == "pass") {
            // TODO: Apply pass logic (switch turns, update history)
            
            std::cout << "pass\n";
            std::cout << "ok\n";
        } 
        else if (cmd == "validmoves") {
            // TODO: Get turnPlayer and hand from a GameState object
            // std::vector<Move> validMoves = RuleEngine::generateMoves(board, turnPlayer, hand);
            
            // Example of formatting output:
            // if (validMoves.empty()) {
            //     std::cout << "pass\n";
            // } else {
            //     for (size_t i = 0; i < validMoves.size(); ++i) {
            //         std::cout << MoveToString(validMoves[i], board) << (i == validMoves.size()-1 ? "" : ";");
            //     }
            //     std::cout << "\n";
            // }

            std::cout << "pass\n"; // Placeholder until State management is complete
        } 
        else if (cmd == "bestmove") {
            if (chunks.size() >= 3 && chunks[1] == "time") {
                // int timeLimit = std::stoi(chunks[2]);
                // TODO: Call AI Engine (e.g., RandomMoveEngine::getBestMove)
            }
            std::cout << "pass\n"; // Placeholder
        } 
        else if (cmd == "undo") {
            // TODO: Pop from State history and un-apply move
            std::cout << "ok\n";
        } 
        else if (cmd == "options") {
            // Standard MZinga options processing (ignore for now)
        } 
        else if (cmd == "exit") {
            break;
        }

        // Flush output after each command to ensure Mzinga receives it immediately
        std::cout << std::flush;
    }

    return 0;
}