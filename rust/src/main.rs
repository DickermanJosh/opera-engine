// Opera Engine UCI (Universal Chess Interface) Implementation
// 
// This is the main entry point for the Rust-based UCI coordination layer
// that bridges the C++ engine core and external chess applications.

#![deny(unsafe_code)] // Allow unsafe code only where absolutely necessary (FFI)
#![warn(
    clippy::all,
    clippy::pedantic,
    clippy::nursery,
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

use anyhow::{Context, Result};
use std::panic;
use tracing::{error, info, instrument};

// Public modules for FFI integration
pub mod ffi;

/// Main entry point for the Opera UCI engine
#[tokio::main]
async fn main() -> Result<()> {
    // Set up panic hook for never-panic operation
    setup_panic_hook();
    
    // Initialize structured logging
    setup_logging()?;
    
    info!("🎼 Opera UCI Engine starting...");
    info!("Version: {}", env!("CARGO_PKG_VERSION"));
    
    // TODO: Initialize UCI engine (Task 2.2)
    // TODO: Start async I/O processing loop (Task 2.4)
    
    // Placeholder implementation - will be replaced in subsequent tasks
    tokio::time::sleep(std::time::Duration::from_secs(1)).await;
    
    info!("Opera UCI Engine initialization complete");
    println!("id name Opera Engine");
    println!("id author Opera Engine Team");
    println!("uciok");
    
    Ok(())
}

/// Set up panic hook to ensure graceful error handling
fn setup_panic_hook() {
    panic::set_hook(Box::new(|panic_info| {
        error!("CRITICAL: Panic occurred: {}", panic_info);
        eprintln!("info string CRITICAL ERROR: Engine panic - {}", panic_info);
        std::process::abort();
    }));
}

/// Initialize structured logging with tracing
#[instrument]
fn setup_logging() -> Result<()> {
    use tracing_subscriber::{fmt, EnvFilter};
    
    // Set up environment filter with default level
    let filter = EnvFilter::try_from_default_env()
        .or_else(|_| EnvFilter::try_new("opera_uci=info"))
        .context("Failed to create logging filter")?;
    
    // Initialize subscriber with structured formatting
    fmt()
        .with_env_filter(filter)
        .with_target(false)
        .with_thread_ids(true)
        .with_file(true)
        .with_line_number(true)
        .init();
    
    Ok(())
}