//! Concrete implementations of time management policies
//!
//! This module provides several standard time management algorithms
//! used in chess engines for calculating optimal search time allocation.

use super::{PositionInfo, SearchParams, SearchProgress, TimePolicy, TimeLimits};
use std::time::Duration;

/// Standard time management policy using classical time allocation algorithm
///
/// This policy allocates time based on:
/// - Fixed movetime if specified
/// - Remaining time divided by estimated moves remaining
/// - Time increment consideration
/// - Safety margin to avoid time losses
/// - Early stopping when best move is stable
#[derive(Debug, Clone)]
pub struct StandardTimePolicy {
    /// Safety margin in milliseconds to avoid time forfeits
    safety_margin_ms: u64,

    /// Factor for soft time limit (typically 0.3-0.5)
    time_factor: f64,

    /// Maximum factor for hard time limit (typically 2-5)
    max_time_factor: f64,

    /// Minimum stability count for early stopping
    min_stability_for_early_stop: u32,
}

impl StandardTimePolicy {
    /// Create a new standard time policy
    ///
    /// # Arguments
    /// * `safety_margin_ms` - Safety margin in milliseconds (typically 50-100ms)
    /// * `time_factor` - Soft limit factor (typically 0.3-0.5)
    pub fn new(safety_margin_ms: u64, time_factor: f64) -> Self {
        Self {
            safety_margin_ms,
            time_factor,
            max_time_factor: 3.0,
            min_stability_for_early_stop: 3,
        }
    }

    /// Create default standard policy
    pub fn default_policy() -> Self {
        Self::new(50, 0.35)
    }

    /// Estimate moves remaining in the game
    fn estimate_moves_remaining(&self, position_info: &PositionInfo) -> u32 {
        if position_info.is_endgame {
            15
        } else if position_info.is_opening {
            40
        } else {
            30 // Middlegame
        }
    }
}

impl TimePolicy for StandardTimePolicy {
    fn calculate_time_limit(&self, params: &SearchParams, position_info: &PositionInfo) -> TimeLimits {
        // Handle infinite search
        if params.is_infinite() {
            return TimeLimits::infinite();
        }

        // Handle fixed movetime
        if let Some(movetime) = params.movetime {
            let safe_time = movetime.saturating_sub(self.safety_margin_ms);
            return TimeLimits {
                soft_limit: Duration::from_millis(safe_time),
                hard_limit: Duration::from_millis(movetime),
            };
        }

        // Handle time controls with remaining time
        if let (Some(our_time), our_inc) = (params.wtime.or(params.btime), params.winc.or(params.binc)) {
            // Ensure we have enough time for safety margin
            if our_time <= self.safety_margin_ms {
                // Emergency: use minimal time
                return TimeLimits {
                    soft_limit: Duration::from_millis(10),
                    hard_limit: Duration::from_millis(our_time.saturating_sub(10)),
                };
            }

            let available_time = our_time.saturating_sub(self.safety_margin_ms);
            let increment = our_inc.unwrap_or(0);

            // Calculate base time allocation
            let moves_remaining = params.movestogo.unwrap_or_else(|| {
                self.estimate_moves_remaining(position_info)
            });

            // Base allocation: (remaining_time / moves_remaining) + increment
            let base_time = (available_time / moves_remaining as u64).saturating_add(increment);

            // Soft limit: use time_factor of base time
            let soft_ms = ((base_time as f64) * self.time_factor) as u64;

            // Hard limit: allow using more time if needed (up to max_time_factor)
            let hard_ms = ((base_time as f64) * self.max_time_factor).min(available_time as f64) as u64;

            return TimeLimits {
                soft_limit: Duration::from_millis(soft_ms),
                hard_limit: Duration::from_millis(hard_ms),
            };
        }

        // No time controls - infinite search
        TimeLimits::infinite()
    }

    fn should_stop_early(&self, elapsed: Duration, progress: &SearchProgress) -> bool {
        // Don't stop early if we're in minimal depth
        if progress.depth < 6 {
            return false;
        }

        // Stop early if best move has been very stable
        if progress.best_move_stable && progress.stability_count >= self.min_stability_for_early_stop {
            // Only stop early if we've used at least some time
            return elapsed.as_millis() > 100;
        }

        false
    }
}

/// Fixed time policy - always use exact specified time
///
/// This policy is useful for:
/// - Time per move controls (movetime)
/// - Testing and benchmarking
/// - Fixed-depth searches with time limit
#[derive(Debug, Clone)]
pub struct FixedTimePolicy {
    /// Fixed time limit in milliseconds
    time_ms: u64,

    /// Safety margin in milliseconds
    safety_margin_ms: u64,
}

impl FixedTimePolicy {
    /// Create a new fixed time policy
    pub fn new(time_ms: u64, safety_margin_ms: u64) -> Self {
        Self {
            time_ms,
            safety_margin_ms,
        }
    }

    /// Create from milliseconds with default safety margin
    pub fn from_millis(time_ms: u64) -> Self {
        Self::new(time_ms, 50)
    }
}

