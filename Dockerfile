# Stage 1: Build Environment
FROM nvidia/cuda:12.2.2-devel-ubuntu22.04 AS builder

# Prevent interactive prompts during apt installations
ENV DEBIAN_FRONTEND=noninteractive

# Install C++ build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build_env

# Copy the repository contents into the container
COPY . .

# Build the C++ engine using the root CMakeLists.txt
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Stage 2: Runtime Environment
FROM nvidia/cuda:12.2.2-runtime-ubuntu22.04

WORKDIR /app

# Copy only the compiled 'high_hive' binary from the builder stage
COPY --from=builder /build_env/build/high_hive /app/high_hive

# Ensure execution permissions
RUN chmod +x /app/high_hive

# Execute the engine
CMD ["/app/high_hive"]