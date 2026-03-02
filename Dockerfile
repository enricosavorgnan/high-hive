FROM nvidia/cuda:12.4.0-devel-ubuntu22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    wget \
    unzip \
    git \
    && rm -rf /var/lib/apt/lists/*

# Download LibTorch (CUDA 12.6, stable)
RUN cd /opt \
    && wget https://download.pytorch.org/libtorch/cu126/libtorch-shared-with-deps-2.10.0%2Bcu126.zip \
    && unzip libtorch-shared-with-deps-2.10.0+cu126.zip \
    && rm libtorch-shared-with-deps-2.10.0+cu126.zip

ENV PATH=/opt/libtorch/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/libtorch/lib:$LD_LIBRARY_PATH
ENV CMAKE_PREFIX_PATH=/opt/libtorch
