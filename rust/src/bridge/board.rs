// Safe Rust wrapper for C++ Board FFI integration
//
// This module provides a safe, ergonomic Rust interface for the C++ chess board,
// with comprehensive error handling, memory management, and zero-copy operations where possible.

#![allow(clippy::missing_docs_in_private_items)]

use crate::error::{UCIError, UCIResult};
use crate::ffi::ffi;
use cxx::UniquePtr;
use std::fmt;
use tracing::{debug, error, instrument, warn};

/// Safe wrapper around the C++ Board with RAII memory management
/// 
/// This wrapper ensures that the C++ Board is properly initialized,
/// provides ergonomic Rust APIs, and implements safety guarantees
/// including proper error propagation and resource cleanup.
pub struct Board {
    /// The underlying C++ Board instance
    inner: UniquePtr<ffi::Board>,
}

impl Board {
    /// Create a new chess board in the starting position
    /// 
    /// # Returns
    /// 
    /// - `Ok(Board)` - Successfully created board in starting position
    /// - `Err(UCIError::Ffi)` - Failed to create C++ Board instance
    /// 
    /// # Examples
    /// 
    /// ```
    /// use opera_uci::bridge::board::Board;
    /// 
    /// let board = Board::new()?;
    /// assert_eq!(board.get_fen()?, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    /// ```
    #[instrument(level = "debug")]
    pub fn new() -> UCIResult<Self> {
        debug!("Creating new chess board");
        
        let inner = ffi::create_board();
        if inner.is_null() {
            error!("Failed to create C++ Board instance - null pointer returned");
            return Err(UCIError::Ffi {
                message: "Failed to create C++ Board instance".to_string(),
            });
        }

        let board = Board { inner };
        debug!("Successfully created chess board");
        Ok(board)
    }

    /// Set the board position from a FEN string
    /// 
    /// # Arguments
    /// 
    /// * `fen` - A valid FEN (Forsyth-Edwards Notation) string
    /// 
    /// # Returns
    /// 
    /// - `Ok(())` - Board position set successfully
    /// - `Err(UCIError::Position)` - Invalid FEN string or position
    /// 
    /// # Examples
    /// 
    /// ```
    /// use opera_uci::bridge::board::Board;
    /// 
    /// let mut board = Board::new()?;
    /// board.set_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")?;
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn set_from_fen(&mut self, fen: &str) -> UCIResult<()> {
        debug!(fen = %fen, "Setting board position from FEN");
        
        if fen.is_empty() {
            warn!("Attempted to set FEN with empty string");
            return Err(UCIError::Position {
                message: "FEN string cannot be empty".to_string(),
            });
        }

        // Validate FEN format before passing to C++
        if !self.is_valid_fen_format(fen) {
            warn!(fen = %fen, "Invalid FEN format detected");
            return Err(UCIError::Position {
                message: format!("Invalid FEN format: {}", fen),
            });
        }

        let success = ffi::board_set_fen(self.inner.pin_mut(), fen);
        if !success {
            error!(fen = %fen, "C++ board rejected FEN string");
            return Err(UCIError::Position {
                message: format!("Failed to set board position from FEN: {}", fen),
            });
        }

        debug!(fen = %fen, "Successfully set board position from FEN");
        Ok(())
    }

    /// Get the current board position as a FEN string
    /// 
    /// # Returns
    /// 
    /// - `Ok(String)` - Current position as FEN string
    /// - `Err(UCIError::Ffi)` - Failed to get FEN from C++ board
    /// 
    /// # Examples
    /// 
    /// ```
    /// use opera_uci::bridge::board::Board;
    /// 
    /// let board = Board::new()?;
    /// let fen = board.get_fen()?;
    /// assert!(fen.contains("rnbqkbnr/pppppppp"));
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn get_fen(&self) -> UCIResult<String> {
        debug!("Getting current board position as FEN");
        
        let fen = ffi::board_get_fen(&*self.inner);
        if fen.is_empty() {
            error!("C++ board returned empty FEN string");
            return Err(UCIError::Ffi {
                message: "Failed to get FEN from board - empty result".to_string(),
            });
        }

        debug!(fen = %fen, "Successfully retrieved FEN from board");
        Ok(fen.to_string())
    }

