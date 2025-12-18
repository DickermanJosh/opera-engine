# Opera Chess Engine - Multi-stage Docker Build
# Dockerfile for building and running the Opera Chess Engine with C++ core and Rust UCI layer

# =============================================================================
# Stage 1: Build C++ Engine Core
# =============================================================================
FROM ubuntu:22.04 as cpp-builder

# Install C++ build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    clang \
    libgtest-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy C++ source files
COPY cpp/ ./cpp/

# Build C++ engine core
RUN cd cpp && \
    mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -G Ninja && \
    ninja && \
    # Verify build artifacts exist
    ls -la . && \
    echo "C++ build completed successfully"

# =============================================================================
# Stage 2: Build Rust UCI Interface
# =============================================================================
FROM rust:1.83-slim as rust-builder

# Install system dependencies needed for Rust compilation
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    clang \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy Rust source and build configuration
COPY rust/ ./rust/
# Copy C++ artifacts from previous stage for FFI compilation
COPY --from=cpp-builder /build/cpp/ ./cpp/

# Build Rust UCI interface
RUN cd rust && \
    # First, check dependencies are available
    cargo --version && \
    rustc --version && \
    # Build release binary
    cargo build --release --features ffi && \
    # Verify binary exists
    ls -la target/release/ && \
    echo "Rust build completed successfully"

# =============================================================================
# Stage 3: Runtime Image (Lightweight)
# =============================================================================
FROM ubuntu:22.04 as runtime

# Install minimal runtime dependencies only
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

# Create app user for security
RUN useradd -m -u 1001 opera && \
    mkdir -p /app /nn /logs && \
    chown -R opera:opera /app /nn /logs

# Set working directory
WORKDIR /app

# Copy built binaries from previous stages
COPY --from=rust-builder /build/rust/target/release/opera-uci ./opera-uci
COPY --from=cpp-builder /build/cpp/build/ ./cpp-engine/

# Copy entrypoint script
COPY docker-entrypoint.sh ./
RUN chmod +x docker-entrypoint.sh

# Set proper ownership
RUN chown -R opera:opera /app

# Switch to non-root user
USER opera

# Create volume mount points
VOLUME ["/nn", "/logs"]

# Expose no ports (UCI engine communicates via stdin/stdout)
# But document that UCI protocol uses standard I/O
LABEL description="Opera Chess Engine - UCI compliant chess engine with Morphy-inspired playing style"
LABEL version="1.0.0"
LABEL maintainer="Opera Engine Team"

# Default command - start in UCI mode
ENTRYPOINT ["./docker-entrypoint.sh"]
CMD ["uci"]