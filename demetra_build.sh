#!/bin/bash
# Build hive_pretrain inside the Apptainer container on Demetra
# Run this ONCE after uploading the repo to the cluster
#
# Usage: bash demetra_build.sh

set -e

CONTAINER="$HOME/hive_pretrain.sif"
REPO_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Building hive_pretrain inside container ==="

module load apptainer

apptainer exec --nv "$CONTAINER" bash -c "
    cd '$REPO_DIR/cpp'
    mkdir -p build && cd build
    cmake .. -DENABLE_LEARNING=ON -DCMAKE_PREFIX_PATH=/opt/libtorch
    make -j\$(nproc) hive_pretrain uhp
"

echo "=== Build complete ==="
echo "Binary at: $REPO_DIR/cpp/build/hive_pretrain"
