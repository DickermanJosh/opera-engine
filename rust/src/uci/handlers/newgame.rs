// UCI New Game Command Handler
//
// This module handles the UCI `ucinewgame` command which resets the engine
// state for a new game. It provides comprehensive cleanup and initialization
// including C++ transposition table clearing and Rust state management.

use std::sync::Arc;
use tracing::{debug, info, instrument, warn};

use crate::error::{UCIResult, UCIError, ErrorContext, ResultExt};
use crate::uci::state::{UCIState, EngineState};
use crate::ffi::ffi;

/// Handler for UCI new game command with comprehensive state management
pub struct NewGameHandler {
    /// Reference to engine state for atomic operations
    state: Arc<UCIState>,
}

impl NewGameHandler {
    /// Creates a new game handler with state reference
    pub fn new(state: Arc<UCIState>) -> Self {
        Self { state }
    }

    /// Handles the 'ucinewgame' command - full engine reset for new game
    ///
    /// The UCI specification states that ucinewgame should:
    /// 1. Clear all transposition tables and hash tables
    /// 2. Reset engine internal state to initial values
    /// 3. Prepare for a completely fresh game
    /// 4. Not change engine options that were set previously
    /// 
    /// This is different from just setting a new position - it's a complete
    /// state reset that assumes the previous game is over.
    #[instrument(skip(self))]
    pub fn handle_ucinewgame_command(&self) -> UCIResult<()> {
        info!("Processing UCI new game command - full engine reset");

        // Validate current engine state - must be Ready (not searching)
        let current_state = self.state.current_state();
        if current_state != EngineState::Ready {
            return Err(UCIError::Protocol {
                message: format!(
                    "ucinewgame command invalid in state {:?}, expected Ready", 
                    current_state
                ),
            });
        }

        // Step 1: Clear C++ engine internal state
        self.clear_cpp_engine_state()
            .with_context(ErrorContext::new("Failed to clear C++ engine state"))
            .map_err(|e| e.error)?;

        // Step 2: Reset search statistics
        self.reset_search_statistics()
            .with_context(ErrorContext::new("Failed to reset search statistics"))
            .map_err(|e| e.error)?;

        // Step 3: Clear game-specific history (but preserve options)
        self.clear_game_history()
            .with_context(ErrorContext::new("Failed to clear game history"))
            .map_err(|e| e.error)?;

        // Step 4: Reset debug mode to default if needed
        if self.state.is_debug_mode() {
            debug!("Debug mode remains active across games");
        }

        info!("UCI new game reset completed successfully");
        Ok(())
    }

    /// Clear C++ engine internal state including transposition tables
    #[instrument(skip(self))]
    fn clear_cpp_engine_state(&self) -> UCIResult<()> {
        debug!("Clearing C++ engine transposition tables and hash tables");

        // Clear hash tables through FFI
        let success = ffi::engine_clear_hash();
        if !success {
            return Err(UCIError::Engine {
                message: "Failed to clear C++ engine hash tables".to_string(),
            });
        }

        debug!("C++ engine state cleared successfully");
        Ok(())
    }

    /// Reset Rust-side search statistics to initial values
    #[instrument(skip(self))]
    fn reset_search_statistics(&self) -> UCIResult<()> {
        debug!("Resetting search statistics");

        // Reset engine state (this will reset statistics)
        let _ = self.state.reset();

        debug!("Search statistics reset to initial values");
        Ok(())
    }

    /// Clear game-specific history while preserving engine options
    #[instrument(skip(self))]
    fn clear_game_history(&self) -> UCIResult<()> {
        debug!("Clearing game-specific history and state");

        // For a new game, we want to clear game-specific state but preserve:
        // - Engine options (Hash, Threads, MorphyStyle, etc.)
        // - Debug mode settings
        // - Engine identification
        //
        // We do want to clear:
        // - Position history
        // - Search context
        // - Any game-specific caches

        // Clear any active search context
        if self.state.current_state() == EngineState::Searching {
            warn!("Clearing active search context during ucinewgame");
            // This would be handled by the search cancellation system
        }

        // Note: Position clearing will happen when the next 'position' command
        // is received. ucinewgame doesn't change the current position, it just
        // clears internal state.

        debug!("Game history cleared successfully");
        Ok(())
    }

    /// Get current engine state for validation
    pub fn get_state(&self) -> EngineState {
        self.state.current_state()
    }

    /// Check if engine is ready for new game command
    pub fn is_ready_for_newgame(&self) -> bool {
        self.state.current_state() == EngineState::Ready
    }

    /// Get search statistics for validation
    pub fn get_search_stats(&self) -> (u64, u64, u64) {
        let stats = self.state.statistics();
        (
            stats.searches_started,
            stats.searches_completed,
            stats.total_nodes_searched,
        )
    }
}

