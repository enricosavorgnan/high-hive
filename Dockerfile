FROM nvidia/cuda:12.6.3-devel-ubuntu22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    wget \
    unzip \
    git \
    && rm -rf /var/lib/apt/lists/*

# Install CMake 3.31 (needed for CUDA20 dialect support)
RUN cd /tmp \
    && wget -q https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-x86_64.tar.gz \
    && tar xzf cmake-3.31.6-linux-x86_64.tar.gz -C /opt \
    && rm cmake-3.31.6-linux-x86_64.tar.gz
ENV PATH=/opt/cmake-3.31.6-linux-x86_64/bin:$PATH

# Download LibTorch (CUDA 12.6, stable)
RUN cd /opt \
    && wget https://download.pytorch.org/libtorch/cu126/libtorch-shared-with-deps-2.10.0%2Bcu126.zip \
    && unzip libtorch-shared-with-deps-2.10.0+cu126.zip \
    && rm libtorch-shared-with-deps-2.10.0+cu126.zip

ENV PATH=/opt/libtorch/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/libtorch/lib:$LD_LIBRARY_PATH
ENV CMAKE_PREFIX_PATH=/opt/libtorch
