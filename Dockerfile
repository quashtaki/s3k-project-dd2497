FROM ubuntu:22.04

# Install necessary dependencies
RUN apt-get update && \
    apt-get install -y build-essential git wget qemu-system-riscv64 && \
    rm -rf /var/lib/apt/lists/*

# Create a directory for the s3k project
WORKDIR /s3k

# Copy the local s3k directory into the container
COPY ./s3k /s3k

# Copy the local toolchain archive into the container
COPY ./riscv64-unknown-elf-toolchain.tgz /s3k

# Extract the local toolchain
RUN tar -xzvf riscv64-unknown-elf-toolchain.tgz -C /s3k

RUN ls /s3k

# Add the toolchain binaries to the PATH
ENV PATH="/s3k/opt/riscv/bin:${PATH}"

# Print the contents of the /s3k directory
RUN ls -R /s3k

RUN echo $PATH

# Build Hello World
WORKDIR /s3k/projects/hello-world
RUN make all

# Build the kernel and applications
WORKDIR /s3k
RUN make all

WORKDIR /s3k/projects/hello-world
CMD ["./run.sh"]
