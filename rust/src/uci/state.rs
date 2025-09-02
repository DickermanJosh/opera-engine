// UCI Engine State Management with Thread-Safe Operations
//
// This module provides thread-safe state management for the UCI engine with atomic operations
// and async-compatible locking for high-performance concurrent operation.

use std::sync::atomic::{AtomicBool, AtomicU64, AtomicU8, Ordering};
use parking_lot::RwLock;
use tokio::sync::broadcast;
use tracing::{debug, info, warn};

use crate::error::{UCIError, UCIResult};
use crate::uci::commands::TimeControl;

/// UCI Engine operational states
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum EngineState {
    /// Engine is starting up and initializing
    Initializing = 0,
    /// Engine is ready to receive commands
    Ready = 1,
    /// Engine is searching for the best move
    Searching = 2,
    /// Engine is pondering (thinking on opponent's time)
    Pondering = 3,
    /// Engine is shutting down gracefully
    Stopping = 4,
    /// Engine has encountered an error and needs reset
    Error = 5,
}

impl EngineState {
    /// Check if the engine can accept new commands in this state
    pub fn can_accept_commands(&self) -> bool {
        matches!(self, EngineState::Ready | EngineState::Pondering)
    }

    /// Check if the engine is actively computing
    pub fn is_computing(&self) -> bool {
        matches!(self, EngineState::Searching | EngineState::Pondering)
    }

    /// Check if the engine is in a terminal state
    pub fn is_terminal(&self) -> bool {
        matches!(self, EngineState::Stopping | EngineState::Error)
    }

    /// Get human-readable state description
    pub fn description(&self) -> &'static str {
        match self {
            EngineState::Initializing => "Initializing engine components",
            EngineState::Ready => "Ready to receive commands",
            EngineState::Searching => "Searching for best move",
            EngineState::Pondering => "Pondering on opponent time",
            EngineState::Stopping => "Shutting down gracefully", 
            EngineState::Error => "Error state - reset required",
        }
    }
}

impl From<u8> for EngineState {
    fn from(value: u8) -> Self {
        match value {
            0 => EngineState::Initializing,
            1 => EngineState::Ready,
            2 => EngineState::Searching,
            3 => EngineState::Pondering,
            4 => EngineState::Stopping,
            5 => EngineState::Error,
            _ => EngineState::Error, // Default to error for invalid values
        }
    }
}

/// State change events for monitoring and debugging
#[derive(Debug, Clone)]
pub struct StateChangeEvent {
    pub from: EngineState,
    pub to: EngineState,
    pub timestamp: std::time::Instant,
    pub reason: String,
}

/// Thread-safe UCI engine state manager
pub struct UCIState {
    /// Current engine state (atomic for lock-free reads)
    current_state: AtomicU8,
    
    /// Debug mode flag
    debug_mode: AtomicBool,
    
    /// Search statistics (atomic counters)
    searches_started: AtomicU64,
    searches_completed: AtomicU64,
    total_nodes_searched: AtomicU64,
    
    /// Current search context (protected by RwLock for infrequent updates)
    search_context: RwLock<Option<SearchContext>>,
    
    /// State change notification channel
    state_change_tx: broadcast::Sender<StateChangeEvent>,
    
    /// Engine configuration (protected by RwLock)
    config: RwLock<EngineConfig>,
}

/// Current search context information
#[derive(Debug, Clone)]
pub struct SearchContext {
    pub start_time: std::time::Instant,
    pub time_control: TimeControl,
    pub max_depth: Option<u32>,
    pub max_nodes: Option<u64>,
    pub is_infinite: bool,
    pub is_ponder: bool,
}

/// Engine configuration parameters
#[derive(Debug, Clone)]
pub struct EngineConfig {
    pub hash_size_mb: u32,
    pub thread_count: u32,
    pub ponder_enabled: bool,
    pub multithread_enabled: bool,
    pub analysis_mode: bool,
    pub contempt_factor: i32,
}

impl Default for EngineConfig {
    fn default() -> Self {
        Self {
            hash_size_mb: 16,        // 16MB default hash
            thread_count: 1,         // Single-threaded by default  
            ponder_enabled: false,   // No pondering by default
            multithread_enabled: false,
            analysis_mode: false,
            contempt_factor: 0,      // Neutral contempt
        }
    }
}

