// Async runtime configuration for Opera UCI Engine
//
// This module provides tokio runtime configuration and async utilities
// optimized for UCI protocol processing and engine coordination.

use std::time::Duration;
use tokio::runtime::{Builder, Handle, Runtime};
use tokio::sync::{mpsc, oneshot, Mutex};
use tokio::time::{sleep, timeout, Instant};
use futures::future;
use tracing::{debug, info, warn, instrument};
use crate::error::{UCIResult, UCIError};

/// Runtime configuration for the async engine
#[derive(Debug, Clone)]
pub struct RuntimeConfig {
    /// Number of worker threads (None = automatic)
    pub worker_threads: Option<usize>,
    
    /// Stack size for worker threads (None = default)
    pub thread_stack_size: Option<usize>,
    
    /// Thread name prefix
    pub thread_name: String,
    
    /// Enable I/O driver
    pub enable_io: bool,
    
    /// Enable time driver
    pub enable_time: bool,
    
    /// Default timeout for operations (milliseconds)
    pub default_timeout_ms: u64,
    
    /// Maximum number of blocking threads
    pub max_blocking_threads: usize,
    
    /// Keep alive duration for idle threads
    pub thread_keep_alive: Duration,
}

impl Default for RuntimeConfig {
    fn default() -> Self {
        Self {
            worker_threads: Some(1), // Single-threaded for UCI coordination
            thread_stack_size: None, // Use default
            thread_name: "opera-uci".to_string(),
            enable_io: true,
            enable_time: true,
            default_timeout_ms: 30_000, // 30 seconds default timeout
            max_blocking_threads: 4,
            thread_keep_alive: Duration::from_secs(10),
        }
    }
}

impl RuntimeConfig {
    /// Create configuration optimized for UCI protocol processing
    pub fn uci_optimized() -> Self {
        Self {
            worker_threads: Some(1), // Single-threaded for deterministic processing
            thread_stack_size: Some(2 * 1024 * 1024), // 2MB stack for safety
            thread_name: "opera-uci".to_string(),
            enable_io: true,
            enable_time: true,
            default_timeout_ms: 10_000, // 10 seconds for responsive UCI
            max_blocking_threads: 2, // Minimal blocking threads
            thread_keep_alive: Duration::from_secs(5),
        }
    }
    
    /// Create configuration for development and testing
    pub fn development() -> Self {
        Self {
            worker_threads: Some(2), // More threads for debugging
            thread_stack_size: Some(4 * 1024 * 1024), // 4MB stack for debugging
            thread_name: "opera-uci-dev".to_string(),
            enable_io: true,
            enable_time: true,
            default_timeout_ms: 60_000, // 60 seconds for development
            max_blocking_threads: 8, // More blocking threads for development tools
            thread_keep_alive: Duration::from_secs(30),
        }
    }
    
    /// Create configuration for maximum performance
    pub fn performance() -> Self {
        Self {
            worker_threads: None, // Let tokio decide based on CPU cores
            thread_stack_size: None, // Use default for efficiency
            thread_name: "opera-uci-perf".to_string(),
            enable_io: true,
            enable_time: true,
            default_timeout_ms: 5_000, // 5 seconds for performance
            max_blocking_threads: 16, // More blocking threads for performance
            thread_keep_alive: Duration::from_secs(1), // Quick cleanup
        }
    }
}

/// Async runtime manager for the UCI engine
pub struct RuntimeManager {
    runtime: Runtime,
    config: RuntimeConfig,
}

impl RuntimeManager {
    /// Create a new runtime manager with the given configuration
    pub fn new(config: RuntimeConfig) -> UCIResult<Self> {
        info!("Creating async runtime with configuration: {:?}", config);
        
        let mut builder = Builder::new_multi_thread();
        
        // Configure worker threads
        if let Some(worker_threads) = config.worker_threads {
            builder.worker_threads(worker_threads);
        }
        
        // Configure stack size
        if let Some(stack_size) = config.thread_stack_size {
            builder.thread_stack_size(stack_size);
        }
        
        // Configure thread naming
        builder.thread_name(&config.thread_name);
        
        // Configure drivers
        if config.enable_io {
            builder.enable_io();
        }
        if config.enable_time {
            builder.enable_time();
        }
        
        // Configure blocking threads
        builder.max_blocking_threads(config.max_blocking_threads);
        builder.thread_keep_alive(config.thread_keep_alive);
        
        let runtime = builder.build().map_err(|e| UCIError::Internal {
            message: format!("Failed to create tokio runtime: {}", e),
        })?;
        
        info!("Async runtime created successfully");
        
        Ok(Self { runtime, config })
    }
    
    /// Create runtime manager with UCI-optimized configuration
    pub fn uci_optimized() -> UCIResult<Self> {
        Self::new(RuntimeConfig::uci_optimized())
    }
    
