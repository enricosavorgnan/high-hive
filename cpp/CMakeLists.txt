cmake_minimum_required(VERSION 3.20)
project(uhp_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Core engine library ---
add_library(hive_core STATIC
        src/board.cpp
        src/engine.cpp
        src/generator.cpp
        src/moves.cpp
        src/rules.cpp
        src/state.cpp
        src/uhp.cpp
        src/utils.cpp
)

target_include_directories(hive_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers
)

# --- UHP executable ---
add_executable(uhp src/main.cpp)
target_link_libraries(uhp PRIVATE hive_core)
target_include_directories(uhp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/headers)

# --- Optional: Learning module (requires LibTorch) ---
option(ENABLE_LEARNING "Build learning module (requires LibTorch)" OFF)

if(ENABLE_LEARNING)
    find_package(Torch REQUIRED)
    add_definitions(${TORCH_DEFINITIONS})

    # Link Torch to core
    target_link_libraries(hive_core PUBLIC ${TORCH_LIBRARIES})

    # Define macro fro hive_core
    target_compile_definitions(hive_core PUBLIC ENABLE_LEARNING)

    # Learning Module Library
    add_library(hive_learning STATIC
            src/alphaZeroEngine/nn/neural_net.cpp
            src/alphaZeroEngine/nn/state_encoder.cpp
            src/alphaZeroEngine/nn/action_encoder.cpp
            src/alphaZeroEngine/mcts/mcts.cpp
            src/alphaZeroEngine/training/replay_buffer.cpp
            src/alphaZeroEngine/training/self_play.cpp
            src/alphaZeroEngine/training/trainer.cpp
            src/alphaZeroEngine/data/sgf_parser.cpp
    )

    target_include_directories(hive_learning PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/headers
            ${CMAKE_CURRENT_SOURCE_DIR}/src/alphaZeroEngine
    )

    target_link_libraries(hive_learning PUBLIC
            hive_core
            ${TORCH_LIBRARIES}
    )

    # Pre-training executable
    add_executable(hive_pretrain pretrain_main.cpp)
    target_link_libraries(hive_pretrain PRIVATE hive_learning)
    target_include_directories(hive_pretrain PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/headers
            ${CMAKE_CURRENT_SOURCE_DIR}/src/alphaZeroEngine
    )

    # Enable AlphaZeroEngine in the UHP executable
    target_link_libraries(uhp PRIVATE hive_learning)
    target_include_directories(uhp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/alphaZeroEngine)
    target_compile_definitions(uhp PRIVATE ENABLE_LEARNING)

endif()