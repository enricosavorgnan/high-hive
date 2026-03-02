FROM nvidia/cuda:12.4.0-devel-ubuntu22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    wget \
    unzip \
    git \
    && rm -rf /var/lib/apt/lists/*

# Download LibTorch (CUDA 12.4, cxx11 ABI)
RUN cd /opt \
    && wget -q https://download.pytorch.org/libtorch/cu124/libtorch-cxx11-abi-shared-with-deps-2.6.0%2Bcu124-linux-x86_64.zip \
    && unzip libtorch-cxx11-abi-shared-with-deps-2.6.0+cu124-linux-x86_64.zip \
    && rm libtorch-cxx11-abi-shared-with-deps-2.6.0+cu124-linux-x86_64.zip

ENV PATH=/opt/libtorch/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/libtorch/lib:$LD_LIBRARY_PATH
ENV CMAKE_PREFIX_PATH=/opt/libtorch