    /// Get a handle to the runtime
    pub fn handle(&self) -> Handle {
        self.runtime.handle().clone()
    }
    
    /// Block on an async operation with default timeout
    pub fn block_on<F>(&self, future: F) -> UCIResult<F::Output>
    where
        F: std::future::Future,
    {
        self.block_on_with_timeout(future, self.config.default_timeout_ms)
    }
    
    /// Block on an async operation with custom timeout
    pub fn block_on_with_timeout<F>(
        &self,
        future: F,
        timeout_ms: u64,
    ) -> UCIResult<F::Output>
    where
        F: std::future::Future,
    {
        let timeout_duration = Duration::from_millis(timeout_ms);
        
        self.runtime.block_on(async {
            timeout(timeout_duration, future)
                .await
                .map_err(|_| UCIError::Timeout { duration_ms: timeout_ms })
        })
    }
    
    /// Spawn a task on the runtime
    pub fn spawn<F>(&self, future: F) -> tokio::task::JoinHandle<F::Output>
    where
        F: std::future::Future + Send + 'static,
        F::Output: Send + 'static,
    {
        self.runtime.spawn(future)
    }
    
    /// Spawn a blocking task on the runtime
    pub fn spawn_blocking<F, R>(&self, func: F) -> tokio::task::JoinHandle<R>
    where
        F: FnOnce() -> R + Send + 'static,
        R: Send + 'static,
    {
        self.runtime.spawn_blocking(func)
    }
    
    /// Shutdown the runtime gracefully
    pub fn shutdown(self) {
        info!("Shutting down async runtime");
        self.runtime.shutdown_background();
        info!("Async runtime shutdown complete");
    }
}

/// Async utilities for UCI operations
pub mod async_utils {
    use super::*;
    
    /// Create a bounded channel for command processing
    pub fn create_command_channel<T>(buffer_size: usize) -> (mpsc::Sender<T>, mpsc::Receiver<T>) {
        mpsc::channel(buffer_size)
    }
    
    /// Create an unbounded channel for event processing
    pub fn create_event_channel<T>() -> (mpsc::UnboundedSender<T>, mpsc::UnboundedReceiver<T>) {
        mpsc::unbounded_channel()
    }
    
    /// Create a oneshot channel for request-response patterns
    pub fn create_oneshot_channel<T>() -> (oneshot::Sender<T>, oneshot::Receiver<T>) {
        oneshot::channel()
    }
    
    /// Safe sleep with cancellation support
    pub async fn safe_sleep(duration: Duration) -> UCIResult<()> {
        sleep(duration).await;
        Ok(())
    }
    
    /// Timeout wrapper with error conversion
    pub async fn with_timeout<F>(
        future: F,
        timeout_ms: u64,
    ) -> UCIResult<F::Output>
    where
        F: std::future::Future,
    {
        let timeout_duration = Duration::from_millis(timeout_ms);
        timeout(timeout_duration, future)
            .await
            .map_err(|_| UCIError::Timeout { duration_ms: timeout_ms })
    }
    
    /// Retry an async operation with exponential backoff
    pub async fn retry_with_backoff<F, Fut, T>(
        mut operation: F,
        max_retries: usize,
        initial_delay_ms: u64,
    ) -> UCIResult<T>
    where
        F: FnMut() -> Fut,
        Fut: std::future::Future<Output = UCIResult<T>>,
    {
        let mut delay_ms = initial_delay_ms;
        let mut last_error = None;
        
        for attempt in 0..=max_retries {
            match operation().await {
                Ok(result) => {
                    if attempt > 0 {
                        info!("Operation succeeded after {} retries", attempt);
                    }
                    return Ok(result);
                }
                Err(error) => {
                    last_error = Some(error);
                    
                    if attempt < max_retries {
                        debug!(
                            attempt = attempt + 1,
                            delay_ms = delay_ms,
                            "Operation failed, retrying"
                        );
                        sleep(Duration::from_millis(delay_ms)).await;
                        delay_ms *= 2; // Exponential backoff
                    }
                }
            }
        }
        
        Err(last_error.unwrap_or_else(|| UCIError::Internal {
            message: "Retry operation failed with no error".to_string(),
        }))
    }
    
    /// Task coordinator for managing multiple async operations
    pub struct TaskCoordinator {
        tasks: Vec<tokio::task::JoinHandle<UCIResult<()>>>,
    }
    
    impl TaskCoordinator {
        /// Create a new task coordinator
        pub fn new() -> Self {
            Self { tasks: Vec::new() }
        }
        
        /// Add a task to coordinate
        pub fn add_task<F>(&mut self, future: F)
        where
            F: std::future::Future<Output = UCIResult<()>> + Send + 'static,
        {
            let handle = tokio::spawn(future);
            self.tasks.push(handle);
        }
        