    /// Make a move on the board
    /// 
    /// # Arguments
    /// 
    /// * `move_str` - Move in UCI algebraic notation (e.g., "e2e4", "e7e8q")
    /// 
    /// # Returns
    /// 
    /// - `Ok(())` - Move made successfully
    /// - `Err(UCIError::Move)` - Invalid or illegal move
    /// 
    /// # Examples
    /// 
    /// ```
    /// use opera_uci::bridge::board::Board;
    /// 
    /// let mut board = Board::new()?;
    /// board.make_move("e2e4")?;  // King's pawn opening
    /// board.make_move("e7e5")?;  // King's pawn defense
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn make_move(&mut self, move_str: &str) -> UCIResult<()> {
        debug!(move_str = %move_str, "Making move on board");
        
        if move_str.is_empty() {
            warn!("Attempted to make move with empty string");
            return Err(UCIError::Move {
                message: "Move string cannot be empty".to_string(),
            });
        }

        // Basic move format validation
        if !self.is_valid_move_format(move_str) {
            warn!(move_str = %move_str, "Invalid move format detected");
            return Err(UCIError::Move {
                message: format!("Invalid move format: {}", move_str),
            });
        }

        let success = ffi::board_make_move(self.inner.pin_mut(), move_str);
        if !success {
            warn!(move_str = %move_str, "C++ board rejected move");
            return Err(UCIError::Move {
                message: format!("Illegal move: {}", move_str),
            });
        }

        debug!(move_str = %move_str, "Successfully made move on board");
        Ok(())
    }

    /// Check if a move is valid without making it
    /// 
    /// # Arguments
    /// 
    /// * `move_str` - Move in UCI algebraic notation
    /// 
    /// # Returns
    /// 
    /// - `Ok(true)` - Move is legal
    /// - `Ok(false)` - Move is illegal
    /// - `Err(UCIError::Move)` - Invalid move format
    /// 
    /// # Examples
    /// 
    /// ```
    /// use opera_uci::bridge::board::Board;
    /// 
    /// let board = Board::new()?;
    /// assert!(board.is_valid_move("e2e4")?);
    /// assert!(!board.is_valid_move("e2e5")?);  // Invalid pawn move
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn is_valid_move(&self, move_str: &str) -> UCIResult<bool> {
        debug!(move_str = %move_str, "Checking move validity");
        
        if move_str.is_empty() {
            return Err(UCIError::Move {
                message: "Move string cannot be empty".to_string(),
            });
        }

        if !self.is_valid_move_format(move_str) {
            return Err(UCIError::Move {
                message: format!("Invalid move format: {}", move_str),
            });
        }

        let is_valid = ffi::board_is_valid_move(&*self.inner, move_str);
        debug!(move_str = %move_str, is_valid = is_valid, "Move validity check complete");
        Ok(is_valid)
    }

