//! Timer utilities for precise time management with tokio
//!
//! This module provides async-compatible timer utilities for tracking
//! search time and enforcing time limits using tokio::time.

use std::time::{Duration, Instant};
use tokio::time::{sleep, timeout};

/// Async timer for tracking elapsed time and enforcing limits
///
/// This timer integrates with tokio for precise, async-compatible timing
/// that doesn't block the event loop.
#[derive(Debug)]
pub struct SearchTimer {
    start: Instant,
    soft_limit: Duration,
    hard_limit: Duration,
}

impl SearchTimer {
    /// Create a new search timer with specified limits
    pub fn new(soft_limit: Duration, hard_limit: Duration) -> Self {
        Self {
            start: Instant::now(),
            soft_limit,
            hard_limit,
        }
    }

    /// Get elapsed time since timer start
    pub fn elapsed(&self) -> Duration {
        self.start.elapsed()
    }

    /// Get elapsed time in milliseconds
    pub fn elapsed_ms(&self) -> u64 {
        self.elapsed().as_millis() as u64
    }

    /// Check if soft limit has been exceeded
    pub fn soft_limit_exceeded(&self) -> bool {
        self.elapsed() >= self.soft_limit
    }

    /// Check if hard limit has been exceeded
    pub fn hard_limit_exceeded(&self) -> bool {
        self.elapsed() >= self.hard_limit
    }

    /// Get remaining time until soft limit
    pub fn soft_limit_remaining(&self) -> Duration {
        self.soft_limit.saturating_sub(self.elapsed())
    }

    /// Get remaining time until hard limit
    pub fn hard_limit_remaining(&self) -> Duration {
        self.hard_limit.saturating_sub(self.elapsed())
    }

    /// Get soft limit in milliseconds
    pub fn soft_limit_ms(&self) -> u64 {
        self.soft_limit.as_millis() as u64
    }

    /// Get hard limit in milliseconds
    pub fn hard_limit_ms(&self) -> u64 {
        self.hard_limit.as_millis() as u64
    }

    /// Wait until soft limit is reached (async)
    ///
    /// This function sleeps until the soft limit time has elapsed.
    /// Useful for implementing time-based search termination.
    pub async fn wait_until_soft_limit(&self) {
        let remaining = self.soft_limit_remaining();
        if remaining > Duration::from_millis(0) {
            sleep(remaining).await;
        }
    }

    /// Wait until hard limit is reached (async)
    ///
    /// This function sleeps until the hard limit time has elapsed.
    pub async fn wait_until_hard_limit(&self) {
        let remaining = self.hard_limit_remaining();
        if remaining > Duration::from_millis(0) {
            sleep(remaining).await;
        }
    }

    /// Run a future with hard time limit enforcement
    ///
    /// This wraps a future with a timeout at the hard limit.
    /// If the future doesn't complete before the hard limit,
    /// it will be cancelled and return an error.
    pub async fn run_with_timeout<F, T>(&self, future: F) -> Result<T, tokio::time::error::Elapsed>
    where
        F: std::future::Future<Output = T>,
    {
        let remaining = self.hard_limit_remaining();
        timeout(remaining, future).await
    }
}

impl Default for SearchTimer {
    fn default() -> Self {
        // Default to infinite time limits
        Self::new(Duration::from_secs(u64::MAX / 1000), Duration::from_secs(u64::MAX / 1000))
    }
}

/// Helper function to create a timer from milliseconds
pub fn timer_from_millis(soft_ms: u64, hard_ms: u64) -> SearchTimer {
    SearchTimer::new(Duration::from_millis(soft_ms), Duration::from_millis(hard_ms))
}

#[cfg(test)]
mod tests {
    use super::*;
    use tokio::time::sleep;

    #[test]
    fn test_timer_creation() {
        let timer = SearchTimer::new(Duration::from_millis(1000), Duration::from_millis(2000));

        assert_eq!(timer.soft_limit_ms(), 1000);
        assert_eq!(timer.hard_limit_ms(), 2000);
        assert!(!timer.soft_limit_exceeded());
        assert!(!timer.hard_limit_exceeded());
    }

    #[test]
    fn test_timer_from_millis() {
        let timer = timer_from_millis(500, 1000);

        assert_eq!(timer.soft_limit_ms(), 500);
        assert_eq!(timer.hard_limit_ms(), 1000);
    }

