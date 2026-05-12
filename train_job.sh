#!/bin/bash
#SBATCH --job-name=hive-selfplay
#SBATCH --output=hive-selfplay-%j.out
#SBATCH --error=hive-selfplay-%j.err
#SBATCH --partition=lovelace
#SBATCH --gres=gpu:1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=32G
#SBATCH --time=24:00:00
#
# Submit with:    sbatch train_job.sh
# Monitor with:   squeue -u $USER
# Check output:   tail -f hive-selfplay-<jobid>.out

# --- Configuration (EDIT THESE) ---
# Path to the cloned repo on the cluster (referenced by other vars below)
REPO_DIR="$HOME/high-hive"

# Apptainer image (CUDA 12.4 + LibTorch 2.6.0 cu124, see hive_pretrain.def)
CONTAINER="$HOME/hive_pretrain.sif"

# Number of self-play iterations to run in this single job.
# Each iteration is ~3-6h on one GPU at default config (256 self-play games,
# 800 MCTS sims, 1000 train steps, 400 eval games), so 4 fits comfortably in
# the 24h SLURM ceiling with margin. To continue across jobs just resubmit:
# the script picks up the newest checkpoint from CHECKPOINT_DIR automatically.
ITERATIONS=4

# Where new checkpoints (latest_iter_N.pt, best_iter_N.pt) are written.
CHECKPOINT_DIR="$HOME/checkpoints/selfplay"

# Supervised pretrained model that ships in the repo. Used the very first
# time, before any self-play checkpoint exists.
PRETRAINED_FALLBACK="$REPO_DIR/cpp/src/alphaZeroEngine/checkpoints/pretrained_best.pt"
# -----------------------------------

# Auto-resume: pick the highest-numbered latest_iter_N.pt in CHECKPOINT_DIR
# if any exist, else fall back to the supervised pretrained model. `ls -v`
# does natural ordering so latest_iter_10.pt beats latest_iter_9.pt.
LATEST_CHECKPOINT=$(ls -1v "$CHECKPOINT_DIR"/latest_iter_*.pt 2>/dev/null | tail -1)
if [ -n "$LATEST_CHECKPOINT" ]; then
    RESUME_FROM="$LATEST_CHECKPOINT"
    echo "[auto-resume] Found prior self-play checkpoint: $RESUME_FROM"
else
    RESUME_FROM="$PRETRAINED_FALLBACK"
    echo "[auto-resume] No prior self-play checkpoint, starting from pretrained: $RESUME_FROM"
fi

echo "Job started: $(date)"
echo "Node: $(hostname)"
echo "GPU: $(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null || echo 'N/A')"
echo "Iterations: $ITERATIONS"
echo "Resuming from: $RESUME_FROM"
echo "Saving to: $CHECKPOINT_DIR"

mkdir -p "$CHECKPOINT_DIR"

module load apptainer

# stdbuf -oL line-buffers hive_train's stdout so that progress lines (CUDA
# detection, "=== Iteration N ===", per-step training losses, etc.) appear
# in the SLURM .out file as they are emitted. Without it stdout is fully
# buffered when redirected to a file, hiding progress for hours.
apptainer exec --nv "$CONTAINER" \
    stdbuf -oL -eL \
    "$REPO_DIR/cpp/build/hive_train" \
    --iterations "$ITERATIONS" \
    --checkpoint-dir "$CHECKPOINT_DIR" \
    --resume "$RESUME_FROM"

echo "Job finished: $(date)"
