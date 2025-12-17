# Chess Engine Project - Development Guide

## Project Overview
A UCI-compliant chess engine with Paul Morphy-inspired playing style, built as a monorepo with C++ core, Rust UCI system and Python AI wrapper. Refer to the project as: The Opera Engine / Opera Engine

### Core Principles
- **Performance First**: C++ handles all time-critical operations (board representation, move generation, search)
- **Clean Abstraction**: Clear separation between engine mechanics and AI logic
- **UCI Compliance**: Full Universal Chess Interface protocol implementation
- **Extensibility**: Modular design allowing easy integration with multiple chess platforms
- **Testability**: Comprehensive unit testing for all components

### Test Driven Development Principles 
- **Tests First** ALWAYS create tests with expected behavior accounting for edge cases before writing any code. 
- **Negative Tests** - Write tests that are expected to fail as well as ones that are expected to pass. It is not explicit that things that should fair will, and thus always need to be tested.
- **Don't Modify Tests** - Think very hard when writing tests. Treat it entirely as its own phase of the project that needs to be thought about and crafted. Once tests are written and finalized, they should never be changed. (Adding new tests later is fine, but should be treated as a last resort due to a missed edge case, not something to rely upon).
- **100% Coverage** - Always cover 100% of the codebase with tests. Every single function should have a test or multiple tests depending on the situation.

## Kiro System - Adaptation of Amazon's Spec-Driven Development

This project uses an adaptation of Amazon's **Kiro System** for structured feature development. The original Kiro system has been adapted to work with Claude Code through templates and workflow guidance.

### Kiro Workflow (Amazon's 3-Phase Approach)
1. **Requirements** (`requirements.md`) - What needs to be built
2. **Design** (`design.md`) - How it will be built  
3. **Tasks** (`tasks.md`) - Step-by-step implementation plan

### Directory Structure
- `.kiro/specs/{feature-name}/` - Individual feature specifications
- `.kiro/kiro-system-templates/` - Templates and documentation
	- `requirements_template.md` - Template for requirements
	- `design_template.md` - Template for technical design
	- `tasks_template.md` - Template for implementation tasks
	- `how_kiro_works.md` - Detailed Kiro documentation

### How Claude Code Should Work with Kiro

#### When Asked to Create New Features:
1. **Check for existing specs first**: Look in `.kiro/specs/` for any existing feature documentation
2. **Use templates**: Copy templates from `.kiro/kiro-system-templates/` when creating new specs
3. **Follow the 3-phase process**: Requirements → Design → Tasks → Implementation
4. **Require approval**: Each phase needs explicit user approval before proceeding

#### Template Usage:
- **Requirements**: Use `requirements_template.md` to create user stories and EARS acceptance criteria
- **Design**: Use `design_template.md` for technical architecture and component design
- **Tasks**: Use `tasks_template.md` to break down implementation into numbered, actionable tasks

#### During Implementation:
- **Reference requirements**: Always link tasks back to specific requirements
- **Work incrementally**: Implement tasks one at a time, not all at once
- **Validate against specs**: Ensure implementations match the design and requirements
- **Update documentation**: Keep specs updated if changes are needed

#### Key Behaviors:
- **Always suggest using Kiro** when user wants to build new features
- **Guide through templates** if user is unfamiliar with the process
- **Enforce the approval process** - don't skip phases
- **Maintain traceability** from requirements to code

## Claude / Codex Collaboration

You MUST use Codex as a code review agent during task development. Read .claude/CLAUDE_CODEX.md for further instructions on how to utilize CODEX.

## C++ Core Requirements

### Board Representation
- **Bitboards**: Use 64-bit integers for piece representation
- **Magic Bitboards**: For sliding piece move generation
- **Zobrist Hashing**: For transposition table keys
- **FEN Support**: Full Forsyth-Edwards Notation parsing/generation

### Design Patterns to Implement
- **Command Pattern**: For move execution/undo
- **Strategy Pattern**: For different search algorithms
- **Observer Pattern**: For position evaluation updates
- **Factory Pattern**: For creating different board configurations
- **Singleton Pattern**: For global resources (hash tables, magic numbers)

### Code Review Practice
- **READ AND FOLLOW**: .claude/CLAUDE_CODEX.md
    - Codex is ALWAYS available to you to use as an assistant and code reviewer.

### Performance Requirements
- **Move Generation**: < 1ms for complex positions
- **Search Speed**: > 1M nodes/second on modern hardware
- **Memory Usage**: Configurable hash table sizes
- **Threading**: Support for multiple search threads

## Python AI Wrapper

### Integration Layer
- **pybind11**: For C++ binding (not ctypes)
- **Seamless Interface**: Python classes mirror C++ objects
- **Error Handling**: Proper exception propagation

### AI Components
```python
class MorphyEvaluator:
    """Implements Paul Morphy-inspired position evaluation"""
    
    def __init__(self):
        self.model = tf.keras.load_model('morphy_model.h5')
        
    def evaluate_sacrifice_potential(self, position):
        """Identify positions where sacrifices lead to binding attacks"""
        pass
        
    def tactical_pattern_recognition(self, position):
        """Recognize typical Morphy tactical motifs"""
        pass

class StyleTrainer:
    """Train neural networks on Morphy-style games"""
    
    def prepare_morphy_dataset(self):
        """Augment limited Morphy games with similar patterns"""
        pass
        
    def reward_shaping(self, position, move):
        """Custom reward function favoring sacrificial play"""
        pass
```

## Paul Morphy Playing Style Implementation

### Strategic Characteristics to Implement
1. **Rapid Development**: Prioritize piece activity over material
2. **King Safety**: Aggressive pursuit of enemy king
3. **Initiative**: Maintain tempo even at material cost
4. **Tactical Brilliance**: Pattern recognition for sacrificial combinations
5. **Elegant Simplicity**: Prefer forcing, clear-cut variations

