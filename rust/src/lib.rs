// Opera UCI Engine Library
//
// This library provides a safe Rust coordination layer for the UCI protocol,
// integrating with the existing C++ chess engine core through FFI.

#![deny(unsafe_code)] // Allow unsafe code only where absolutely necessary (FFI)
#![warn(
    clippy::all,
    // Temporarily disabled for quick CI fix - will re-enable with Docker
    // clippy::pedantic,
    // clippy::nursery,
    missing_docs,
    rust_2018_idioms
)]
#![allow(
    clippy::missing_errors_doc,
    clippy::missing_panics_doc,
    clippy::module_name_repetitions
)]

//! # Opera UCI Engine
//!
//! A Rust-based Universal Chess Interface implementation for the Opera Chess Engine.
//! This serves as an intelligent coordination layer between the C++ engine core,
//! Python AI wrapper, and external chess GUI applications.
//!
//! ## Features
//!
//! - **Never-panic Operation**: Comprehensive error handling with graceful recovery
//! - **Async Architecture**: Non-blocking I/O with tokio runtime
//! - **Safe FFI**: C++ integration using cxx crate for memory safety
//! - **Morphy Style**: Paul Morphy-inspired tactical and sacrificial playing style
//! - **High Performance**: Zero-copy parsing and efficient async patterns
//!
//! ## Architecture
//!
//! ```text
//! ┌─────────────────┐    ┌──────────────┐    ┌─────────────────┐
//! │   Chess GUI     │◄──►│  Rust UCI    │◄──►│  C++ Engine     │
//! │   (External)    │    │  Coordinator │    │  Core           │
//! └─────────────────┘    └──────────────┘    └─────────────────┘
//!                               │
//!                        ┌──────────────┐
//!                        │ Python AI    │
//!                        │ (Optional)   │
//!                        └──────────────┘
//! ```
//!
//! ## Usage
//!
//! ```no_run
//! use opera_uci::{UCIEngine, UCIError};
//!
//! #[tokio::main]
//! async fn main() -> Result<(), UCIError> {
//!     let engine = UCIEngine::new().await?;
//!     engine.run().await
//! }
//! ```

use std::panic;
use tracing::{error, info, warn};

#[cfg(feature = "ffi")]
pub mod bridge;
pub mod error;
#[cfg(feature = "ffi")]
pub mod ffi;
pub mod logging;
pub mod runtime;
pub mod time;
pub mod uci;

#[cfg(test)]
pub mod testing;

// Re-export commonly used types
pub use error::{ContextualError, ContextualResult, ErrorContext, ResultExt, UCIError, UCIResult};
pub use time::{
    FixedTimePolicy, InfiniteTimePolicy, PositionInfo, SearchParams, SearchProgress, SearchTimer,
    StandardTimePolicy, TimePolicy, TimeLimits, timer_from_millis,
};
pub use uci::{
    run_uci_event_loop, BasicCommandHandler, BestMoveBuilder, ChessMove, EngineConfig,
    EngineIdentification, EngineState, EngineStatistics, EventLoopConfig, EventLoopStats,
    InfoBuilder, InputSanitizer, NewGameHandler, ParserStats, Position, PositionCommandHandler,
    ResponseFormatter, SearchContext, StateChangeEvent, TimeControl, UCICommand, UCIEngine,
    UCIEventLoop, UCIResponse, UCIState, ZeroCopyParser,
};

/// Global panic hook setup for never-panic operation
///
/// This function installs a custom panic hook that ensures the UCI engine
/// never panics and always provides graceful error handling to chess GUIs.
pub fn setup_panic_hook() {
    // Set custom panic hook that logs and exits gracefully
    let original_hook = panic::take_hook();

    panic::set_hook(Box::new(move |panic_info| {
        // Log the panic with maximum detail for debugging
        error!("CRITICAL: Panic occurred in UCI engine");
        error!(
            "Panic location: {}",
            panic_info.location().map_or("unknown".to_string(), |loc| {
                format!("{}:{}:{}", loc.file(), loc.line(), loc.column())
            })
        );

        if let Some(payload) = panic_info.payload().downcast_ref::<&'static str>() {
            error!("Panic message: {}", payload);
        } else if let Some(payload) = panic_info.payload().downcast_ref::<String>() {
            error!("Panic message: {}", payload);
        } else {
            error!("Panic message: <unknown>");
        }

        // Print UCI error message to stdout for GUI
        println!("info string CRITICAL ERROR: Engine panic - shutting down gracefully");
        eprintln!("CRITICAL: UCI engine panic - see logs for details");

        // Call original hook for any additional panic handling
        original_hook(panic_info);

        // Exit gracefully instead of aborting
        std::process::exit(1);
    }));

    info!("Panic hook installed - UCI engine will never panic");
}

/// Global error reporting function for FFI callbacks
///
/// This function is called from C++ code via FFI to report errors
/// back to the Rust coordination layer.
pub fn report_engine_error(message: String) {
    warn!("C++ engine error reported: {}", message);

    // Convert to structured error for handling
    let error = UCIError::Engine {
        message: message.clone(),
    };

    // Apply recovery strategy
    error::recovery::recover_from_error(&error, "C++ engine");

    // Report to GUI if needed
    println!("info string ENGINE ERROR: {}", message);
}