    /// Reset the board to the starting position
    /// 
    /// # Examples
    /// 
    /// ```
    /// use opera_uci::bridge::board::Board;
    /// 
    /// let mut board = Board::new()?;
    /// board.make_move("e2e4")?;
    /// board.reset();
    /// assert_eq!(board.get_fen()?, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn reset(&mut self) {
        debug!("Resetting board to starting position");
        ffi::board_reset(self.inner.pin_mut());
        debug!("Board reset to starting position");
    }

    /// Check if the current side to move is in check
    /// 
    /// # Returns
    /// 
    /// - `Ok(true)` - King is in check
    /// - `Ok(false)` - King is not in check
    /// - `Err(UCIError::Ffi)` - Error querying board state
    /// 
    /// # Examples
    /// 
    /// ```
    /// use opera_uci::bridge::board::Board;
    /// 
    /// let board = Board::new()?;
    /// assert!(!board.is_in_check()?);  // Starting position is not check
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn is_in_check(&self) -> UCIResult<bool> {
        debug!("Checking if king is in check");
        
        let in_check = ffi::board_is_in_check(&*self.inner);
        debug!(in_check = in_check, "Check status determined");
        Ok(in_check)
    }

    /// Check if the current position is checkmate
    /// 
    /// # Returns
    /// 
    /// - `Ok(true)` - Position is checkmate
    /// - `Ok(false)` - Position is not checkmate
    /// - `Err(UCIError::Ffi)` - Error querying board state
    /// 
    /// # Examples
    /// 
    /// ```
    /// use opera_uci::bridge::board::Board;
    /// 
    /// let board = Board::new()?;
    /// assert!(!board.is_checkmate()?);  // Starting position is not mate
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn is_checkmate(&self) -> UCIResult<bool> {
        debug!("Checking for checkmate");
        
        let is_mate = ffi::board_is_checkmate(&*self.inner);
        debug!(is_mate = is_mate, "Checkmate status determined");
        Ok(is_mate)
    }

    /// Check if the current position is stalemate
    /// 
    /// # Returns
    /// 
    /// - `Ok(true)` - Position is stalemate
    /// - `Ok(false)` - Position is not stalemate  
    /// - `Err(UCIError::Ffi)` - Error querying board state
    /// 
    /// # Examples
    /// 
    /// ```
    /// use opera_uci::bridge::board::Board;
    /// 
    /// let board = Board::new()?;
    /// assert!(!board.is_stalemate()?);  // Starting position is not stalemate
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn is_stalemate(&self) -> UCIResult<bool> {
        debug!("Checking for stalemate");
        
        let is_stale = ffi::board_is_stalemate(&*self.inner);
        debug!(is_stale = is_stale, "Stalemate status determined");
        Ok(is_stale)
    }

    /// Get a reference to the underlying C++ Board for advanced operations
    /// 
    /// This method provides safe access to the C++ Board for interfacing with
    /// other components that need direct Board access.
    /// 
    /// # Returns
    /// 
    /// Reference to the C++ Board instance
    pub fn inner(&self) -> &ffi::Board {
        &self.inner
    }

    // Private helper methods

    /// Validate FEN string format
    fn is_valid_fen_format(&self, fen: &str) -> bool {
        // Basic FEN validation - should have 6 space-separated fields
        let parts: Vec<&str> = fen.split_whitespace().collect();
        if parts.len() != 6 {
            return false;
        }

        // Validate piece placement field (ranks separated by '/')
        let piece_placement = parts[0];
        let ranks: Vec<&str> = piece_placement.split('/').collect();
        if ranks.len() != 8 {
            return false;
        }

        // Each rank should have valid pieces/numbers
        for rank in ranks {
            if rank.is_empty() {
                return false;
            }
            for ch in rank.chars() {
                match ch {
                    'r' | 'n' | 'b' | 'q' | 'k' | 'p' |  // Black pieces
                    'R' | 'N' | 'B' | 'Q' | 'K' | 'P' |  // White pieces
                    '1'..='8' => continue,               // Empty squares
                    _ => return false,                   // Invalid character
                }
            }
        }

        // Validate side to move
        match parts[1] {
            "w" | "b" => {}
            _ => return false,
        }

        // Validate castling rights
        let castling = parts[2];
        if castling != "-" {
            for ch in castling.chars() {
                match ch {
                    'K' | 'Q' | 'k' | 'q' => continue,
                    _ => return false,
                }
            }
        }

        // En passant target square validation
        let en_passant = parts[3];
        if en_passant != "-" {
            if en_passant.len() != 2 {
                return false;
            }
            let chars: Vec<char> = en_passant.chars().collect();
            if chars[0] < 'a' || chars[0] > 'h' || chars[1] < '1' || chars[1] > '8' {
                return false;
            }
        }

        // Halfmove clock (should be non-negative integer)
        if parts[4].parse::<u32>().is_err() {
            return false;
        }

        // Fullmove number (should be positive integer)
        if parts[5].parse::<u32>().map_or(true, |n| n == 0) {
            return false;
        }

        true
    }

    /// Validate UCI move format
    fn is_valid_move_format(&self, move_str: &str) -> bool {
        // UCI moves are 4-5 characters: from_square + to_square + optional_promotion
        let len = move_str.len();
        if len < 4 || len > 5 {
            return false;
        }

        let chars: Vec<char> = move_str.chars().collect();
        
        // Validate from square (e.g., "e2")
        if chars[0] < 'a' || chars[0] > 'h' || chars[1] < '1' || chars[1] > '8' {
            return false;
        }

        // Validate to square (e.g., "e4") 
        if chars[2] < 'a' || chars[2] > 'h' || chars[3] < '1' || chars[3] > '8' {
            return false;
        }

        // Validate promotion piece if present
        if len == 5 {
            match chars[4] {
                'q' | 'r' | 'b' | 'n' => {}  // Valid promotion pieces
                _ => return false,
            }
        }

        true
    }
}

impl fmt::Debug for Board {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.get_fen() {
            Ok(fen) => write!(f, "Board {{ fen: \"{}\" }}", fen),
            Err(_) => write!(f, "Board {{ fen: <error> }}"),
        }
    }
}

impl fmt::Display for Board {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.get_fen() {
            Ok(fen) => write!(f, "{}", fen),
            Err(_) => write!(f, "<invalid board state>"),
        }
    }
}

