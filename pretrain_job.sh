#!/bin/bash
#SBATCH --job-name=hive-pretrain
#SBATCH --output=hive-pretrain-%j.out
#SBATCH --error=hive-pretrain-%j.err
#SBATCH --gres=gpu:1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=32G
#SBATCH --time=24:00:00
#
# Submit with: sbatch pretrain_job.sh
# Monitor with: squeue -u $USER
# Check output:  tail -f hive-pretrain-<jobid>.out

# --- Configuration (EDIT THESE) ---
SGF_DIR="$HOME/hive-games"          # Directory with SGF files
EPOCHS=30                            # Training epochs
CHECKPOINT_DIR="$HOME/checkpoints"   # Where to save model
CONTAINER="$HOME/hive_pretrain.sif"  # Path to .sif image
REPO_DIR="$HOME/high-hive"          # Path to cloned repo
# -----------------------------------

echo "Job started: $(date)"
echo "Node: $(hostname)"
echo "GPU: $(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null || echo 'N/A')"

module load apptainer

apptainer exec --nv "$CONTAINER" \
    "$REPO_DIR/cpp/build/hive_pretrain" \
    --sgf-dir "$SGF_DIR" \
    --epochs "$EPOCHS" \
    --checkpoint-dir "$CHECKPOINT_DIR"

echo "Job finished: $(date)"
