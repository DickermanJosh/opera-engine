# UCI Protocol Requirements

## 1. Introduction

This document specifies the requirements for implementing the Universal Chess Interface (UCI) protocol in the Opera Engine. The UCI protocol is the standard communication protocol used by chess engines to interact with chess GUI applications, tournament management systems, and analysis tools.

**Architecture Overview**: The UCI implementation will be a Rust-based coordination layer that serves as an intelligent bridge between the C++ engine core, Python AI wrapper, and external chess applications. It will handle async protocol parsing, command routing, FFI coordination, and robust error handling while maintaining the engine's Paul Morphy-inspired playing style.

## 2. User Stories

### Chess GUI Developers
- **As a chess GUI developer**, I want to communicate with the Opera Engine using standard UCI commands, so that I can integrate the engine without custom protocol handling
- **As a chess GUI developer**, I want to configure engine parameters through UCI options, so that users can customize the engine's behavior
- **As a chess GUI developer**, I want real-time search information during analysis, so that I can display progress and principal variations to users

### Tournament Organizers
- **As a tournament organizer**, I want to use the Opera Engine in automated tournaments, so that I can include Morphy-style play in competitions
- **As a tournament organizer**, I want reliable time management and move execution, so that games complete without technical issues
- **As a tournament organizer**, I want configurable playing strength, so that I can balance tournaments with different skill levels

### Chess Analysts
- **As a chess analyst**, I want to use the Opera Engine for position analysis, so that I can explore Morphy-style tactical possibilities
- **As a chess analyst**, I want infinite analysis mode, so that I can deeply examine complex positions
- **As a chess analyst**, I want to set specific search parameters, so that I can control analysis depth and time

### Engine Users
- **As an engine user**, I want consistent UCI behavior across different platforms, so that my tools work reliably
- **As an engine user**, I want fast engine startup and response times, so that I can analyze positions efficiently
- **As an engine user**, I want clear error messages for invalid commands, so that I can debug integration issues

## 3. Acceptance Criteria

### Protocol Compliance Requirements
- **WHEN** engine receives "uci" command, **THEN** the system **SHALL** respond with identification and option list followed by "uciok"
- **WHEN** engine receives "isready" command, **THEN** the system **SHALL** respond with "readyok" when initialization is complete
- **WHEN** engine receives "ucinewgame" command, **THEN** the system **SHALL** reset internal state and prepare for new game
- **WHEN** engine receives "quit" command, **THEN** the system **SHALL** terminate cleanly within 100ms

### Position Management Requirements
- **WHEN** engine receives "position startpos" command, **THEN** the system **SHALL** set board to standard starting position
- **WHEN** engine receives "position fen [FEN]" command, **THEN** the system **SHALL** validate and set board to specified FEN position
- **WHEN** engine receives "position [pos] moves [movelist]" command, **THEN** the system **SHALL** apply moves sequentially and validate legality
- **IF** position command contains invalid FEN or illegal moves, **THEN** the system **SHALL** report error and maintain current position

### Search and Analysis Requirements
- **WHEN** engine receives "go" command, **THEN** the system **SHALL** begin search with specified parameters
- **WHEN** engine receives "go infinite" command, **THEN** the system **SHALL** search until "stop" command received
- **WHEN** engine receives "stop" command, **THEN** the system **SHALL** halt search within 50ms and return best move
- **WHEN** search completes, **THEN** the system **SHALL** output "bestmove [move]" with optional ponder move

### Morphy-Style Configuration Requirements
- **WHEN** engine receives "setoption name MorphyStyle value true", **THEN** the system **SHALL** enable Morphy-inspired evaluation
- **WHEN** engine receives "setoption name SacrificeThreshold value [num]", **THEN** the system **SHALL** adjust material sacrifice willingness
- **WHEN** engine receives "setoption name TacticalDepth value [num]", **THEN** the system **SHALL** set extra depth for tactical sequences
- **IF** option value is invalid or out of range, **THEN** the system **SHALL** maintain current setting and optionally log error

### User Experience Requirements
- **WHEN** search is active, **THEN** the system **SHALL** output "info" strings with depth, score, time, and principal variation
- **WHEN** engine encounters internal error, **THEN** the system **SHALL** output descriptive error message and continue operation
- **WHEN** engine starts, **THEN** the system **SHALL** complete initialization within 2 seconds
- **IF** command parsing fails, **THEN** the system **SHALL** ignore invalid command and continue processing