    #[test]
    fn test_timer_elapsed() {
        let timer = SearchTimer::new(Duration::from_millis(100), Duration::from_millis(200));

        // Immediately after creation, elapsed should be minimal
        assert!(timer.elapsed_ms() < 10);
    }

    #[test]
    fn test_timer_default() {
        let timer = SearchTimer::default();

        // Default should have very long limits
        assert!(timer.soft_limit_ms() > 86400000); // More than 1 day
        assert!(timer.hard_limit_ms() > 86400000);
    }

    #[tokio::test]
    async fn test_timer_soft_limit_exceeded() {
        let timer = SearchTimer::new(Duration::from_millis(50), Duration::from_millis(100));

        assert!(!timer.soft_limit_exceeded());

        sleep(Duration::from_millis(60)).await;

        assert!(timer.soft_limit_exceeded());
        assert!(!timer.hard_limit_exceeded());
    }

    #[tokio::test]
    async fn test_timer_hard_limit_exceeded() {
        let timer = SearchTimer::new(Duration::from_millis(50), Duration::from_millis(100));

        assert!(!timer.hard_limit_exceeded());

        sleep(Duration::from_millis(110)).await;

        assert!(timer.soft_limit_exceeded());
        assert!(timer.hard_limit_exceeded());
    }

    #[tokio::test]
    async fn test_timer_remaining_time() {
        let timer = SearchTimer::new(Duration::from_millis(100), Duration::from_millis(200));

        sleep(Duration::from_millis(30)).await;

        let soft_remaining = timer.soft_limit_remaining();
        let hard_remaining = timer.hard_limit_remaining();

        // Should have roughly 70ms and 170ms remaining
        assert!(soft_remaining.as_millis() >= 60 && soft_remaining.as_millis() <= 80);
        assert!(hard_remaining.as_millis() >= 160 && hard_remaining.as_millis() <= 180);
    }

    #[tokio::test]
    async fn test_timer_wait_until_soft_limit() {
        let timer = SearchTimer::new(Duration::from_millis(50), Duration::from_millis(100));

        let start = Instant::now();
        timer.wait_until_soft_limit().await;
        let elapsed = start.elapsed();

        // Should wait approximately 50ms
        assert!(elapsed.as_millis() >= 45 && elapsed.as_millis() <= 65);
    }

    #[tokio::test]
    async fn test_timer_wait_until_hard_limit() {
        let timer = SearchTimer::new(Duration::from_millis(50), Duration::from_millis(100));

        let start = Instant::now();
        timer.wait_until_hard_limit().await;
        let elapsed = start.elapsed();

        // Should wait approximately 100ms
        assert!(elapsed.as_millis() >= 95 && elapsed.as_millis() <= 115);
    }

    #[tokio::test]
    async fn test_timer_run_with_timeout_success() {
        let timer = SearchTimer::new(Duration::from_millis(50), Duration::from_millis(200));

        let result = timer
            .run_with_timeout(async {
                sleep(Duration::from_millis(50)).await;
                42
            })
            .await;

        assert_eq!(result.unwrap(), 42);
    }

    #[tokio::test]
    async fn test_timer_run_with_timeout_exceeded() {
        let timer = SearchTimer::new(Duration::from_millis(50), Duration::from_millis(100));

        let result = timer
            .run_with_timeout(async {
                sleep(Duration::from_millis(150)).await;
                42
            })
            .await;

        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_timer_wait_when_already_exceeded() {
        let timer = SearchTimer::new(Duration::from_millis(10), Duration::from_millis(20));

        // Wait for limits to be exceeded
        sleep(Duration::from_millis(30)).await;

        // Waiting should return immediately
        let start = Instant::now();
        timer.wait_until_soft_limit().await;
        let elapsed = start.elapsed();

        assert!(elapsed.as_millis() < 5);
    }

    #[test]
    fn test_timer_remaining_saturating() {
        let timer = SearchTimer::new(Duration::from_millis(10), Duration::from_millis(20));

        // Wait synchronously for longer than limits
        std::thread::sleep(Duration::from_millis(30));

        // Remaining should saturate to zero, not underflow
        let soft_remaining = timer.soft_limit_remaining();
        let hard_remaining = timer.hard_limit_remaining();

        assert_eq!(soft_remaining, Duration::from_millis(0));
        assert_eq!(hard_remaining, Duration::from_millis(0));
    }
}