impl TimePolicy for FixedTimePolicy {
    fn calculate_time_limit(&self, _params: &SearchParams, _position_info: &PositionInfo) -> TimeLimits {
        let safe_time = self.time_ms.saturating_sub(self.safety_margin_ms);
        TimeLimits {
            soft_limit: Duration::from_millis(safe_time),
            hard_limit: Duration::from_millis(self.time_ms),
        }
    }

    fn should_stop_early(&self, _elapsed: Duration, _progress: &SearchProgress) -> bool {
        // Fixed time policy doesn't stop early
        false
    }
}

/// Infinite time policy - no time limits
///
/// Used for:
/// - Infinite analysis mode
/// - Depth-limited searches
/// - Node-limited searches
#[derive(Debug, Clone)]
pub struct InfiniteTimePolicy;

impl InfiniteTimePolicy {
    /// Create a new infinite time policy
    pub fn new() -> Self {
        Self
    }
}

impl Default for InfiniteTimePolicy {
    fn default() -> Self {
        Self::new()
    }
}

impl TimePolicy for InfiniteTimePolicy {
    fn calculate_time_limit(&self, _params: &SearchParams, _position_info: &PositionInfo) -> TimeLimits {
        TimeLimits::infinite()
    }

    fn should_stop_early(&self, _elapsed: Duration, _progress: &SearchProgress) -> bool {
        // Never stop early in infinite mode
        false
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // StandardTimePolicy tests

    #[test]
    fn test_standard_policy_infinite_search() {
        let policy = StandardTimePolicy::default_policy();
        let params = SearchParams::infinite();
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);
        assert!(limits.is_infinite());
    }

