#!/bin/bash
# Benchmark script for UHP bot response times using a predefined sequence of moves.
# Usage (local):  bash benchmark_bot.sh ./cpp/build/uhp path/to/model.pt
# Usage (Demetra): apptainer exec --nv $CONTAINER bash benchmark_bot.sh ./cpp/build/uhp path/to/model.pt

UHP_BIN="${1:?Usage: $0 <uhp_binary> <model_path>}"
MODEL="${2:?Usage: $0 <uhp_binary> <model_path>}"

echo "=== High-Hive Benchmark ==="
echo "Binary: $UHP_BIN"
echo "Model:  $MODEL"
echo ""

# Move sequence to simulate a mid-game scenario with ~10 pieces on the board.
COMMANDS=$(cat <<'EOF'
u1
newgame
bestmove time 5
play wS1
play bG1 wS1-
bestmove time 5
play wA1 wS1/
play bA1 bG1\
bestmove time 5
play wQ -wS1
play bQ -bG1
bestmove time 5
play wB1 wQ/
play bB1 bQ/
bestmove time 5
play wG1 wA1-
play bS1 bA1-
bestmove time 5
exit
EOF
)

# Execute the commands and capture response times
echo "$COMMANDS" | $UHP_BIN --engine AlphaZeroEngine --model-path "$MODEL" | while IFS= read -r line; do
    if [[ "$line" == bestmove* ]]; then
        echo ""
        echo ">>> Sending: $line"
        START=$(date +%s%N)
        echo "$line"
        # Answer will be read by the process
    else
        echo "$line"
    fi
done | "$UHP_BIN" --model "$MODEL" 2>&1 | while IFS= read -r response; do
    echo "    $response"
done

echo "=== For a precise timing, please use the interactive method ==="
echo "Launch: $UHP_BIN --model $MODEL"
echo "Then, manually input the commands from the benchmark sequence to see exact response times."