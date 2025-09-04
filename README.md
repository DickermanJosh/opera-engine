# Opera Chess Engine

A UCI compliant chess engine designed to emulate Paul Morphy's sacrifical, fast paced playing style. **Currently in active development**.

## Quick Start with Docker

The easiest way to build and run the Opera Engine is using Docker, which handles all dependencies automatically.

### Build the Engine

```bash
docker build -t opera-engine .
```

### Run the Engine

**Start in UCI mode (default):**

```bash
docker run -it opera-engine
```

**Run tests:**

```bash
docker run opera-engine test
```

**Run with custom neural network weights:**

```bash
docker run -v $(pwd)/nn:/nn opera-engine -weights /nn/morphy.nnue
```

**Debug mode with custom settings:**

```bash
docker run -it opera-engine debug -hash 256 -threads 2 -morphy
```

### Docker Configuration

The Docker setup uses a multi-stage build for optimal performance:

- **Stage 1**: Builds C++ engine core with CMake/Ninja
- **Stage 2**: Builds Rust UCI interface with Cargo
- **Stage 3**: Creates lightweight runtime image (<300MB)

**Volume Mounts:**

- `/nn` - Neural network weights directory
- `/logs` - Log file output directory

**Supported Platforms:**

- Linux x86_64/arm64
- macOS x86_64/arm64 (via Docker Desktop)

## Manual Development Setup

For local development without Docker, you'll need to set up the multi-language build system manually.
