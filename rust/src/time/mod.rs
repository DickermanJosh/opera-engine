//! Time management system for UCI search control
//!
//! This module provides flexible time allocation and management for chess engine search.
//! It implements various time control policies and provides utilities for calculating
//! optimal search time based on UCI time parameters.

use std::fmt::Debug;
use std::time::Duration;

pub mod policies;
pub mod timer;

pub use policies::{StandardTimePolicy, FixedTimePolicy, InfiniteTimePolicy};
pub use timer::{SearchTimer, timer_from_millis};

/// Parameters for time-based search control from UCI "go" command
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct SearchParams {
    /// Fixed time per move in milliseconds
    pub movetime: Option<u64>,

    /// White's remaining time in milliseconds
    pub wtime: Option<u64>,

    /// Black's remaining time in milliseconds
    pub btime: Option<u64>,

    /// White's increment per move in milliseconds
    pub winc: Option<u64>,

    /// Black's increment per move in milliseconds
    pub binc: Option<u64>,

    /// Number of moves until next time control
    pub movestogo: Option<u32>,

    /// Search indefinitely (until "stop" command)
    pub infinite: bool,

    /// Maximum search depth
    pub depth: Option<u32>,

    /// Maximum nodes to search
    pub nodes: Option<u64>,
}

impl SearchParams {
    /// Create infinite search parameters
    pub fn infinite() -> Self {
        Self {
            infinite: true,
            ..Default::default()
        }
    }

    /// Create fixed time per move parameters
    pub fn movetime(ms: u64) -> Self {
        Self {
            movetime: Some(ms),
            ..Default::default()
        }
    }

    /// Create depth-limited search parameters
    pub fn depth(depth: u32) -> Self {
        Self {
            depth: Some(depth),
            ..Default::default()
        }
    }

    /// Create node-limited search parameters
    pub fn nodes(nodes: u64) -> Self {
        Self {
            nodes: Some(nodes),
            ..Default::default()
        }
    }

    /// Check if this is an infinite search
    pub fn is_infinite(&self) -> bool {
        self.infinite
    }

    /// Check if this is a fixed-time search
    pub fn is_movetime(&self) -> bool {
        self.movetime.is_some()
    }

    /// Check if this has any time controls
    pub fn has_time_control(&self) -> bool {
        self.wtime.is_some() || self.btime.is_some() || self.movetime.is_some()
    }
}

/// Information about the current position for time management decisions
#[derive(Debug, Clone, Default)]
pub struct PositionInfo {
    /// Number of legal moves in the position
    pub legal_moves: usize,

    /// Whether we're in the opening phase
    pub is_opening: bool,

    /// Whether we're in the endgame phase
    pub is_endgame: bool,

    /// Move number (full moves since start)
    pub move_number: u32,
}

/// Progress information during search for early stopping decisions
#[derive(Debug, Clone, Default)]
pub struct SearchProgress {
    /// Current search depth completed
    pub depth: u32,

    /// Total nodes searched so far
    pub nodes: u64,

    /// Best score found so far (centipawns)
    pub score: i32,

    /// Whether the best move has been stable across depths
    pub best_move_stable: bool,

    /// Number of consecutive depths with same best move
    pub stability_count: u32,
}

/// Time limits for a search
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TimeLimits {
    /// Soft time limit - aim to finish by this time
    pub soft_limit: Duration,

    /// Hard time limit - must stop by this time
    pub hard_limit: Duration,
}

impl TimeLimits {
    /// Create new time limits
    pub fn new(soft_limit: Duration, hard_limit: Duration) -> Self {
        Self {
            soft_limit,
            hard_limit,
        }
    }

    /// Create time limits from milliseconds
    pub fn from_millis(soft_ms: u64, hard_ms: u64) -> Self {
        Self {
            soft_limit: Duration::from_millis(soft_ms),
            hard_limit: Duration::from_millis(hard_ms),
        }
    }

    /// Create infinite time limits (for infinite search)
    pub fn infinite() -> Self {
        Self {
            soft_limit: Duration::from_secs(u64::MAX / 1000),
            hard_limit: Duration::from_secs(u64::MAX / 1000),
        }
    }

