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

# --- Find and configure LibTorch (Unconditional) ---
find_package(Torch REQUIRED)
add_definitions(${TORCH_DEFINITIONS})
target_link_libraries(hive_core PUBLIC ${TORCH_LIBRARIES})

# --- UHP executable ---
add_executable(uhp src/main.cpp)
target_link_libraries(uhp PRIVATE hive_core)
target_include_directories(uhp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/headers)

# --- Learning Module Library (Unconditional) ---
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

# --- Link learning module to UHP executable ---
target_link_libraries(uhp PRIVATE hive_learning)
target_include_directories(uhp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/alphaZeroEngine)

# --- Global Asset Management: Neural Network Checkpoints ---
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/alphaZeroEngine/checkpoints
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/alphaZeroEngine)




# --- Training executables ---
add_executable(hive_train train_main.cpp)
target_link_libraries(hive_train PRIVATE hive_learning)
target_include_directories(hive_train PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers
        ${CMAKE_CURRENT_SOURCE_DIR}/src/alphaZeroEngine
)

add_executable(hive_pretrain pretrain_main.cpp)
target_link_libraries(hive_pretrain PRIVATE hive_learning)
target_include_directories(hive_pretrain PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers
        ${CMAKE_CURRENT_SOURCE_DIR}/src/alphaZeroEngine
)
