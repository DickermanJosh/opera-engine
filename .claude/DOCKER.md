## Runtime validation — 2026-09-18

Docker is now running locally. Linux ARM64 image build and UCI smoke execution pass. The separate `uci-validation` target runs the 21-test process suite and an independent Stockfish legality check across 176 positions against the runtime binary; both pass. Python/Stockfish stay out of the final runtime stage. Native Windows and Linux x86-64 CI remain unverified. These checks do not run the full repository unit suite or validate neural weights. See [current evidence](../docs/current-baseline.md).

## Current implementation — 2026-09-13

The Rust UCI executable is now connected to real C++ search. See [current baseline](../docs/current-baseline.md) and [usage/integration guide](../docs/rust_uci_usage.md) for supported commands, observed tests and remaining limitations. This supersedes the September 7 placeholder/build-blocker status below. Full test targets now compile; the whole repository suite still has failures. No Unity/network or strength/NN expansion is included. Earlier dated claims remain historical.

# Docker Setup

## Current audit — 2026-09-07

See [verified baseline and next milestones](../docs/current-baseline.md). Component implementation and historical test reports do not establish executable UCI compliance, current full-suite success, performance acceptance, or production readiness. Docker validation is blocked locally by an unavailable daemon. The current Dockerfile disables C++ tests and runs no Cargo tests; entrypoint smoke checks do not prove command handling. Image size, platform support, and NN configuration are unverified acceptance targets.

The material below is historical implementation reporting or design intent; its original dates are preserved and its completion claims are not fresh verification.

The Opera engine build system is non-trivial (C++ core, Rust UCI layer, optional Python AI wrapper).
We need a Dockerized environment that allows developers and users to build and run the engine without manually setting up dependencies.

The container should support:

Building and running the C++ core and Rust UCI system.
Optional: Python environment for training/export (may not be required at runtime if NN weights are already exported).
Simplified entrypoint: docker run opera-engine <uci|test> should work out of the box.
Scope
In Scope
Dockerfile:
Multi-stage build for performance:
Stage 1: Build C++ engine (with cmake/make or equivalent).
Stage 2: Build Rust UCI interface (cargo build --release).
Stage 3 (optional): Install Python runtime only if NN training/inference is expected inside container.
Final image should be lightweight (copy only necessary binaries + NN weights).
Entrypoint script:
Default: run engine in UCI mode.
Allow override: run tests or debug build.
Volume mounts:
Allow NN weights (.bin) to be mounted into container.
Documentation:
Update README.md with usage instructions.
Out of Scope
Python training pipelines (assume NN already exported).
Multi-container orchestration (Kubernetes, Docker Compose) — tracked separately.
Deliverables
Dockerfile (multi-stage, optimized for size and reproducibility).
docker-entrypoint.sh for simple engine/test invocation.
Update README.md with:
Build instructions: docker build -t opera-engine .
Run instructions:
docker run -it opera-engine → starts engine in UCI mode.
docker run -it opera-engine ./tests/opera_tests → run tests.
Volume mount usage for NN weights:
docker run -v $(pwd)/nn:/nn opera-engine -weights /nn/morphy.nnue.
Acceptance Criteria

docker build completes successfully on Linux and macOS (x86_64 and arm64).

Final runtime image size < 300MB (no unnecessary build tools included).

Container runs engine in UCI mode by default.

Tests can be run inside container (./tests/opera_tests).

NN weights can be mounted and passed to engine.

Documentation in README.md updated with clear instructions.
