cmake_minimum_required(VERSION 3.20)
project(uhp_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Core engine library (no LibTorch dependency) ---
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/headers
)

# --- UHP executable ---
add_executable(uhp src/main.cpp)
target_link_libraries(uhp PRIVATE hive_core)
target_include_directories(uhp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/headers)

# When learning is enabled, uhp can use AlphaZeroEngine via --model flag
# (linking and define are added below in the ENABLE_LEARNING block)

# --- Optional: Learning module (requires LibTorch) ---
option(ENABLE_LEARNING "Build learning module (requires LibTorch)" OFF)

if(ENABLE_LEARNING)
    find_package(Torch REQUIRED)

    add_library(hive_learning STATIC
        learning/nn/neural_net.cpp
        learning/nn/state_encoder.cpp
        learning/nn/action_encoder.cpp
        learning/mcts/mcts.cpp
        learning/training/replay_buffer.cpp
        learning/training/self_play.cpp
        learning/training/trainer.cpp
        learning/data/sgf_parser.cpp
    )

    target_include_directories(hive_learning PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers
        ${CMAKE_CURRENT_SOURCE_DIR}/learning
    )

    target_link_libraries(hive_learning PUBLIC
        hive_core
        ${TORCH_LIBRARIES}
    )

    # Pre-training executable
    add_executable(hive_pretrain pretrain_main.cpp)
    target_link_libraries(hive_pretrain PRIVATE hive_learning)
    target_include_directories(hive_pretrain PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers
        ${CMAKE_CURRENT_SOURCE_DIR}/learning
    )

    # Enable AlphaZeroEngine in the UHP executable
    target_link_libraries(uhp PRIVATE hive_learning)
    target_include_directories(uhp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/learning)
    target_compile_definitions(uhp PRIVATE ENABLE_LEARNING)
endif()