impl UCIState {
    /// Create new UCI state manager with default configuration
    pub fn new() -> Self {
        let (state_change_tx, _) = broadcast::channel(32);
        
        Self {
            current_state: AtomicU8::new(EngineState::Initializing as u8),
            debug_mode: AtomicBool::new(false),
            searches_started: AtomicU64::new(0),
            searches_completed: AtomicU64::new(0),
            total_nodes_searched: AtomicU64::new(0),
            search_context: RwLock::new(None),
            state_change_tx,
            config: RwLock::new(EngineConfig::default()),
        }
    }

    /// Get current engine state (lock-free atomic read)
    pub fn current_state(&self) -> EngineState {
        let state_value = self.current_state.load(Ordering::Acquire);
        EngineState::from(state_value)
    }

    /// Attempt to transition to a new state with validation
    pub fn transition_to(&self, new_state: EngineState, reason: &str) -> UCIResult<()> {
        let current = self.current_state();
        
        // Validate state transition
        self.validate_transition(current, new_state)?;
        
        // Perform atomic state update
        let old_state = self.current_state.swap(new_state as u8, Ordering::AcqRel);
        let old_state_enum = EngineState::from(old_state);
        
        // Log state change
        info!(
            from = ?old_state_enum,
            to = ?new_state,
            reason = reason,
            "Engine state transition"
        );
        
        // Broadcast state change event (non-blocking)
        let event = StateChangeEvent {
            from: old_state_enum,
            to: new_state,
            timestamp: std::time::Instant::now(),
            reason: reason.to_string(),
        };
        
        // Ignore broadcast errors (no active listeners is OK)
        let _ = self.state_change_tx.send(event);
        
        Ok(())
    }

    /// Validate that a state transition is legal
    fn validate_transition(&self, from: EngineState, to: EngineState) -> UCIResult<()> {
        use EngineState::*;
        
        let valid_transition = match (from, to) {
            // From Initializing
            (Initializing, Ready) | (Initializing, Error) => true,
            
            // From Ready  
            (Ready, Searching) | (Ready, Pondering) | (Ready, Stopping) | (Ready, Error) => true,
            
            // From Searching
            (Searching, Ready) | (Searching, Pondering) | (Searching, Stopping) | (Searching, Error) => true,
            
            // From Pondering
            (Pondering, Ready) | (Pondering, Searching) | (Pondering, Stopping) | (Pondering, Error) => true,
            
            // From Error (can only reset to Initializing or stop)
            (Error, Initializing) | (Error, Stopping) => true,
            
            // From Stopping (terminal state)
            (Stopping, _) => false,
            
            // Same state (no-op, always allowed)
            (state1, state2) if state1 == state2 => true,
            
            // All other transitions are invalid
            _ => false,
        };
        
        if !valid_transition {
            return Err(UCIError::Engine {
                message: format!(
                    "Invalid state transition from {:?} to {:?}",
                    from, to
                ),
            });
        }
        
        Ok(())
    }

    /// Start a new search with the given context
    pub fn start_search(&self, context: SearchContext) -> UCIResult<()> {
        // Transition to searching state
        self.transition_to(EngineState::Searching, "Starting new search")?;
        
        // Update search context
        {
            let mut search_ctx = self.search_context.write();
            *search_ctx = Some(context);
        }
        
        // Increment search counter
        self.searches_started.fetch_add(1, Ordering::Relaxed);
        
        debug!("Search started with new context");
        Ok(())
    }

    /// Complete the current search and return to ready state
    pub fn complete_search(&self, nodes_searched: u64) -> UCIResult<()> {
        // Update statistics
        self.searches_completed.fetch_add(1, Ordering::Relaxed);
        self.total_nodes_searched.fetch_add(nodes_searched, Ordering::Relaxed);
        
        // Clear search context
        {
            let mut search_ctx = self.search_context.write();
            *search_ctx = None;
        }
        
        // Transition back to ready
        self.transition_to(EngineState::Ready, "Search completed")?;
        
        debug!(nodes_searched, "Search completed successfully");
        Ok(())
    }

    /// Enable or disable debug mode
    pub fn set_debug_mode(&self, enabled: bool) {
        let old_value = self.debug_mode.swap(enabled, Ordering::Relaxed);
        if old_value != enabled {
            info!(enabled, "Debug mode changed");
        }
    }

