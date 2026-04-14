#include <iostream>
#include <string>
#include <memory>
#include "headers/uhp.h"

// #include "alphazero_engine.h"

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // std::string modelPath;
    // for (int i = 1; i < argc; ++i) {
    //     std::string arg = argv[i];
    //     if (arg == "--model" && i + 1 < argc) {
    //         modelPath = argv[++i];
    //     }
    // }

    // std::unique_ptr<Hive::Engine> engine;
    //
    // if (!modelPath.empty()) {
// #ifdef ENABLE_LEARNING
//         std::cerr << "Loading model: " << modelPath << std::endl;
//         engine = std::make_unique<Hive::AlphaZeroEngine>(modelPath);
//         std::cerr << "Model loaded." << std::endl;
// #else
//         std::cerr << "Error: --model requires building with -DENABLE_LEARNING=ON\n";
//         return 1;
// #endif
//     } else {
//         engine = std::make_unique<Hive::RandomEngine>();
//     }

    // Hive::UhpHandler uhp(std::move(engine));

    Hive::UhpHandler uhp;
    uhp.cmdInfo();
    std::cout << std::flush;

    uhp.loop();

    return 0;
}