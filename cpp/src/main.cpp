#include <iostream>
#include <string>
#include <memory>
#include "headers/uhp.h"

#include "alphaZeroEngine/alphaZeroEngine.h"

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string modelPath = "";
    std::string engineName = "RandomEngine";
    int timeBudget = 4500;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--engine" && i+1 < argc) {
            engineName = argv[++i];
        } else if (arg == "--model-path" && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (arg == "--time-budget" && i + 1 < argc) {
            timeBudget = std::stoi(argv[++i]);
        }
    }

    std::unique_ptr<Hive::Engine> engine;

    if (engineName == "RandomEngine") {
        engine = std::make_unique<Hive::RandomEngine>();
    } else if (engineName == "AlphaZeroEngine") {
        if (modelPath.empty()) {
            std::cerr << "ERROR: modelPath is empty" << std::endl;
            return 1;
        }
        engine = std::make_unique<Hive::AlphaZeroEngine>(modelPath, timeBudget);
    } else {
        std::cerr << "ERROR: unknown engine" << std::endl;
        return 2;
    }

    Hive::UhpHandler uhp(std::move(engine));

    uhp.cmdInfo();
    std::cout << std::flush;

    uhp.loop();

    return 0;
}