### Performance Requirements
- **WHEN** processing UCI commands, **THEN** the system **SHALL** respond within 10ms for non-search commands
- **WHEN** searching positions, **THEN** the system **SHALL** achieve minimum 1M nodes/second on modern hardware
- **WHEN** managing time controls, **THEN** the system **SHALL** respect time limits with �50ms accuracy
- **WHEN** handling concurrent commands, **THEN** the system **SHALL** maintain thread safety and avoid race conditions

### Security Requirements
- **WHEN** receiving external input, **THEN** the system **SHALL** validate all command parameters to prevent buffer overflows
- **WHEN** parsing FEN strings, **THEN** the system **SHALL** sanitize input to prevent injection attacks
- **IF** malformed commands are received, **THEN** the system **SHALL** reject input without system compromise

## 4. Technical Architecture

### Communication Architecture
- **Language**: Rust with async/await using Tokio for concurrency
- **Protocol**: Standard input/output text-based communication with efficient parsing
- **Threading Model**: Single-threaded event loop with async command processing
- **FFI Integration**: Safe Rust-to-C++ bindings using cxx crate
- **Error Handling**: Never-panic design with comprehensive error recovery

### Integration Points
- **C++ Core**: FFI bindings to Board, MoveGenerator, and Search classes via cxx
- **Python AI**: Optional coordination with Morphy AI training pipeline
- **Configuration**: YAML-based configuration with type-safe validation
- **Logging**: Structured logging using tracing crate with configurable outputs

### Key Libraries & Dependencies
- **Async Runtime**: tokio for async I/O and task management
- **FFI Bindings**: cxx crate for safe C++ interoperability  
- **Parsing**: Custom hand-rolled tokenizer for performance
- **Serialization**: serde for configuration and structured data
- **Logging**: tracing and tracing-subscriber for structured logging
- **Time Management**: tokio::time for precise timing control

## 5. Feature Specifications

### Core UCI Commands
1. **Engine Identification**: "uci" command with name, author, and options
2. **Readiness Check**: "isready"/"readyok" synchronization
3. **Position Setup**: "position" with FEN and move list support
4. **Search Control**: "go" with time controls, depth limits, and infinite mode
5. **Termination**: "quit" command with clean shutdown

### Advanced UCI Features
1. **Search Information**: Real-time "info" output during search
2. **Option Management**: "setoption" for engine configuration
3. **New Game Reset**: "ucinewgame" state management
4. **Pondering Support**: "go ponder" for opponent's time thinking

### Rust-Specific Features  
1. **Zero-Panic Operation**: All input validation with never-panic error handling
2. **Async Command Processing**: Non-blocking I/O with tokio for responsive UCI communication
3. **Type-Safe FFI**: Safe C++ integration using cxx crate with compile-time checks
4. **Memory Safety**: Rust ownership system prevents memory leaks and buffer overflows

### Morphy-Specific Features
1. **Style Toggle**: MorphyStyle option for enabling/disabling characteristic play
2. **Sacrifice Control**: SacrificeThreshold for material exchange preferences
3. **Tactical Enhancement**: TacticalDepth for extended tactical search
4. **AI Coordination**: Optional Python AI integration for enhanced position evaluation

## 6. Success Criteria

### Functional Success
- **WHEN** tested with Arena GUI, **THEN** engine **SHALL** play games without protocol errors
- **WHEN** tested with Fritz interface, **THEN** engine **SHALL** handle all standard commands correctly
- **WHEN** running tournament games, **THEN** engine **SHALL** complete games within time controls

### Performance Success
- **WHEN** benchmarked against other UCI engines, **THEN** protocol overhead **SHALL** be less than 1% of total computation time
- **WHEN** processing command sequences, **THEN** response time **SHALL** meet real-time requirements
- **WHEN** handling rapid time controls, **THEN** engine **SHALL** make moves with consistent timing

### Style Recognition Success
- **WHEN** MorphyStyle is enabled, **THEN** engine **SHALL** demonstrate sacrificial and tactical play patterns
- **WHEN** playing against standard engines, **THEN** games **SHALL** show aesthetic and forcing characteristics
- **WHEN** analyzing historical Morphy positions, **THEN** engine **SHALL** find key moves within reasonable depth

## 7. Assumptions and Dependencies

