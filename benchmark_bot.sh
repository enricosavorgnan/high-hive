#!/bin/bash
# Benchmark: misura il tempo di risposta del bot per diverse posizioni
# Usage (locale):  bash benchmark_bot.sh ./cpp/build/uhp path/to/model.pt
# Usage (Demetra): apptainer exec --nv $CONTAINER bash benchmark_bot.sh ./cpp/build/uhp path/to/model.pt

UHP_BIN="${1:?Usage: $0 <uhp_binary> <model_path>}"
MODEL="${2:?Usage: $0 <uhp_binary> <model_path>}"

echo "=== High-Hive Benchmark ==="
echo "Binary: $UHP_BIN"
echo "Model:  $MODEL"
echo ""

# Sequenza di mosse per simulare una partita in corso (mid-game con ~10 pezzi)
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

# Esegui e cattura tempi
echo "$COMMANDS" | while IFS= read -r line; do
    if [[ "$line" == bestmove* ]]; then
        echo ""
        echo ">>> Sending: $line"
        START=$(date +%s%N)
        echo "$line"
        # La risposta verrà letta dal processo
    else
        echo "$line"
    fi
done | "$UHP_BIN" --model "$MODEL" 2>&1 | while IFS= read -r response; do
    echo "    $response"
done

echo ""
echo "=== Per timing preciso, usa il metodo interattivo ==="
echo "Lancia: $UHP_BIN --model $MODEL"
echo "Poi invia manualmente i comandi e misura il tempo di bestmove"
