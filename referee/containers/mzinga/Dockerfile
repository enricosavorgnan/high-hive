# Use the official .NET image as the base image
FROM mcr.microsoft.com/dotnet/sdk:7.0 AS build-env

# Set the working directory
WORKDIR /app

# Copy the Mzinga source code into the container
# (Assumes the context includes the Mzinga source code)
# COPY . ./
ARG TARGETARCH
RUN apt-get update && apt-get install -y wget unzip \
    && case "${TARGETARCH}" in \
         amd64) MZINGA_ARCH="LinuxX64" ;; \
         arm64) MZINGA_ARCH="LinuxARM64" ;; \
         *)     echo "Unsupported TARGETARCH=${TARGETARCH}" && exit 1 ;; \
       esac \
    && wget "https://github.com/jonthysell/Mzinga/releases/download/v0.15.1/Mzinga.${MZINGA_ARCH}.tar.gz" -O Mzinga_Linux.tar.gz \
    && tar xvf Mzinga_Linux.tar.gz -C /app --strip-components=1 \
    && rm Mzinga_Linux.tar.gz

# RUN cd Mzinga.Linux

# Specify the entry point for the container
CMD ["/app/MzingaEngine"]
