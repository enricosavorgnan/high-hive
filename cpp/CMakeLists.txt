cmake_minimum_required(VERSION 3.20)
project(high_hive LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Core game engine library ---
add_library(hive_core STATIC
  src/board.cpp
  src/moves.cpp
  src/rules.cpp
  src/engine.cpp
  src/utils.cpp
)

target_include_directories(hive_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/headers
)

# --- UHP executable (game engine only, no LibTorch) ---
add_executable(high_hive
  src/main.cpp
  src/uhp.cpp
)

target_link_libraries(high_hive PRIVATE hive_core)

# --- Learning module (optional, requires LibTorch) ---
option(ENABLE_LEARNING "Build learning module with LibTorch" OFF)

if(ENABLE_LEARNING)
  # Find LibTorch
  find_package(Torch REQUIRED)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

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
    "${TORCH_LIBRARIES}"
  )

  # --- Training executable (self-play AlphaZero loop) ---
  add_executable(hive_train
    train_main.cpp
  )
  target_link_libraries(hive_train PRIVATE hive_learning)

  # --- Pre-training executable (supervised from SGF) ---
  add_executable(hive_pretrain
    pretrain_main.cpp
  )
  target_link_libraries(hive_pretrain PRIVATE hive_learning)
endif()
