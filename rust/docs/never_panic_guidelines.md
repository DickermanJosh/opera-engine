# Never-Panic Coding Guidelines for Opera UCI Engine

## Overview

The Opera UCI Engine follows a strict **never-panic** philosophy to ensure reliable operation when integrated with chess GUI applications. Panics in UCI engines can crash the entire GUI or tournament system, making robust error handling essential.

## Core Principles

### 1. **Comprehensive Error Handling**
- Every fallible operation must return a `Result<T, UCIError>`
- Use the custom error types defined in `error.rs`
- Never use `.unwrap()`, `.expect()`, or operations that can panic

### 2. **Safe Alternatives to Panic-Prone Operations**

#### String Parsing
```rust
// ❌ BAD: Can panic on invalid input
let number: i32 = input.parse().unwrap();

// ✅ GOOD: Use safe parsing utilities
use crate::error::never_panic;
let number = never_panic::safe_parse::<i32>(input, "move depth")?;
```

#### Array/Vector Access
```rust
// ❌ BAD: Can panic on out-of-bounds access
let item = vec[index];

// ✅ GOOD: Use safe access utilities
let item = never_panic::safe_get(&vec, index, "command tokens")?;
```

#### String Slicing
```rust
// ❌ BAD: Can panic on invalid indices
let substr = &input[start..end];

// ✅ GOOD: Use safe slicing utilities
let substr = never_panic::safe_slice(input, start, Some(end), "UCI command")?;
```

#### Division Operations
```rust
// ❌ BAD: Can panic on division by zero
let result = numerator / denominator;

// ✅ GOOD: Use safe division utilities
let result = never_panic::safe_divide(numerator, denominator, "time calculation")?;
```

### 3. **Error Context and Recovery**

#### Adding Context to Errors
```rust
use crate::error::{ResultExt, ErrorContext};

// Add simple context
let result = risky_operation()
    .with_operation("parsing UCI command");

// Add detailed context
let context = ErrorContext::new("position setup")
    .detail("FEN string provided")
    .detail("move list validation");
let result = risky_operation()
    .with_context(context);
```

#### Error Recovery Strategies
```rust
use crate::error::recovery;

match dangerous_operation() {
    Ok(value) => value,
    Err(error) => {
        // Apply appropriate recovery strategy
        recovery::recover_from_error(&error, "command processing");
        
        match recovery::recovery_strategy(&error) {
            recovery::RecoveryAction::ContinueWithDefault => default_value(),
            recovery::RecoveryAction::Skip => return Ok(()),
            recovery::RecoveryAction::ResetState => {
                reset_engine_state();
                default_value()
            }
            recovery::RecoveryAction::Terminate => {
                return Err(error);
            }
            _ => default_value(),
        }
    }
}
```

### 4. **FFI Safety**

#### Safe C++ Integration
```rust
// Always check for null pointers from C++
let board = ffi::create_board();
if board.is_null() {
    return Err(UCIError::Ffi {
        message: "Failed to create C++ board instance".to_string(),
    });
}

// Validate C++ function results
let success = ffi::board_set_fen(board.pin_mut(), fen);
if !success {
    return Err(UCIError::Position {
        message: format!("Invalid FEN string: {}", fen),
    });
}
```

#### Error Callbacks from C++
```rust
// Implement error callbacks that never panic
pub fn on_engine_error(error_msg: String) {
    use tracing::error;
    
    // Log error with structured logging
    error!(error = %error_msg, "C++ engine error");
    
    // Report to GUI via UCI protocol
    println!("info string ERROR: {}", error_msg);
    
    // Never panic, even on severe errors
}
```

### 5. **Input Validation**

#### Command Parsing
```rust
fn parse_uci_command(line: &str) -> UCIResult<Command> {
    // Always validate input before processing
    if line.trim().is_empty() {
        return Err(UCIError::Protocol {
            message: "Empty command line".to_string(),
        });
    }
    
    let tokens: Vec<&str> = line.split_whitespace().collect();
    
    // Use safe array access
    let command_name = never_panic::safe_get(&tokens, 0, "command parsing")?;
    
    match *command_name {
        "uci" => Ok(Command::Uci),
        "quit" => Ok(Command::Quit),
        "go" => parse_go_command(&tokens[1..]), // Safe slice, already validated
        _ => Err(UCIError::Protocol {
            message: format!("Unknown command: {}", command_name),
        }),
    }
}
```

#### Position Validation
```rust
fn set_position(fen: &str) -> UCIResult<()> {
    // Validate FEN string format
    if fen.len() > 100 {  // Reasonable upper bound
        return Err(UCIError::Position {
            message: "FEN string too long".to_string(),
        });
    }
    
    // Check basic FEN structure
    let parts: Vec<&str> = fen.split_whitespace().collect();
    if parts.len() != 6 {
        return Err(UCIError::Position {
            message: format!("Invalid FEN format: expected 6 parts, got {}", parts.len()),
        });
    }
    
    // Proceed with C++ validation
    let board = ffi::create_board();
    // ... continue with safe FFI calls
}
```

### 6. **Async Safety**