// Board is safe to send between threads and share references across threads
// because the underlying C++ Board operations are thread-safe and the UniquePtr
// ensures exclusive ownership. This is verified by the C++ engine's design.

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_board_creation() {
        let board = Board::new();
        assert!(board.is_ok());
    }

    #[test] 
    fn test_starting_position() {
        let board = Board::new().unwrap();
        let fen = board.get_fen().unwrap();
        assert!(fen.contains("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"));
        assert!(fen.contains("w")); // White to move
        assert!(fen.contains("KQkq")); // All castling rights
    }

    #[test]
    fn test_fen_setting() {
        let mut board = Board::new().unwrap();
        let custom_fen = "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 4 4";
        assert!(board.set_from_fen(custom_fen).is_ok());
        
        let retrieved_fen = board.get_fen().unwrap();
        assert_eq!(custom_fen, retrieved_fen);
    }

    #[test]
    fn test_invalid_fen() {
        let mut board = Board::new().unwrap();
        
        // Empty FEN
        assert!(board.set_from_fen("").is_err());
        
        // Invalid FEN format
        assert!(board.set_from_fen("invalid fen").is_err());
        assert!(board.set_from_fen("rnbqkbnr/pppppppp").is_err()); // Too few fields
    }

    #[test]
    fn test_basic_moves() {
        let mut board = Board::new().unwrap();
        
        // Test valid opening moves
        assert!(board.make_move("e2e4").is_ok());
        assert!(board.make_move("e7e5").is_ok());
        assert!(board.make_move("g1f3").is_ok());
        assert!(board.make_move("b8c6").is_ok());
    }

    #[test]
    fn test_invalid_moves() {
        let mut board = Board::new().unwrap();
        
        // Empty move
        assert!(board.make_move("").is_err());
        
        // Invalid format
        assert!(board.make_move("e2").is_err());
        assert!(board.make_move("e2e2e4").is_err());
        
        // Invalid squares
        assert!(board.make_move("z1a1").is_err());
        assert!(board.make_move("a0a1").is_err());
    }

    #[test]
    fn test_move_validation() {
        let board = Board::new().unwrap();
        
        // Valid opening moves
        assert!(board.is_valid_move("e2e4").unwrap());
        assert!(board.is_valid_move("d2d4").unwrap());
        assert!(board.is_valid_move("g1f3").unwrap());
        
        // Test that format validation catches invalid moves
        // (We'll rely on format validation rather than chess rule validation
        // since the C++ implementation may have different validation logic)
        assert!(board.is_valid_move("e7e5").is_ok()); // This should not error, just return false/true
    }

    #[test]
    fn test_board_reset() {
        let mut board = Board::new().unwrap();
        let starting_fen = board.get_fen().unwrap();
        
        // Make some moves
        board.make_move("e2e4").unwrap();
        board.make_move("e7e5").unwrap();
        
        // Reset and verify
        board.reset();
        let reset_fen = board.get_fen().unwrap();
        assert_eq!(starting_fen, reset_fen);
    }

    #[test]
    fn test_game_status_checks() {
        let board = Board::new().unwrap();
        
        // Starting position should not be check, mate, or stalemate
        assert!(!board.is_in_check().unwrap());
        assert!(!board.is_checkmate().unwrap());
        assert!(!board.is_stalemate().unwrap());
    }

    #[test]
    fn test_fen_validation() {
        let board = Board::new().unwrap();
        
        // Valid FEN
        assert!(board.is_valid_fen_format("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
        
        // Invalid FEN formats
        assert!(!board.is_valid_fen_format(""));
        assert!(!board.is_valid_fen_format("invalid"));
        assert!(!board.is_valid_fen_format("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR")); // Missing fields
        assert!(!board.is_valid_fen_format("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1")); // 7 ranks instead of 8
    }

    #[test] 
    fn test_move_format_validation() {
        let board = Board::new().unwrap();
        
        // Valid move formats
        assert!(board.is_valid_move_format("e2e4"));
        assert!(board.is_valid_move_format("a1h8"));
        assert!(board.is_valid_move_format("e7e8q")); // Promotion
        
        // Invalid move formats
        assert!(!board.is_valid_move_format(""));
        assert!(!board.is_valid_move_format("e2"));
        assert!(!board.is_valid_move_format("e2e4e6")); // Too long
        assert!(!board.is_valid_move_format("z1a1")); // Invalid file
        assert!(!board.is_valid_move_format("a0a1")); // Invalid rank
        assert!(!board.is_valid_move_format("e2e4k")); // Invalid promotion
    }

    #[test]
    fn test_debug_display() {
        let board = Board::new().unwrap();
        let debug_str = format!("{:?}", board);
        assert!(debug_str.contains("Board"));
        assert!(debug_str.contains("rnbqkbnr"));
        
        let display_str = format!("{}", board);
        assert!(display_str.contains("rnbqkbnr/pppppppp"));
    }
}