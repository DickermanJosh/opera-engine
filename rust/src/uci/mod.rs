// UCI (Universal Chess Interface) module for Opera Chess Engine
//
// This module provides a complete UCI protocol implementation with zero-copy parsing,
// comprehensive input validation, and never-panic operation for production use.

pub mod commands;
pub mod engine;
pub mod event_loop;
pub mod handlers;
pub mod parser;
pub mod response;
pub mod sanitizer;
pub mod state;

pub use commands::{ChessMove, Position, TimeControl, UCICommand};
pub use engine::{EngineCommand, EngineIdentification, SearchResult, UCIEngine};
pub use event_loop::{UCIEventLoop, EventLoopConfig, EventLoopStats, run_uci_event_loop};
pub use handlers::BasicCommandHandler;
pub use parser::{BatchParser, ParserStats, ZeroCopyParser};
pub use response::{UCIResponse, ResponseFormatter, InfoBuilder, BestMoveBuilder};
pub use sanitizer::{InputLimits, InputSanitizer};
pub use state::{EngineConfig, EngineState, EngineStatistics, SearchContext, StateChangeEvent, UCIState};

// Re-export commonly used error types
pub use crate::error::{UCIError, UCIResult};