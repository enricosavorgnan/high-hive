#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"

# exec need to follow the structure:
# exec <path_to_executable> --engine <engine_name> --model-path <path_to_model> --time-budget <time_in_ms>
# --model-path is required for --engine AlphaZeroEngine
# --time-budget is required for --engine AlphaZeroEngine, and measures milliseconds
exec "$DIR/cpp/build/uhp" --engine AlphaZeroEngine --model-path "$DIR/cpp/src/alphaZeroEngine/checkpoints/pretrained_best.pt" --time-budget 4500