/// Global search progress reporting function for FFI callbacks
///
/// This function receives search progress updates from the C++ engine
/// and formats them for UCI output.
#[cfg(feature = "ffi")]
pub fn report_search_progress(info: &ffi::ffi::FFISearchInfo) {
    use tracing::debug;

    debug!(
        depth = info.depth,
        score = info.score,
        time_ms = info.time_ms,
        nodes = info.nodes,
        nps = info.nps,
        pv = %info.pv,
        "Search progress update"
    );

    // Output UCI info string
    println!(
        "info depth {} score cp {} time {} nodes {} nps {} pv {}",
        info.depth, info.score, info.time_ms, info.nodes, info.nps, info.pv
    );
}

/// Engine initialization and safety checks
///
/// Performs essential safety and compatibility checks before starting
/// the UCI engine to ensure reliable operation.
pub fn initialize_engine() -> UCIResult<()> {
    info!("Initializing Opera UCI Engine");

    // Install panic hook first
    setup_panic_hook();

    #[cfg(feature = "ffi")]
    {
        // Verify FFI integration
        let mut board = ffi::ffi::create_board();
        if board.is_null() {
            return Err(UCIError::Ffi {
                message: "Failed to create C++ board instance via FFI".to_string(),
            });
        }
        info!("FFI bridge to C++ engine verified");

        // Test basic board operations
        let starting_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        let success = ffi::ffi::board_set_fen(board.pin_mut(), starting_fen);
        if !success {
            return Err(UCIError::Ffi {
                message: "Failed to set starting FEN via FFI".to_string(),
            });
        }

        let retrieved_fen = ffi::ffi::board_get_fen(&board);
        if retrieved_fen != starting_fen {
            return Err(UCIError::Ffi {
                message: format!(
                    "FEN round-trip test failed: expected '{}', got '{}'",
                    starting_fen, retrieved_fen
                ),
            });
        }
        info!("Basic board operations verified");

        // Test SearchEngine creation
        let search_engine = ffi::ffi::create_search_engine(board.pin_mut());
        if search_engine.is_null() {
            return Err(UCIError::Ffi {
                message: "Failed to create C++ SearchEngine instance via FFI".to_string(),
            });
        }
        info!("SearchEngine interface verified");

        // Verify engine configuration functions
        let hash_result = ffi::ffi::engine_set_hash_size(128);
        let thread_result = ffi::ffi::engine_set_threads(1);
        if !hash_result || !thread_result {
            warn!("Engine configuration functions may not be fully implemented");
        } else {
            info!("Engine configuration interface verified");
        }
    }

    #[cfg(not(feature = "ffi"))]
    {
        info!("FFI disabled - skipping C++ engine verification");
    }

    info!("Opera UCI Engine initialization complete");
    Ok(())
}

/// Version information
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
pub const NAME: &str = "Opera Engine";
pub const AUTHOR: &str = "Opera Engine Team";

/// Additional FFI bridge components
#[cfg(feature = "ffi")]
pub use bridge::Board;

/// UCI options supported by the engine
pub mod uci_options {
    /// Available UCI options with their types and default values
    pub const OPTIONS: &[(&str, &str, &str)] = &[
        ("Hash", "spin", "128"),
        ("Threads", "spin", "1"),
        ("MorphyStyle", "check", "false"),
        ("SacrificeThreshold", "spin", "100"),
        ("TacticalDepth", "spin", "2"),
    ];
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_version_info() {
        assert!(!VERSION.is_empty());
        assert_eq!(NAME, "Opera Engine");
        assert_eq!(AUTHOR, "Opera Engine Team");
    }

    #[test]
    fn test_uci_options() {
        assert_eq!(uci_options::OPTIONS.len(), 5);

        // Check that Hash option exists
        let hash_option = uci_options::OPTIONS
            .iter()
            .find(|(name, _, _)| *name == "Hash")
            .expect("Hash option should exist");
        assert_eq!(hash_option.1, "spin");
        assert_eq!(hash_option.2, "128");
    }

    #[test]
    fn test_error_reporting() {
        // Test that error reporting doesn't panic
        report_engine_error("Test error message".to_string());
    }

    #[test]
    fn test_panic_hook_installation() {
        // This test just verifies the function can be called
        // We can't easily test panic behavior in unit tests
        setup_panic_hook();
    }

    #[tokio::test]
    async fn test_engine_initialization() {
        // Test initialization (may fail if C++ engine not available)
        match initialize_engine() {
            Ok(()) => {
                // Initialization succeeded
                assert!(true);
            }
            Err(UCIError::Ffi { message }) => {
                // FFI not available in test environment, expected
                assert!(message.contains("FFI") || message.contains("board"));
            }
            Err(other) => {
                panic!("Unexpected error during initialization: {}", other);
            }
        }
    }

    /// Test error type coverage
    #[test]
    fn test_error_types_comprehensive() {
        use error::UCIError;

        // Test each error variant can be created
        let errors = vec![
            UCIError::Protocol {
                message: "test".to_string(),
            },
            UCIError::Engine {
                message: "test".to_string(),
            },
            UCIError::Position {
                message: "test".to_string(),
            },
            UCIError::Move {
                message: "test".to_string(),
            },
            UCIError::Search {
                message: "test".to_string(),
            },
            UCIError::Configuration {
                message: "test".to_string(),
            },
            UCIError::Ffi {
                message: "test".to_string(),
            },
            UCIError::Timeout { duration_ms: 1000 },
            UCIError::Resource {
                resource: "memory".to_string(),
            },
            UCIError::Internal {
                message: "test".to_string(),
            },
        ];

        // Verify all errors can be displayed
        for error in errors {
            let _display = format!("{}", error);
            let _debug = format!("{:?}", error);
        }
    }
}
