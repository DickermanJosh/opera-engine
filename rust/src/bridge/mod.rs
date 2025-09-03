// Safe Rust bridge interfaces to C++ engine components
//
// This module provides safe, ergonomic Rust wrappers around C++ engine components,
// ensuring memory safety, proper error handling, and idiomatic Rust APIs.

#![allow(clippy::missing_docs_in_private_items)]

pub mod board;
pub mod safety_tests;

// Re-export main bridge components
pub use board::Board;