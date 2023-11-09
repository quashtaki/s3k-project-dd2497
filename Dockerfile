FROM ubuntu:22.04

# Install necessary dependencies
RUN apt-get update && \
    apt-get install -y build-essential git wget qemu-system-riscv64 mtools && \
    rm -rf /var/lib/apt/lists/*

# Create a directory for the s3k project
WORKDIR /s3k

# Copy the local s3k directory into the container
COPY ./s3k /s3k

# Copy the local toolchain archive into the container
COPY ./riscv64-unknown-elf-toolchain /s3k/riscv64-unknown-elf-toolchain

# Add the toolchain binaries to the PATH
ENV PATH="/s3k/riscv64-unknown-elf-toolchain/opt/riscv/bin:${PATH}"

WORKDIR /s3k/projects/fs

RUN make
# RUN make qemu

RUN make disk-image-binary
#RUN make disk-read



CMD make qemu