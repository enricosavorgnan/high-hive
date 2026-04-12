#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$DIR/cpp/build/uhp" --model "$DIR/cpp/learning/checkpoints/pretrained_best.pt"
