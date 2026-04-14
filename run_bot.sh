#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$DIR/cpp/build/uhp --engine AlphaZeroEngine --model-path $DIR/cpp/src/alphaZeroEngine/checkpoints/pretrained_best.pt --time-budget 4500"