    #[test]
    fn test_standard_policy_movetime() {
        let policy = StandardTimePolicy::new(50, 0.35);
        let params = SearchParams::movetime(5000);
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);
        assert_eq!(limits.soft_limit_ms(), 4950); // 5000 - 50
        assert_eq!(limits.hard_limit_ms(), 5000);
    }

    #[test]
    fn test_standard_policy_time_control() {
        let policy = StandardTimePolicy::new(50, 0.35);
        let params = SearchParams {
            wtime: Some(60000),
            winc: Some(1000),
            ..Default::default()
        };
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);

        // With 60s remaining, 30 moves estimated, 1s increment:
        // base_time = (60000 - 50) / 30 + 1000 = 1998 + 1000 = 2998
        // soft = 2998 * 0.35 = 1049
        assert_eq!(limits.soft_limit_ms(), 1049);

        // hard = 2998 * 3.0 = 8994
        assert_eq!(limits.hard_limit_ms(), 8994);
    }

    #[test]
    fn test_standard_policy_with_movestogo() {
        let policy = StandardTimePolicy::new(50, 0.35);
        let params = SearchParams {
            wtime: Some(60000),
            winc: Some(1000),
            movestogo: Some(20),
            ..Default::default()
        };
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);

        // With 60s remaining, 20 moves to go, 1s increment:
        // base_time = (60000 - 50) / 20 + 1000 = 2997 + 1000 = 3997
        // soft = 3997 * 0.35 = 1398
        assert_eq!(limits.soft_limit_ms(), 1398);

        // hard = 3997 * 3.0 = 11991
        assert_eq!(limits.hard_limit_ms(), 11991);
    }

    #[test]
    fn test_standard_policy_emergency_time() {
        let policy = StandardTimePolicy::new(50, 0.35);
        let params = SearchParams {
            wtime: Some(40), // Very low time
            winc: Some(0),
            ..Default::default()
        };
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);

        // Emergency mode: minimal time allocation
        assert_eq!(limits.soft_limit_ms(), 10);
        assert_eq!(limits.hard_limit_ms(), 30); // 40 - 10
    }

    #[test]
    fn test_standard_policy_endgame_adjustment() {
        let policy = StandardTimePolicy::new(50, 0.35);
        let params = SearchParams {
            wtime: Some(60000),
            winc: Some(1000),
            ..Default::default()
        };
        let position_info = PositionInfo {
            is_endgame: true,
            ..Default::default()
        };

        let limits = policy.calculate_time_limit(&params, &position_info);

        // Endgame: 15 moves estimated
        // base_time = (60000 - 50) / 15 + 1000 = 3996 + 1000 = 4996
        // soft = 4996 * 0.35 = 1748
        assert_eq!(limits.soft_limit_ms(), 1748);
    }

    #[test]
    fn test_standard_policy_opening_adjustment() {
        let policy = StandardTimePolicy::new(50, 0.35);
        let params = SearchParams {
            wtime: Some(60000),
            winc: Some(1000),
            ..Default::default()
        };
        let position_info = PositionInfo {
            is_opening: true,
            ..Default::default()
        };

        let limits = policy.calculate_time_limit(&params, &position_info);

        // Opening: 40 moves estimated
        // base_time = (60000 - 50) / 40 + 1000 = 1498 + 1000 = 2498
        // soft = 2498 * 0.35 = 874
        assert_eq!(limits.soft_limit_ms(), 874);
    }

    #[test]
    fn test_standard_policy_no_time_control() {
        let policy = StandardTimePolicy::default_policy();
        let params = SearchParams::default();
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);
        assert!(limits.is_infinite());
    }

    #[test]
    fn test_standard_policy_early_stop_shallow_depth() {
        let policy = StandardTimePolicy::default_policy();
        let progress = SearchProgress {
            depth: 4,
            best_move_stable: true,
            stability_count: 5,
            ..Default::default()
        };

        // Should not stop early at shallow depth
        assert!(!policy.should_stop_early(Duration::from_millis(500), &progress));
    }

    #[test]
    fn test_standard_policy_early_stop_unstable() {
        let policy = StandardTimePolicy::default_policy();
        let progress = SearchProgress {
            depth: 10,
            best_move_stable: false,
            stability_count: 0,
            ..Default::default()
        };

        // Should not stop early if move is unstable
        assert!(!policy.should_stop_early(Duration::from_millis(500), &progress));
    }

    #[test]
    fn test_standard_policy_early_stop_stable() {
        let policy = StandardTimePolicy::default_policy();
        let progress = SearchProgress {
            depth: 10,
            best_move_stable: true,
            stability_count: 5,
            ..Default::default()
        };

        // Should stop early with stable move at good depth
        assert!(policy.should_stop_early(Duration::from_millis(500), &progress));
    }

    #[test]
    fn test_standard_policy_early_stop_minimal_time() {
        let policy = StandardTimePolicy::default_policy();
        let progress = SearchProgress {
            depth: 10,
            best_move_stable: true,
            stability_count: 5,
            ..Default::default()
        };

        // Should not stop if very little time has elapsed
        assert!(!policy.should_stop_early(Duration::from_millis(50), &progress));
    }

    // FixedTimePolicy tests

    #[test]
    fn test_fixed_policy_calculation() {
        let policy = FixedTimePolicy::new(5000, 50);
        let params = SearchParams::default();
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);
        assert_eq!(limits.soft_limit_ms(), 4950);
        assert_eq!(limits.hard_limit_ms(), 5000);
    }

    #[test]
    fn test_fixed_policy_from_millis() {
        let policy = FixedTimePolicy::from_millis(3000);
        let params = SearchParams::default();
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);
        assert_eq!(limits.soft_limit_ms(), 2950); // 3000 - 50
        assert_eq!(limits.hard_limit_ms(), 3000);
    }

    #[test]
    fn test_fixed_policy_no_early_stop() {
        let policy = FixedTimePolicy::from_millis(5000);
        let progress = SearchProgress {
            depth: 10,
            best_move_stable: true,
            stability_count: 10,
            ..Default::default()
        };

        // Fixed policy never stops early
        assert!(!policy.should_stop_early(Duration::from_millis(100), &progress));
    }

    // InfiniteTimePolicy tests

    #[test]
    fn test_infinite_policy_always_infinite() {
        let policy = InfiniteTimePolicy::new();
        let params = SearchParams::movetime(5000); // Even with movetime
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);
        assert!(limits.is_infinite());
    }

    #[test]
    fn test_infinite_policy_no_early_stop() {
        let policy = InfiniteTimePolicy::new();
        let progress = SearchProgress {
            depth: 10,
            best_move_stable: true,
            stability_count: 10,
            ..Default::default()
        };

        // Infinite policy never stops early
        assert!(!policy.should_stop_early(Duration::from_millis(1000), &progress));
    }

    #[test]
    fn test_infinite_policy_default() {
        let policy = InfiniteTimePolicy::default();
        let params = SearchParams::default();
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);
        assert!(limits.is_infinite());
    }

    // Edge case tests

    #[test]
    fn test_standard_policy_zero_time() {
        let policy = StandardTimePolicy::new(50, 0.35);
        let params = SearchParams {
            wtime: Some(0),
            ..Default::default()
        };
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);

        // Should handle zero time gracefully
        assert_eq!(limits.soft_limit_ms(), 10);
        assert_eq!(limits.hard_limit_ms(), 0); // 0 - 10 saturating
    }

    #[test]
    fn test_standard_policy_large_increment() {
        let policy = StandardTimePolicy::new(50, 0.35);
        let params = SearchParams {
            wtime: Some(10000),
            winc: Some(10000), // Larger than remaining time
            ..Default::default()
        };
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);

        // Should handle large increments properly
        // base_time = (10000 - 50) / 30 + 10000 = 331 + 10000 = 10331
        assert_eq!(limits.soft_limit_ms(), 3615); // 10331 * 0.35
    }

    #[test]
    fn test_fixed_policy_zero_safety_margin() {
        let policy = FixedTimePolicy::new(1000, 0);
        let params = SearchParams::default();
        let position_info = PositionInfo::default();

        let limits = policy.calculate_time_limit(&params, &position_info);
        assert_eq!(limits.soft_limit_ms(), 1000);
        assert_eq!(limits.hard_limit_ms(), 1000);
    }
}