        /// Wait for all tasks to complete
        pub async fn wait_all(self) -> UCIResult<Vec<UCIResult<()>>> {
            let mut results = Vec::new();
            
            for task in self.tasks {
                match task.await {
                    Ok(result) => results.push(result),
                    Err(join_error) => {
                        return Err(UCIError::Internal {
                            message: format!("Task join error: {}", join_error),
                        });
                    }
                }
            }
            
            Ok(results)
        }
        
        /// Wait for the first task to complete successfully
        pub async fn wait_any_success(mut self) -> UCIResult<()> {
            while !self.tasks.is_empty() {
                let (result, _index, remaining) = 
                    futures::future::select_all(self.tasks).await;
                
                self.tasks = remaining;
                
                match result {
                    Ok(Ok(())) => return Ok(()),
                    Ok(Err(task_error)) => {
                        warn!("Task failed: {}", task_error);
                        // Continue waiting for other tasks
                    }
                    Err(join_error) => {
                        warn!("Task join error: {}", join_error);
                        // Continue waiting for other tasks
                    }
                }
            }
            
            Err(UCIError::Internal {
                message: "All tasks failed".to_string(),
            })
        }
    }
    
    impl Default for TaskCoordinator {
        fn default() -> Self {
            Self::new()
        }
    }
}

/// Performance monitoring for async operations
pub mod performance {
    use super::*;
    use std::sync::Arc;
    
    /// Performance metrics for async operations
    #[derive(Debug, Clone)]
    pub struct PerformanceMetrics {
        pub total_operations: u64,
        pub successful_operations: u64,
        pub failed_operations: u64,
        pub average_duration_ms: f64,
        pub min_duration_ms: u64,
        pub max_duration_ms: u64,
    }
    
    impl Default for PerformanceMetrics {
        fn default() -> Self {
            Self {
                total_operations: 0,
                successful_operations: 0,
                failed_operations: 0,
                average_duration_ms: 0.0,
                min_duration_ms: u64::MAX,
                max_duration_ms: 0,
            }
        }
    }
    
    /// Performance monitor for tracking async operation metrics
    pub struct PerformanceMonitor {
        metrics: Arc<Mutex<PerformanceMetrics>>,
    }
    
    impl PerformanceMonitor {
        /// Create a new performance monitor
        pub fn new() -> Self {
            Self {
                metrics: Arc::new(Mutex::new(PerformanceMetrics::default())),
            }
        }
        
        /// Record the result of an async operation
        pub async fn record_operation<T>(
            &self,
            operation_name: &str,
            result: Result<T, &dyn std::error::Error>,
            duration_ms: u64,
        ) {
            let mut metrics = self.metrics.lock().await;
            
            metrics.total_operations += 1;
            
            match result {
                Ok(_) => {
                    metrics.successful_operations += 1;
                    debug!(
                        operation = %operation_name,
                        duration_ms = duration_ms,
                        "✓ Async operation completed"
                    );
                }
                Err(error) => {
                    metrics.failed_operations += 1;
                    warn!(
                        operation = %operation_name,
                        duration_ms = duration_ms,
                        error = %error,
                        "✗ Async operation failed"
                    );
                }
            }
            
            // Update duration statistics
            if duration_ms < metrics.min_duration_ms {
                metrics.min_duration_ms = duration_ms;
            }
            if duration_ms > metrics.max_duration_ms {
                metrics.max_duration_ms = duration_ms;
            }
            
            // Update average duration
            let total_duration = metrics.average_duration_ms * (metrics.total_operations - 1) as f64 
                + duration_ms as f64;
            metrics.average_duration_ms = total_duration / metrics.total_operations as f64;
        }
        
        /// Get current performance metrics
        pub async fn get_metrics(&self) -> PerformanceMetrics {
            self.metrics.lock().await.clone()
        }
        
        /// Reset performance metrics
        pub async fn reset_metrics(&self) {
            *self.metrics.lock().await = PerformanceMetrics::default();
        }
        
        /// Log performance summary
        pub async fn log_performance_summary(&self) {
            let metrics = self.get_metrics().await;
            
            info!(
                total_operations = metrics.total_operations,
                successful_operations = metrics.successful_operations,
                failed_operations = metrics.failed_operations,
                success_rate = if metrics.total_operations > 0 {
                    (metrics.successful_operations as f64 / metrics.total_operations as f64) * 100.0
                } else { 0.0 },
                average_duration_ms = metrics.average_duration_ms,
                min_duration_ms = if metrics.min_duration_ms == u64::MAX { 0 } else { metrics.min_duration_ms },
                max_duration_ms = metrics.max_duration_ms,
                "📊 Async performance summary"
            );
        }
    }
    