### Technical Approaches

#### 1. Modified Evaluation Function
```cpp
class MorphyEvaluator : public Evaluator {
    int evaluate(const Board& board) override {
        int score = materialBalance(board);
        
        // Morphy-specific adjustments
        score += developmentBonus(board) * 1.2;    // Overvalue development
        score += kingSafetyThreat(board) * 1.5;    // Aggressive king hunt
        score += initiativeBonus(board) * 1.1;    // Reward tempo
        score -= passivityPenalty(board) * 0.8;   // Punish passive play
        
        return score;
    }
};
```

#### 2. Sacrifice Detection System
- **Pattern Database**: Store known sacrificial motifs
- **Tactical Search**: Deep search on forcing sequences
- **Compensation Analysis**: Evaluate long-term positional gains

#### 3. Training Data Augmentation
- **Style Transfer**: Use modern games with Morphy-like characteristics
- **Position Similarity**: Cluster similar tactical patterns
- **Synthetic Generation**: Create training positions from known motifs

#### 4. Custom Search Modifications
```cpp
class MorphySearch : public Search {
    int search(Board& board, int depth, bool allowSacrifice = true) {
        if (allowSacrifice && detectSacrificeOpportunity(board)) {
            // Extend search for sacrificial lines
            return search(board, depth + 2, false);
        }
        return alphaBeta(board, depth, -INFINITY, INFINITY);
    }
};
```

## UCI Protocol Implementation

### Required Commands
- `uci` - Engine identification
- `isready` - Engine ready status
- `ucinewgame` - New game initialization
- `position` - Set board position
- `go` - Start search
- `stop` - Stop search
- `quit` - Engine shutdown

### Custom Options
- `MorphyStyle` - Enable/disable Morphy playing style
- `SacrificeThreshold` - Material threshold for sacrificial play
- `TacticalDepth` - Extra depth for tactical sequences

## Development Standards

### Code Quality
- **Modern C++17/20**: Use latest standard features
- **RAII**: Proper resource management
- **const-correctness**: Immutable by default
- **No raw pointers**: Use smart pointers or references
- **Comprehensive testing**: Unit tests for all components

### Performance Guidelines
- **Profiling**: Regular performance measurement
- **Optimization**: Profile-guided optimization
- **Memory**: Minimize allocations in search
- **Cache-friendly**: Data structure layout optimization

### Documentation
- **Doxygen**: API documentation for all public interfaces
- **README**: Clear setup and usage instructions
- **Architecture**: High-level design documentation
- **Performance**: Benchmarking results and targets

## Build System

### CMake Configuration
- **Cross-platform**: Windows, Linux, macOS support
- **Dependency Management**: vcpkg or Conan integration
- **Testing**: CTest integration
- **Packaging**: CPack for distribution

### Python Setup
- **Virtual Environment**: Isolated dependency management
- **Requirements**: Pinned versions for reproducibility
- **Testing**: pytest for all Python components
- **Linting**: Black, flake8, mypy for code quality

## Integration Points

### Platform Connectors
- **Lichess Bot API**: Real-time game participation
- **Custom Chess App**: WebSocket/REST API integration
- **Analysis Mode**: Position analysis and move suggestions
- **Tournament Mode**: Automated tournament participation

### Configuration Management
- **YAML/JSON**: Human-readable configuration files
- **Environment Variables**: Runtime configuration
- **Command Line**: Override configuration options
- **Validation**: Schema validation for all configs

## Success Metrics

### Technical Performance
- **Tactical Rating**: > 2000 ELO on tactical puzzles
- **Playing Strength**: Competitive at club level
- **Style Recognition**: Demonstrable Morphy-like characteristics
- **Code Quality**: > 95% test coverage, zero warnings

### Behavioral Goals
- **Sacrificial Play**: Regularly finds sound sacrifices
- **Development Speed**: Fast piece development
- **King Safety**: Prioritizes king attacks
- **Game Beauty**: Produces aesthetically pleasing games

## Next Steps for Implementation

1. **Setup Project Structure**: Initialize monorepo with build systems
2. **Implement Core Board**: Bitboard representation with magic bitboards
3. **Basic Move Generation**: Legal move generation with performance tests
4. **UCI Interface**: Minimal UCI compliance for testing
5. **Search Framework**: Alpha-beta with transposition table
6. **Python Integration**: Basic pybind11 wrapper
7. **Morphy Evaluation**: Initial style-specific evaluation
8. **Platform Integration**: First external platform connection
9. **Training Pipeline**: Neural network training infrastructure
10. **Style Refinement**: Iterative improvement of Morphy characteristics

Always update your progress and decisions inside .claude/PROGRESS.md, a premade file.

---

## Development Philosophy

This project prioritizes **elegant code architecture** and **distinctive playing style** over pure engine strength. Every component should be:
- **Well-tested**: Comprehensive unit and integration tests
- **Clearly documented**: Self-explanatory code with good comments. Comment the WHY. NOT the what - unless it is absolutely necessary for human readability
- **Performance-conscious**: Maximum efficiency - as optimized as possible. ALWAYS consider if operations or entire systems could be optimized more, both at the bit and architectural level.
- **Modular**: Easy to extend and modify
- **Style-consistent**: Reflects the Morphy aesthetic in both code and play

Remember: We're not just building a chess engine - we're creating a digital tribute to the chess romantic era, where beauty and brilliance matter as much as evaluation points.

General notes: 
- Always explicitly run commands with bash in front of them whenever you are in a shell environment. ex: ```cd foo``` -> ```bash cd foo```
- **Never** commit or push to git / github without confirmation from the user