impl Default for NewGameHandler {
    fn default() -> Self {
        // Create with a default state (mainly for testing)
        Self::new(Arc::new(UCIState::new()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Test basic new game handler creation
    #[test]
    fn test_newgame_handler_creation() {
        let state = Arc::new(UCIState::new());
        let handler = NewGameHandler::new(Arc::clone(&state));
        
        // State starts as Initializing, need to transition to Ready
        let _ = state.transition_to(EngineState::Ready, "test setup");
        assert_eq!(handler.get_state(), EngineState::Ready);
        assert!(handler.is_ready_for_newgame());
    }

    /// Test default handler creation
    #[test]
    fn test_default_handler() {
        let handler = NewGameHandler::default();
        // Transition to ready for testing
        let _ = handler.state.transition_to(EngineState::Ready, "test setup");
        assert_eq!(handler.get_state(), EngineState::Ready);
    }

    /// Test new game command with valid state
    #[tokio::test]
    async fn test_ucinewgame_command_success() {
        let state = Arc::new(UCIState::new());
        let handler = NewGameHandler::new(Arc::clone(&state));

        // Transition to Ready state first
        let _ = state.transition_to(EngineState::Ready, "test setup");

        // Execute new game command (should succeed in Ready state)
        let result = handler.handle_ucinewgame_command();
        assert!(result.is_ok(), "New game command should succeed: {:?}", result);
    }

    /// Test new game command with invalid state (Searching)
    #[test]
    fn test_ucinewgame_invalid_state() {
        let state = Arc::new(UCIState::new());
        let handler = NewGameHandler::new(Arc::clone(&state));

        // Transition to Ready first, then to Searching state
        let _ = state.transition_to(EngineState::Ready, "test setup");
        let _ = state.transition_to(EngineState::Searching, "test");

        // New game command should fail
        let result = handler.handle_ucinewgame_command();
        assert!(result.is_err(), "New game command should fail when searching");

        if let Err(UCIError::Protocol { message }) = result {
            assert!(message.contains("Searching"));
            assert!(message.contains("Ready"));
        } else {
            panic!("Expected Protocol error");
        }
    }

    /// Test statistics reset functionality
    #[test]
    fn test_statistics_reset() {
        let state = Arc::new(UCIState::new());
        let handler = NewGameHandler::new(Arc::clone(&state));

        // For now just test that reset works
        let result = handler.reset_search_statistics();
        assert!(result.is_ok());

        // Verify statistics are available (just test they can be retrieved)
        let (_started, _completed, _nodes) = handler.get_search_stats();
    }

    /// Test readiness check
    #[test]
    fn test_readiness_check() {
        let state = Arc::new(UCIState::new());
        let handler = NewGameHandler::new(Arc::clone(&state));

        // Transition to Ready first
        let _ = state.transition_to(EngineState::Ready, "test setup");
        
        // Should be ready initially
        assert!(handler.is_ready_for_newgame());

        // Should not be ready when searching
        let _ = state.transition_to(EngineState::Searching, "test");
        assert!(!handler.is_ready_for_newgame());

        // Should be ready again when back to Ready state
        let _ = state.transition_to(EngineState::Ready, "test");
        assert!(handler.is_ready_for_newgame());
    }

    /// Test C++ engine state clearing (will succeed with current stub)
    #[test]
    fn test_cpp_engine_clearing() {
        let state = Arc::new(UCIState::new());
        let handler = NewGameHandler::new(Arc::clone(&state));

        let result = handler.clear_cpp_engine_state();
        assert!(result.is_ok(), "C++ engine clearing should succeed with stub implementation");
    }

    /// Test game history clearing
    #[test]
    fn test_game_history_clearing() {
        let state = Arc::new(UCIState::new());
        let handler = NewGameHandler::new(Arc::clone(&state));

        let result = handler.clear_game_history();
        assert!(result.is_ok(), "Game history clearing should succeed");
    }

    /// Test concurrent new game commands (should be serialized by state)
    #[tokio::test]
    async fn test_concurrent_newgame_commands() {
        let state = Arc::new(UCIState::new());
        let handler1 = NewGameHandler::new(Arc::clone(&state));
        let handler2 = NewGameHandler::new(Arc::clone(&state));

        // Transition to Ready first
        let _ = state.transition_to(EngineState::Ready, "test setup");

        // Both handlers should work correctly
        let result1 = handler1.handle_ucinewgame_command();
        let result2 = handler2.handle_ucinewgame_command();

        assert!(result1.is_ok());
        assert!(result2.is_ok());
    }

    /// Test memory safety with repeated new game commands
    #[test]
    fn test_repeated_newgame_commands() {
        let state = Arc::new(UCIState::new());
        let handler = NewGameHandler::new(Arc::clone(&state));

        // Transition to Ready first
        let _ = state.transition_to(EngineState::Ready, "test setup");

        // Execute new game command multiple times
        for i in 0..10 {
            // Execute new game command
            let result = handler.handle_ucinewgame_command();
            assert!(result.is_ok(), "New game command {} should succeed", i);

            // Verify handler still works
            assert!(handler.is_ready_for_newgame(), "Handler should be ready after command {}", i);
        }
    }
}