    /// Check if debug mode is enabled
    pub fn is_debug_mode(&self) -> bool {
        self.debug_mode.load(Ordering::Relaxed)
    }

    /// Get current search context (if searching)
    pub fn search_context(&self) -> Option<SearchContext> {
        self.search_context.read().clone()
    }

    /// Get engine statistics
    pub fn statistics(&self) -> EngineStatistics {
        EngineStatistics {
            current_state: self.current_state(),
            searches_started: self.searches_started.load(Ordering::Relaxed),
            searches_completed: self.searches_completed.load(Ordering::Relaxed),
            total_nodes_searched: self.total_nodes_searched.load(Ordering::Relaxed),
            debug_mode: self.is_debug_mode(),
        }
    }

    /// Get engine configuration
    pub fn config(&self) -> EngineConfig {
        self.config.read().clone()
    }

    /// Update engine configuration
    pub fn update_config<F>(&self, updater: F) -> UCIResult<()>
    where
        F: FnOnce(&mut EngineConfig),
    {
        let mut config = self.config.write();
        updater(&mut *config);
        
        debug!(config = ?*config, "Engine configuration updated");
        Ok(())
    }

    /// Subscribe to state change events
    pub fn subscribe_state_changes(&self) -> broadcast::Receiver<StateChangeEvent> {
        self.state_change_tx.subscribe()
    }

    /// Force engine into error state with reason
    pub fn set_error_state(&self, reason: &str) {
        warn!(reason, "Engine forced into error state");
        
        // Force transition to error state (this should always succeed)
        if let Err(e) = self.transition_to(EngineState::Error, reason) {
            // If even error transition fails, something is very wrong
            warn!(error = ?e, "Failed to transition to error state");
        }
    }

    /// Reset engine to clean ready state (recovery mechanism)
    pub fn reset(&self) -> UCIResult<()> {
        info!("Resetting engine state");
        
        // Clear search context
        {
            let mut search_ctx = self.search_context.write();
            *search_ctx = None;
        }
        
        // If in error state, we can go through initializing
        let current = self.current_state();
        if current == EngineState::Error {
            self.transition_to(EngineState::Initializing, "Recovering from error state")?;
            self.transition_to(EngineState::Ready, "Reset complete")?;
        } else {
            // For all other states, just ensure we're ready
            if current != EngineState::Ready {
                self.transition_to(EngineState::Ready, "Reset to ready state")?;
            }
        }
        
        Ok(())
    }
}

impl Default for UCIState {
    fn default() -> Self {
        Self::new()
    }
}

/// Engine statistics snapshot
#[derive(Debug, Clone)]
pub struct EngineStatistics {
    pub current_state: EngineState,
    pub searches_started: u64,
    pub searches_completed: u64,
    pub total_nodes_searched: u64,
    pub debug_mode: bool,
}

// Thread safety: UCIState is designed to be Send + Sync
// All fields are either atomic or use parking_lot which provides Send + Sync
// This is safe because:
// - Atomic types are inherently Send + Sync
// - parking_lot::RwLock<T> is Send + Sync when T is Send + Sync  
// - broadcast::Sender is Send + Sync
// The compiler should automatically derive these, but we're being explicit for clarity.

#[cfg(test)]
mod tests {
    use super::*;
    use tokio::time::{sleep, Duration};

    #[tokio::test]
    async fn test_initial_state() {
        let state = UCIState::new();
        assert_eq!(state.current_state(), EngineState::Initializing);
        assert!(!state.is_debug_mode());
    }

    #[tokio::test]
    async fn test_valid_state_transitions() {
        let state = UCIState::new();
        
        // Initializing -> Ready
        state.transition_to(EngineState::Ready, "Engine initialized").unwrap();
        assert_eq!(state.current_state(), EngineState::Ready);
        
        // Ready -> Searching
        state.transition_to(EngineState::Searching, "Starting search").unwrap();
        assert_eq!(state.current_state(), EngineState::Searching);
        
        // Searching -> Ready
        state.transition_to(EngineState::Ready, "Search complete").unwrap();
        assert_eq!(state.current_state(), EngineState::Ready);
    }

    #[tokio::test]
    async fn test_invalid_state_transitions() {
        let state = UCIState::new();
        
        // Can't go from Initializing directly to Searching
        let result = state.transition_to(EngineState::Searching, "Invalid transition");
        assert!(result.is_err());
        assert_eq!(state.current_state(), EngineState::Initializing);
    }

