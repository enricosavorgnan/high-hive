#!/bin/bash
#SBATCH --job-name=hive-selfplay
#SBATCH --output=hive-selfplay-%j.out
#SBATCH --error=hive-selfplay-%j.err
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
# the 24h SLURM ceiling with margin. Resubmit with RESUME_FROM updated to
# continue training across jobs.
ITERATIONS=4

# Where new checkpoints (latest_iter_N.pt, best_iter_N.pt) are written.
CHECKPOINT_DIR="$HOME/checkpoints/selfplay"

# Starting checkpoint (full path). For the first self-play job this points at
# the supervised pretrained model that ships in the repo. For subsequent jobs
# point it at the latest_iter_N.pt produced by the previous job (the trainer
# always writes one of these per iteration, regardless of promotion).
RESUME_FROM="$REPO_DIR/cpp/src/alphaZeroEngine/checkpoints/pretrained_best.pt"
# -----------------------------------

echo "Job started: $(date)"
echo "Node: $(hostname)"
echo "GPU: $(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null || echo 'N/A')"
echo "Iterations: $ITERATIONS"
echo "Resuming from: $RESUME_FROM"
echo "Saving to: $CHECKPOINT_DIR"

mkdir -p "$CHECKPOINT_DIR"

module load apptainer

apptainer exec --nv "$CONTAINER" \
    "$REPO_DIR/cpp/build/hive_train" \
    --iterations "$ITERATIONS" \
    --checkpoint-dir "$CHECKPOINT_DIR" \
    --resume "$RESUME_FROM"

echo "Job finished: $(date)"