#### Timeout Handling
```rust
use tokio::time::{timeout, Duration};

async fn safe_search_with_timeout(time_ms: u64) -> UCIResult<String> {
    let duration = Duration::from_millis(time_ms);
    
    match timeout(duration, perform_search()).await {
        Ok(Ok(result)) => Ok(result),
        Ok(Err(search_error)) => Err(search_error),
        Err(_timeout) => Err(UCIError::Timeout { duration_ms: time_ms }),
    }
}
```

#### Channel Communication
```rust
use tokio::sync::mpsc;

async fn safe_command_processing() -> UCIResult<()> {
    let (tx, mut rx) = mpsc::unbounded_channel();
    
    while let Some(command) = rx.recv().await {
        // Process commands with error handling
        if let Err(error) = process_command(command).await {
            // Log error and continue (don't panic)
            tracing::warn!("Command processing error: {}", error);
            recovery::recover_from_error(&error, "command processing");
            // Continue processing other commands
        }
    }
    
    Ok(())
}
```

### 7. **Testing Never-Panic Behavior**

#### Property-Based Testing
```rust
use proptest::prelude::*;

proptest! {
    #[test]
    fn never_panic_on_random_input(input in ".*") {
        // Test that any input never causes panic
        let result = parse_uci_command(&input);
        
        // Should either succeed or return structured error
        match result {
            Ok(_) => {}, // Success is fine
            Err(UCIError::Protocol { .. }) => {}, // Expected error type
            Err(other) => panic!("Unexpected error type: {:?}", other),
        }
    }
}
```

#### Fuzzing Integration
```rust
// For use with cargo-fuzz
#[cfg(fuzzing)]
pub fn fuzz_uci_input(data: &[u8]) {
    if let Ok(input) = std::str::from_utf8(data) {
        // Should never panic regardless of input
        let _ = parse_uci_command(input);
    }
}
```

## Common Anti-Patterns to Avoid

### 1. **Direct Panic Operations**
```rust
// ❌ NEVER use these operations:
.unwrap()           // Can panic on None/Err
.expect("message")  // Can panic with custom message
panic!("message")   // Direct panic
unreachable!()      // Can panic if reached
todo!()            // Panics in production
unimplemented!()   // Panics when called

// ❌ NEVER use indexing operations:
vec[index]         // Can panic on bounds
array[index]       // Can panic on bounds
&str[start..end]   // Can panic on invalid indices
```

### 2. **Unchecked Arithmetic**
```rust
// ❌ BAD: Can overflow and panic (in debug mode)
let result = a + b;
let quotient = numerator / denominator;

// ✅ GOOD: Use checked arithmetic
let result = a.checked_add(b).ok_or_else(|| UCIError::Internal {
    message: "Integer overflow in calculation".to_string(),
})?;

let quotient = never_panic::safe_divide(numerator, denominator, "score calculation")?;
```

### 3. **Unvalidated External Input**
```rust
// ❌ BAD: Trusting external input without validation
fn process_move(move_str: &str) -> Move {
    let from = move_str[0..2].parse().unwrap(); // Can panic!
    let to = move_str[2..4].parse().unwrap();   // Can panic!
    Move::new(from, to)
}

// ✅ GOOD: Validate all external input
fn process_move(move_str: &str) -> UCIResult<Move> {
    if move_str.len() < 4 {
        return Err(UCIError::Move {
            message: format!("Move string too short: '{}'", move_str),
        });
    }
    
    let from_str = never_panic::safe_slice(move_str, 0, Some(2), "move parsing")?;
    let to_str = never_panic::safe_slice(move_str, 2, Some(4), "move parsing")?;
    
    let from = parse_square(from_str)?;
    let to = parse_square(to_str)?;
    
    Ok(Move::new(from, to))
}
```

## Error Recovery Guidelines

### 1. **Protocol Errors**: Continue with safe defaults
- Invalid commands → ignore and continue
- Malformed parameters → use default values
- Unknown options → ignore and continue

### 2. **Engine Errors**: Reset state and continue
- C++ engine errors → reset engine state
- Search failures → return default move
- Position errors → reset to starting position

### 3. **Resource Errors**: Graceful degradation
- Out of memory → reduce search depth
- Time pressure → return best move found so far
- Thread failures → fall back to single-threaded operation

### 4. **Critical Errors**: Graceful shutdown
- FFI failures → attempt restart once, then shutdown
- Corrupted state → save debug info and shutdown cleanly
- Repeated failures → enter safe mode or shutdown

## Implementation Checklist

When implementing new functionality:

- [ ] All fallible operations return `Result<T, UCIError>`
- [ ] No `.unwrap()`, `.expect()`, or panic operations used
- [ ] Input validation for all external data
- [ ] Safe alternatives used for indexing, slicing, arithmetic
- [ ] Error context added with `ResultExt` trait
- [ ] Recovery strategies considered and implemented
- [ ] FFI safety checks for all C++ integration
- [ ] Async operations use timeouts where appropriate
- [ ] Property-based tests added for critical functions
- [ ] Documentation updated with error conditions

By following these guidelines, the Opera UCI Engine maintains its never-panic guarantee while providing robust, reliable chess engine functionality for GUI integration.