    #[tokio::test]
    async fn test_search_lifecycle() {
        let state = UCIState::new();
        state.transition_to(EngineState::Ready, "Ready").unwrap();
        
        let context = SearchContext {
            start_time: std::time::Instant::now(),
            time_control: TimeControl::default(),
            max_depth: Some(10),
            max_nodes: None,
            is_infinite: false,
            is_ponder: false,
        };
        
        // Start search
        state.start_search(context).unwrap();
        assert_eq!(state.current_state(), EngineState::Searching);
        assert!(state.search_context().is_some());
        
        // Complete search
        state.complete_search(1000).unwrap();
        assert_eq!(state.current_state(), EngineState::Ready);
        assert!(state.search_context().is_none());
        
        let stats = state.statistics();
        assert_eq!(stats.searches_started, 1);
        assert_eq!(stats.searches_completed, 1);
        assert_eq!(stats.total_nodes_searched, 1000);
    }

    #[tokio::test]
    async fn test_debug_mode() {
        let state = UCIState::new();
        assert!(!state.is_debug_mode());
        
        state.set_debug_mode(true);
        assert!(state.is_debug_mode());
        
        state.set_debug_mode(false);
        assert!(!state.is_debug_mode());
    }

    #[tokio::test]
    async fn test_configuration_updates() {
        let state = UCIState::new();
        
        // Check default config
        let config = state.config();
        assert_eq!(config.hash_size_mb, 16);
        assert_eq!(config.thread_count, 1);
        
        // Update config
        state.update_config(|cfg| {
            cfg.hash_size_mb = 64;
            cfg.thread_count = 4;
        }).unwrap();
        
        let updated_config = state.config();
        assert_eq!(updated_config.hash_size_mb, 64);
        assert_eq!(updated_config.thread_count, 4);
    }

    #[tokio::test]
    async fn test_state_change_notifications() {
        let state = UCIState::new();
        let mut receiver = state.subscribe_state_changes();
        
        // Perform state transition
        state.transition_to(EngineState::Ready, "Test transition").unwrap();
        
        // Receive notification
        let event = receiver.recv().await.unwrap();
        assert_eq!(event.from, EngineState::Initializing);
        assert_eq!(event.to, EngineState::Ready);
        assert_eq!(event.reason, "Test transition");
    }

    #[tokio::test]
    async fn test_concurrent_state_access() {
        use std::sync::Arc;
        
        let state = Arc::new(UCIState::new());
        state.transition_to(EngineState::Ready, "Ready for concurrent test").unwrap();
        
        let mut handles = Vec::new();
        
        // Spawn multiple tasks that read state concurrently
        for i in 0..10 {
            let state_clone = Arc::clone(&state);
            let handle = tokio::spawn(async move {
                for _ in 0..100 {
                    let current = state_clone.current_state();
                    assert!(matches!(current, EngineState::Ready | EngineState::Searching));
                    
                    let stats = state_clone.statistics();
                    assert!(stats.searches_started >= 0);
                    
                    // Small delay to allow interleaving
                    sleep(Duration::from_micros(1)).await;
                }
                i
            });
            handles.push(handle);
        }
        
        // Wait for all tasks to complete
        for handle in handles {
            handle.await.unwrap();
        }
    }

    #[tokio::test]
    async fn test_error_state_recovery() {
        let state = UCIState::new();
        state.transition_to(EngineState::Ready, "Ready").unwrap();
        
        // Force error state
        state.set_error_state("Test error condition");
        assert_eq!(state.current_state(), EngineState::Error);
        
        // Reset should recover
        state.reset().unwrap();
        assert_eq!(state.current_state(), EngineState::Ready);
    }

    #[test]
    fn test_engine_state_properties() {
        assert!(EngineState::Ready.can_accept_commands());
        assert!(EngineState::Pondering.can_accept_commands());
        assert!(!EngineState::Searching.can_accept_commands());
        
        assert!(EngineState::Searching.is_computing());
        assert!(EngineState::Pondering.is_computing());
        assert!(!EngineState::Ready.is_computing());
        
        assert!(EngineState::Stopping.is_terminal());
        assert!(EngineState::Error.is_terminal());
        assert!(!EngineState::Ready.is_terminal());
    }
}