### Technical Assumptions
- Rust stable toolchain (1.70+) with async/await and FFI capabilities
- Console/terminal environment with standard input/output streams
- Existing C++ Opera Engine core components are functional and tested
- Target platforms support Rust compilation and C++ interoperability

### External Dependencies
- Chess GUI applications that implement UCI protocol correctly
- cxx crate for safe C++ FFI bindings with build integration
- Existing C++ build system (CMake) can link with Rust static library
- Optional Python runtime for AI coordination (not embedded in UCI binary)

### Resource Assumptions
- Async single-threaded UCI coordination with C++ multi-threaded search
- Memory usage primarily determined by existing C++ engine components
- CPU usage dominated by C++ search algorithms with minimal Rust overhead

## 8. Constraints and Limitations

### Technical Constraints
- Text-based protocol limits bandwidth for complex information
- FFI boundary overhead between Rust and C++ must remain minimal
- Standard UCI doesn't support Morphy-specific information display
- Rust async model requires careful integration with C++ blocking operations

### Performance Constraints
- Protocol parsing must not impact C++ search performance
- FFI call overhead must remain under 1% of total computation time
- Command processing latency must support blitz time controls
- Memory allocations in hot paths should be minimized

### Compatibility Constraints
- Must maintain UCI standard compliance for broad tool compatibility
- Custom options must not break standard UCI implementations
- Platform differences in stdin/stdout and FFI handling must be accommodated
- Rust panic safety must never compromise engine stability

## 9. Risk Assessment

### Technical Risks
- **Risk**: FFI integration complexity between Rust and C++ components
  - **Likelihood**: Medium
  - **Impact**: High
  - **Mitigation**: Use proven cxx crate, comprehensive testing, and simple FFI interfaces

- **Risk**: Protocol parsing errors with malformed GUI commands causing panics
  - **Likelihood**: High
  - **Impact**: High
  - **Mitigation**: Never-panic design with comprehensive input validation and fuzzing

- **Risk**: Async/await integration with C++ blocking search operations
  - **Likelihood**: Medium
  - **Impact**: Medium
  - **Mitigation**: Careful async boundary design and tokio::task::spawn_blocking

### Integration Risks
- **Risk**: Build system complexity integrating Rust with existing CMake
  - **Likelihood**: Medium
  - **Impact**: Medium
  - **Mitigation**: Use cargo build scripts and proven integration patterns

- **Risk**: Cross-platform compilation issues with Rust + C++ + Python stack
  - **Likelihood**: Medium
  - **Impact**: High
  - **Mitigation**: CI testing on all platforms and platform-specific build documentation

### User Experience Risks
- **Risk**: GUI compatibility issues with custom Morphy options
  - **Likelihood**: Medium
  - **Impact**: Low
  - **Mitigation**: Fallback to standard UCI behavior when options not supported

## 10. Non-Functional Requirements

### Reliability
- 99.9% uptime during normal operation without memory leaks or crashes
- Graceful error recovery from invalid commands or unexpected input
- Consistent behavior across multiple GUI applications and platforms

### Scalability
- Support for concurrent analysis requests through multiple engine instances
- Efficient memory usage scaling with search depth and complexity
- Thread-safe operation allowing GUI polling during search

### Maintainability
- Clear separation between protocol handling and engine logic
- Comprehensive logging for debugging GUI compatibility issues
- Modular design allowing easy addition of new UCI commands or options

### Usability
- Intuitive option names and descriptions for engine configuration
- Helpful error messages for command syntax issues
- Responsive command processing maintaining GUI interaction feel

## 11. Future Considerations

### Phase 2 Features
- Enhanced search information output with Morphy-specific metrics
- Support for UCI_AnalyseMode and multi-PV analysis
- Custom protocol extensions for advanced Morphy-style configuration

### Integration Opportunities
- Web-based UCI interface for online play
- Integration with popular chess platforms and analysis tools
- Support for Universal Chess Protocol (UCP) when standardized

### Performance Enhancements
- Optimized command parsing with pre-compiled patterns
- Memory-mapped configuration for faster option updates
- Asynchronous command queuing for improved responsiveness

---

**Document Status**: Draft

**Last Updated**: 2025-01-20

**Stakeholders**: Opera Engine development team, chess GUI developers, tournament organizers

**Related Documents**: 
- .claude/CLAUDE.md (Project requirements)
- .claude/UCI.md (UCI protocol reference)
- cpp/include/Types.h (Core engine types)

**Version**: 1.0