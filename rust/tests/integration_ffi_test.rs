// Integration tests for FFI bridge functionality
//
// These tests verify that the Rust-C++ FFI interface works correctly
// and that basic engine operations can be performed through the bridge.

use opera_uci::ffi;

#[test]
fn test_ffi_compilation() {
    // This test simply ensures that the FFI bridge compiles successfully
    // Actual functionality testing will be added as we implement more features

    // Try to create a board instance
    let board = ffi::create_board();
    assert!(!board.is_null(), "Failed to create board instance");
}

#[test]
fn test_basic_board_operations() {
    let mut board = ffi::create_board();
    assert!(!board.is_null(), "Failed to create board instance");

    // Test FEN setting
    let starting_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    let result = ffi::board_set_fen(board.pin_mut(), starting_fen);
    assert!(result, "Failed to set starting FEN");

    // Test FEN getting
    let retrieved_fen = ffi::board_get_fen(&board);
    assert_eq!(retrieved_fen, starting_fen, "Retrieved FEN does not match");

    // Test board reset
    ffi::board_reset(board.pin_mut());
    let reset_fen = ffi::board_get_fen(&board);
    assert_eq!(reset_fen, starting_fen, "Board reset failed");
}

#[test]
fn test_move_operations() {
    let mut board = ffi::create_board();
    assert!(!board.is_null(), "Failed to create board instance");

    // Set up starting position
    let starting_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    ffi::board_set_fen(board.pin_mut(), starting_fen);

    // Test valid move
    let valid = ffi::board_is_valid_move(&board, "e2e4");
    assert!(valid, "e2e4 should be valid in starting position");

    // Test invalid move
    let invalid = ffi::board_is_valid_move(&board, "e2e5");
    assert!(!invalid, "e2e5 should be invalid in starting position");

    // Make a valid move
    let result = ffi::board_make_move(board.pin_mut(), "e2e4");
    assert!(result, "Failed to make move e2e4");
}

#[test]
fn test_search_creation() {
    let search = ffi::create_search();
    assert!(!search.is_null(), "Failed to create search instance");

    let is_searching = ffi::search_is_searching(&search);
    assert!(!is_searching, "Search should not be active initially");
}

#[test]
fn test_engine_configuration() {
    // Test hash size setting
    let result = ffi::engine_set_hash_size(128);
    assert!(result, "Failed to set hash size");

    // Test thread setting
    let result = ffi::engine_set_threads(4);
    assert!(result, "Failed to set thread count");

    // Test hash clearing
    let result = ffi::engine_clear_hash();
    assert!(result, "Failed to clear hash");
}