    /// Check if these are infinite time limits
    pub fn is_infinite(&self) -> bool {
        self.soft_limit.as_secs() > 86400 // More than 1 day = infinite
    }

    /// Get soft limit in milliseconds
    pub fn soft_limit_ms(&self) -> u64 {
        self.soft_limit.as_millis() as u64
    }

    /// Get hard limit in milliseconds
    pub fn hard_limit_ms(&self) -> u64 {
        self.hard_limit.as_millis() as u64
    }
}

impl Default for TimeLimits {
    fn default() -> Self {
        Self::infinite()
    }
}

/// Trait for implementing different time management policies
///
/// This trait allows for flexible time allocation strategies based on
/// the position characteristics and search parameters.
pub trait TimePolicy: Debug + Send + Sync {
    /// Calculate time limits for a search
    ///
    /// # Arguments
    /// * `params` - UCI search parameters (time controls, increments, etc.)
    /// * `position_info` - Information about the current position
    ///
    /// # Returns
    /// Time limits (soft and hard) for the search
    fn calculate_time_limit(&self, params: &SearchParams, position_info: &PositionInfo) -> TimeLimits;

    /// Determine if search should stop early based on progress
    ///
    /// This allows policies to implement early stopping when the best move
    /// is stable and unlikely to change with deeper search.
    ///
    /// # Arguments
    /// * `elapsed` - Time elapsed since search start
    /// * `progress` - Current search progress information
    ///
    /// # Returns
    /// `true` if search should stop early, `false` otherwise
    fn should_stop_early(&self, elapsed: Duration, progress: &SearchProgress) -> bool;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_search_params_infinite() {
        let params = SearchParams::infinite();
        assert!(params.is_infinite());
        assert!(!params.is_movetime());
        assert!(!params.has_time_control());
    }

    #[test]
    fn test_search_params_movetime() {
        let params = SearchParams::movetime(5000);
        assert!(!params.is_infinite());
        assert!(params.is_movetime());
        assert!(params.has_time_control());
        assert_eq!(params.movetime, Some(5000));
    }

    #[test]
    fn test_search_params_depth() {
        let params = SearchParams::depth(10);
        assert!(!params.is_infinite());
        assert!(!params.is_movetime());
        assert_eq!(params.depth, Some(10));
    }

    #[test]
    fn test_search_params_nodes() {
        let params = SearchParams::nodes(1_000_000);
        assert_eq!(params.nodes, Some(1_000_000));
    }

    #[test]
    fn test_search_params_time_control() {
        let params = SearchParams {
            wtime: Some(60000),
            btime: Some(60000),
            winc: Some(1000),
            binc: Some(1000),
            ..Default::default()
        };
        assert!(params.has_time_control());
        assert!(!params.is_infinite());
    }

    #[test]
    fn test_time_limits_new() {
        let limits = TimeLimits::new(
            Duration::from_millis(1000),
            Duration::from_millis(2000),
        );
        assert_eq!(limits.soft_limit_ms(), 1000);
        assert_eq!(limits.hard_limit_ms(), 2000);
        assert!(!limits.is_infinite());
    }

    #[test]
    fn test_time_limits_from_millis() {
        let limits = TimeLimits::from_millis(500, 1000);
        assert_eq!(limits.soft_limit_ms(), 500);
        assert_eq!(limits.hard_limit_ms(), 1000);
    }

    #[test]
    fn test_time_limits_infinite() {
        let limits = TimeLimits::infinite();
        assert!(limits.is_infinite());
        assert!(limits.soft_limit_ms() > 86400000); // More than 1 day
    }

    #[test]
    fn test_position_info_default() {
        let info = PositionInfo::default();
        assert_eq!(info.legal_moves, 0);
        assert!(!info.is_opening);
        assert!(!info.is_endgame);
        assert_eq!(info.move_number, 0);
    }

    #[test]
    fn test_search_progress_default() {
        let progress = SearchProgress::default();
        assert_eq!(progress.depth, 0);
        assert_eq!(progress.nodes, 0);
        assert_eq!(progress.score, 0);
        assert!(!progress.best_move_stable);
        assert_eq!(progress.stability_count, 0);
    }
}
