#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$DIR/cpp/build/uhp" --model "$DIR/cpp/src/alphazeroEngine/checkpoints/pretrained_best.pt"
