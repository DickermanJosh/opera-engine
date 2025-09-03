// FFI bridge definitions for C++ Opera Engine integration
//
// This module defines the safe FFI interface between Rust UCI coordination
// and the C++ engine core using the cxx crate.

#![allow(clippy::missing_docs_in_private_items)]
#![allow(unsafe_code)] // Required for cxx FFI bridge

#[cxx::bridge]
pub mod ffi {
    // Rust-side structs exposed to C++
    #[derive(Debug)]
    pub struct SearchLimits {
        pub depth: i32,
        pub nodes: u64,
        pub time_ms: u64,
        pub infinite: bool,
    }

    #[derive(Debug)]
    pub struct SearchInfo {
        pub depth: i32,
        pub score: i32,
        pub time_ms: u64,
        pub nodes: u64,
        pub nps: u64,
        pub pv: String,
    }

    // C++ side structs and enums
    unsafe extern "C++" {
        include!("UCIBridge.h");

        // C++ types that we'll interface with (fully qualified)
        #[namespace = "opera"]
        type Board;
        #[namespace = "opera"]
        type MoveGen;
        #[namespace = "opera"]
        type Search;

        // Board operations - simplified for initial FFI
        fn create_board() -> UniquePtr<Board>;
        fn board_set_fen(board: Pin<&mut Board>, fen: &str) -> bool;
        fn board_make_move(board: Pin<&mut Board>, move_str: &str) -> bool;
        fn board_get_fen(board: &Board) -> String;
        fn board_is_valid_move(board: &Board, move_str: &str) -> bool;
        fn board_reset(board: Pin<&mut Board>);
        fn board_is_in_check(board: &Board) -> bool;
        fn board_is_checkmate(board: &Board) -> bool;
        fn board_is_stalemate(board: &Board) -> bool;

        // Search operations - simplified interface
        fn create_search() -> UniquePtr<Search>;
        fn search_start(search: Pin<&mut Search>, board: &Board, depth: i32, time_ms: u64) -> bool;
        fn search_stop(search: Pin<&mut Search>);
        fn search_get_best_move(search: &Search) -> String;
        fn search_is_searching(search: &Search) -> bool;

        // Engine configuration
        fn engine_set_hash_size(size_mb: u32) -> bool;
        fn engine_set_threads(thread_count: u32) -> bool;
        fn engine_clear_hash() -> bool;
    }

    // Rust functions that C++ can call (callbacks)
    extern "Rust" {
        // Search progress callback
        fn on_search_progress(info: &SearchInfo);

        // Error reporting callback
        fn on_engine_error(error_msg: String);
    }
}

// Rust implementations of callback functions
/// Called by C++ engine during search to report progress
pub fn on_search_progress(info: &ffi::SearchInfo) {
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

    // TODO: Forward to UCI info output system (Task 5.3)
    println!(
        "info depth {} score cp {} time {} nodes {} nps {} pv {}",
        info.depth, info.score, info.time_ms, info.nodes, info.nps, info.pv
    );
}

/// Called by C++ engine when errors occur
pub fn on_engine_error(error_msg: String) {
    use tracing::error;

    error!(error = %error_msg, "C++ engine error");
    eprintln!("info string ERROR: {}", error_msg);
}
