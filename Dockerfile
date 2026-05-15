# Use a stable Ubuntu base image
FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    wget \
    unzip \
    && rm -rf /var/lib/apt/lists/*

# Download and extract LibTorch (CPU) to match the Demetra path
RUN wget -qO libtorch.zip https://download.pytorch.org/libtorch/nightly/cpu/libtorch-shared-with-deps-latest.zip \
    && unzip -q libtorch.zip -d /opt \
    && rm libtorch.zip

# Set the working directory
WORKDIR /app

# Copy the entire repository (including all locally modified files) into the container
COPY . /app

# Configure and build the C++ project
WORKDIR /app/cpp
RUN rm -rf build && mkdir -p build && cd build \
    && cmake .. -DCMAKE_PREFIX_PATH=/opt/libtorch -DUSE_CUDA=OFF \
    && make uhp hive_pretrain hive_train

# Set the working directory to the build folder for execution
WORKDIR /app/cpp/build

# Default command to run the UHP engine for the referee
ENTRYPOINT ["./uhp"]
CMD ["--engine", "AlphaZeroEngine", "--model-path", "alphaZeroEngine/checkpoints/pretrained_best.pt", "--time-budget", "4800", "--verbose", "--log-file", "./referee/logs/mcts_1.log"]