    impl Default for PerformanceMonitor {
        fn default() -> Self {
            Self::new()
        }
    }
    
    /// Instrument an async function with performance monitoring
    #[instrument(skip(monitor, future))]
    pub async fn instrument_async<F, T>(
        monitor: &PerformanceMonitor,
        operation_name: &str,
        future: F,
    ) -> UCIResult<T>
    where
        F: std::future::Future<Output = UCIResult<T>>,
    {
        let start_time = Instant::now();
        
        let result = future.await;
        
        let duration_ms = start_time.elapsed().as_millis() as u64;
        
        let record_result: Result<_, &dyn std::error::Error> = match &result {
            Ok(_) => Ok(()),
            Err(error) => Err(error),
        };
        
        monitor.record_operation(operation_name, record_result, duration_ms).await;
        
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tokio::time::Duration;
    
    #[test]
    fn test_runtime_config_default() {
        let config = RuntimeConfig::default();
        assert_eq!(config.worker_threads, Some(1));
        assert_eq!(config.thread_name, "opera-uci");
        assert!(config.enable_io);
        assert!(config.enable_time);
        assert_eq!(config.default_timeout_ms, 30_000);
    }
    
    #[test]
    fn test_runtime_config_uci_optimized() {
        let config = RuntimeConfig::uci_optimized();
        assert_eq!(config.worker_threads, Some(1));
        assert_eq!(config.default_timeout_ms, 10_000);
        assert_eq!(config.max_blocking_threads, 2);
    }
    
    #[test]
    fn test_runtime_config_development() {
        let config = RuntimeConfig::development();
        assert_eq!(config.worker_threads, Some(2));
        assert_eq!(config.default_timeout_ms, 60_000);
        assert_eq!(config.max_blocking_threads, 8);
    }
    
    #[tokio::test]
    async fn test_async_utils_channels() {
        let (tx, mut rx) = async_utils::create_command_channel(1);
        
        tx.send("test").await.unwrap();
        let received = rx.recv().await.unwrap();
        assert_eq!(received, "test");
        
        let (tx_unbounded, mut rx_unbounded) = async_utils::create_event_channel();
        tx_unbounded.send("test").unwrap();
        let received = rx_unbounded.recv().await.unwrap();
        assert_eq!(received, "test");
        
        let (tx_oneshot, rx_oneshot) = async_utils::create_oneshot_channel();
        tx_oneshot.send("test").unwrap();
        let received = rx_oneshot.await.unwrap();
        assert_eq!(received, "test");
    }
    
    #[tokio::test]
    async fn test_safe_sleep() {
        let start = Instant::now();
        async_utils::safe_sleep(Duration::from_millis(10)).await.unwrap();
        let elapsed = start.elapsed();
        
        // Should take at least 10ms
        assert!(elapsed >= Duration::from_millis(10));
    }
    
    #[tokio::test]
    async fn test_with_timeout_success() {
        let result = async_utils::with_timeout(
            async { Ok::<&str, UCIError>("success") },
            1000,
        ).await;
        
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), Ok("success"));
    }
    
    #[tokio::test]
    async fn test_with_timeout_failure() {
        let result = async_utils::with_timeout(
            async { sleep(Duration::from_millis(100)).await },
            50,
        ).await;
        
        assert!(result.is_err());
        if let Err(UCIError::Timeout { duration_ms }) = result {
            assert_eq!(duration_ms, 50);
        } else {
            panic!("Expected timeout error");
        }
    }
    
    #[tokio::test]
    async fn test_task_coordinator() {
        let mut coordinator = async_utils::TaskCoordinator::new();
        
        coordinator.add_task(async { Ok(()) });
        coordinator.add_task(async { 
            tokio::time::sleep(Duration::from_millis(10)).await;
            Ok(())
        });
        
        let results = coordinator.wait_all().await.unwrap();
        assert_eq!(results.len(), 2);
        assert!(results.iter().all(|r| r.is_ok()));
    }
    
    #[tokio::test]
    async fn test_performance_monitor() {
        let monitor = performance::PerformanceMonitor::new();
        
        // Record successful operation
        monitor.record_operation("test_op", Ok::<(), &dyn std::error::Error>(()), 100).await;
        
        // Record failed operation
        let error = std::io::Error::new(std::io::ErrorKind::Other, "test error");
        monitor.record_operation::<()>("test_op", Err(&error), 200).await;
        
        let metrics = monitor.get_metrics().await;
        assert_eq!(metrics.total_operations, 2);
        assert_eq!(metrics.successful_operations, 1);
        assert_eq!(metrics.failed_operations, 1);
        assert_eq!(metrics.min_duration_ms, 100);
        assert_eq!(metrics.max_duration_ms, 200);
    }
}