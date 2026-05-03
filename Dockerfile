# Use a stable Ubuntu base image
FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build tools + python3-pip. pip is used below to install torch, which
# bundles libtorch.so and the Torch CMake config for the *host* architecture
# (x86_64 or aarch64). Hardcoding the LibTorch nightly URL fails on Apple
# Silicon because that URL only ships x86_64 binaries.
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    libgfortran5 \
    && rm -rf /var/lib/apt/lists/*

# Install torch CPU wheel. pip auto-selects linux_x86_64 or linux_aarch64
# from the PyTorch CPU index, so this Dockerfile builds on both Intel and
# Apple Silicon hosts.
RUN pip3 install --no-cache-dir \
    --index-url https://download.pytorch.org/whl/cpu \
    torch==2.5.1

# Set the working directory
WORKDIR /app

# Copy the entire repository (including all locally modified files) into the container
COPY . /app

# Configure and build the C++ project. CMAKE_PREFIX_PATH is queried from the
# installed torch package so it always points at the right CMake config.
# CMAKE_EXE_LINKER_FLAGS adds an -rpath-link to torch.libs/, where the wheel
# bundles its hashed-name shared deps (e.g. libgfortran-daac5196.so.5.0.0
# pulled in by libopenblas). Without it the linker can't resolve the indirect
# Fortran symbols at executable link time on arm64.
WORKDIR /app/cpp
RUN rm -rf build && mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_PREFIX_PATH="$(python3 -c 'import torch; print(torch.utils.cmake_prefix_path)')" \
        -DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath-link,/usr/local/lib/python3.10/dist-packages/torch.libs -Wl,-rpath-link,/usr/local/lib/python3.10/dist-packages/torch/lib" \
        -DUSE_CUDA=OFF \
    && make uhp hive_pretrain hive_train

# Set the working directory to the build folder for execution
WORKDIR /app/cpp/build

# Default command to run the UHP engine for the referee
ENTRYPOINT ["./uhp"]
CMD ["--engine", "AlphaZeroEngine", "--model-path", "alphaZeroEngine/checkpoints/pretrained_best.pt", "--time-budget", "4800"]