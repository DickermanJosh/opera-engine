// UCI input sanitization and validation for security and never-panic operation
//
// This module provides comprehensive input validation and sanitization to prevent
// resource exhaustion, injection attacks, and malformed input from causing panics.

// use std::collections::HashSet; // No longer needed after removing command validation

use crate::error::{UCIError, UCIResult};

/// Maximum allowed lengths for various input components
pub struct InputLimits {
    pub max_command_length: usize,
    pub max_fen_length: usize,
    pub max_move_list_length: usize,
    pub max_option_name_length: usize,
    pub max_option_value_length: usize,
    pub max_moves_per_command: usize,
    pub max_tokens_per_command: usize,
}

impl Default for InputLimits {
    fn default() -> Self {
        Self {
            max_command_length: 4096,      // Reasonable command line limit
            max_fen_length: 256,           // FEN strings are typically < 100 chars
            max_move_list_length: 2048,    // Support long games
            max_option_name_length: 64,    // UCI option names are short
            max_option_value_length: 256,  // Option values can be paths
            max_moves_per_command: 512,    // Support very long games
            max_tokens_per_command: 1024,  // Prevent token explosion
        }
    }
}

/// UCI input sanitizer with configurable limits and validation rules
pub struct InputSanitizer {
    limits: InputLimits,
}

impl Default for InputSanitizer {
    fn default() -> Self {
        Self::new(InputLimits::default())
    }
}

impl InputSanitizer {
    /// Create new sanitizer with specified limits
    pub fn new(limits: InputLimits) -> Self {
        Self {
            limits,
        }
    }

    /// Sanitize and validate a complete UCI command line
    pub fn sanitize_command_line(&self, line: &str) -> UCIResult<String> {
        // Remove dangerous characters and normalize whitespace
        let sanitized = self.sanitize_string(line)?;
        
        // Validate overall command structure
        self.validate_command_structure(&sanitized)?;
        
        Ok(sanitized)
    }

    /// Sanitize a general input string
    pub fn sanitize_string(&self, input: &str) -> UCIResult<String> {
        // Check for null bytes and control characters that could cause issues
        if input.contains('\0') {
            return Err(UCIError::Protocol {
                message: "Input contains null bytes".to_string(),
            });
        }

        // Remove dangerous control characters except whitespace
        let cleaned: String = input
            .chars()
            .filter(|&c| c.is_ascii_graphic() || c == ' ' || c == '\t')
            .collect();

        // Normalize and trim whitespace
        let normalized = cleaned
            .split_whitespace()
            .collect::<Vec<_>>()
            .join(" ");

        if normalized.is_empty() && !input.trim().is_empty() {
            return Err(UCIError::Protocol {
                message: "Input contains only invalid characters".to_string(),
            });
        }

        Ok(normalized)
    }

    /// Validate command structure and limits
    pub fn validate_command_structure(&self, command: &str) -> UCIResult<()> {
        // Length validation
        if command.len() > self.limits.max_command_length {
            return Err(UCIError::Protocol {
                message: format!(
                    "Command too long: {} chars (max {})",
                    command.len(),
                    self.limits.max_command_length
                ),
            });
        }

        // Token count validation
        let tokens: Vec<&str> = command.split_whitespace().collect();
        if tokens.len() > self.limits.max_tokens_per_command {
            return Err(UCIError::Protocol {
                message: format!(
                    "Too many tokens: {} (max {})",
                    tokens.len(),
                    self.limits.max_tokens_per_command
                ),
            });
        }

        if tokens.is_empty() {
            return Err(UCIError::Protocol {
                message: "Empty command".to_string(),
            });
        }

        // Note: Command validation is handled by the parser, not the sanitizer
        // The sanitizer focuses on security and safety, not command correctness

        Ok(())
    }

    /// Validate FEN string format and content
    pub fn validate_fen(&self, fen: &str) -> UCIResult<()> {
        if fen.len() > self.limits.max_fen_length {
            return Err(UCIError::Position {
                message: format!(
                    "FEN too long: {} chars (max {})",
                    fen.len(),
                    self.limits.max_fen_length
                ),
            });
        }

        // Check for dangerous characters in FEN
        if !fen.chars().all(|c| {
            c.is_ascii_alphanumeric() || " /-KQRBNPkqrbnp".contains(c)
        }) {
            return Err(UCIError::Position {
                message: "FEN contains invalid characters".to_string(),
            });
        }

        // Basic FEN structure validation
        let parts: Vec<&str> = fen.split_whitespace().collect();
        if parts.len() != 6 {
            return Err(UCIError::Position {
                message: format!("Invalid FEN structure: {} parts (expected 6)", parts.len()),
            });
        }

        // Validate piece placement
        self.validate_piece_placement(parts[0])?;
        
        // Validate active color
        self.validate_active_color(parts[1])?;
        
        // Validate castling rights
        self.validate_castling_rights(parts[2])?;
        
        // Validate en passant square
        self.validate_en_passant_square(parts[3])?;
        
        // Validate halfmove clock
        self.validate_halfmove_clock(parts[4])?;
        
        // Validate fullmove number
        self.validate_fullmove_number(parts[5])?;

        Ok(())
    }

    fn validate_piece_placement(&self, placement: &str) -> UCIResult<()> {
        let ranks: Vec<&str> = placement.split('/').collect();
        if ranks.len() != 8 {
            return Err(UCIError::Position {
                message: format!("Invalid piece placement: {} ranks (expected 8)", ranks.len()),
            });
        }

        for (rank_idx, rank) in ranks.iter().enumerate() {
            if rank.is_empty() {
                return Err(UCIError::Position {
                    message: format!("Empty rank {} in piece placement", rank_idx + 1),
                });
            }

            // Validate each character in rank
            for c in rank.chars() {
                if !("12345678KQRBNPkqrbnp".contains(c)) {
                    return Err(UCIError::Position {
                        message: format!("Invalid character '{}' in piece placement", c),
                    });
                }
            }
        }

        Ok(())
    }

    fn validate_active_color(&self, color: &str) -> UCIResult<()> {
        if !matches!(color, "w" | "b") {
            return Err(UCIError::Position {
                message: format!("Invalid active color: '{}' (expected 'w' or 'b')", color),
            });
        }
        Ok(())
    }

    fn validate_castling_rights(&self, castling: &str) -> UCIResult<()> {
        if castling == "-" {
            return Ok(());
        }

        for c in castling.chars() {
            if !matches!(c, 'K' | 'Q' | 'k' | 'q') {
                return Err(UCIError::Position {
                    message: format!("Invalid castling right: '{}'", c),
                });
            }
        }

        Ok(())
    }

    fn validate_en_passant_square(&self, en_passant: &str) -> UCIResult<()> {
        if en_passant == "-" {
            return Ok(());
        }

        if en_passant.len() != 2 {
            return Err(UCIError::Position {
                message: format!("Invalid en passant square length: '{}'", en_passant),
            });
        }

        let file = en_passant.chars().next().unwrap();
        let rank = en_passant.chars().nth(1).unwrap();

        if !matches!(file, 'a'..='h') {
            return Err(UCIError::Position {
                message: format!("Invalid en passant file: '{}'", file),
            });
        }

        if !matches!(rank, '1'..='8') {
            return Err(UCIError::Position {
                message: format!("Invalid en passant rank: '{}'", rank),
            });
        }

        Ok(())
    }

    fn validate_halfmove_clock(&self, halfmove: &str) -> UCIResult<()> {
        let value: u32 = halfmove.parse().map_err(|_| UCIError::Position {
            message: format!("Invalid halfmove clock: '{}'", halfmove),
        })?;

        // Reasonable upper bound for halfmove clock
        if value > 150 {
            return Err(UCIError::Position {
                message: format!("Halfmove clock too high: {} (max 150)", value),
            });
        }

        Ok(())
    }

    fn validate_fullmove_number(&self, fullmove: &str) -> UCIResult<()> {
        let value: u32 = fullmove.parse().map_err(|_| UCIError::Position {
            message: format!("Invalid fullmove number: '{}'", fullmove),
        })?;

        if value == 0 {
            return Err(UCIError::Position {
                message: "Fullmove number cannot be zero".to_string(),
            });
        }

        // Reasonable upper bound for fullmove number
        if value > 10000 {
            return Err(UCIError::Position {
                message: format!("Fullmove number too high: {} (max 10000)", value),
            });
        }

        Ok(())
    }

    /// Validate a move string
    pub fn validate_move(&self, move_str: &str) -> UCIResult<()> {
        if move_str.len() < 4 || move_str.len() > 5 {
            return Err(UCIError::Move {
                message: format!("Invalid move length: '{}' (expected 4-5 chars)", move_str),
            });
        }

        // Only allow valid chess notation characters (files: a-h, ranks: 1-8, promotions: qrbn)
        // Note: 'b' for bishop is included in 'a'..='h' range
        if !move_str.chars().all(|c| matches!(c, 'a'..='h' | '1'..='8' | 'q' | 'r' | 'n')) {
            return Err(UCIError::Move {
                message: format!("Move contains invalid characters: '{}'", move_str),
            });
        }

        Ok(())
    }

    /// Validate a move list
    pub fn validate_move_list(&self, moves: &[&str]) -> UCIResult<()> {
        if moves.len() > self.limits.max_moves_per_command {
            return Err(UCIError::Protocol {
                message: format!(
                    "Too many moves: {} (max {})",
                    moves.len(),
                    self.limits.max_moves_per_command
                ),
            });
        }

        for move_str in moves {
            self.validate_move(move_str)?;
        }

        Ok(())
    }

    /// Validate option name and value
    pub fn validate_option(&self, name: &str, value: Option<&str>) -> UCIResult<()> {
        if name.len() > self.limits.max_option_name_length {
            return Err(UCIError::Configuration {
                message: format!(
                    "Option name too long: {} chars (max {})",
                    name.len(),
                    self.limits.max_option_name_length
                ),
            });
        }

        if let Some(val) = value {
            if val.len() > self.limits.max_option_value_length {
                return Err(UCIError::Configuration {
                    message: format!(
                        "Option value too long: {} chars (max {})",
                        val.len(),
                        self.limits.max_option_value_length
                    ),
                });
            }
        }

        // Option names should be alphanumeric with spaces and underscores
        if !name.chars().all(|c| c.is_alphanumeric() || c == ' ' || c == '_') {
            return Err(UCIError::Configuration {
                message: format!("Invalid characters in option name: '{}'", name),
            });
        }

        Ok(())
    }

    /// Check for potential resource exhaustion patterns
    pub fn check_resource_exhaustion(&self, input: &str) -> UCIResult<()> {
        // Check for excessive repetition
        let mut char_counts = std::collections::HashMap::new();
        for c in input.chars() {
            *char_counts.entry(c).or_insert(0) += 1;
        }

        // Flag if any character appears excessively
        let max_char_count = input.len() / 4; // No character should be > 25% of input
        for (c, count) in char_counts {
            if count > max_char_count && count > 100 {
                return Err(UCIError::Protocol {
                    message: format!("Excessive repetition of character '{}' ({})", c, count),
                });
            }
        }

        // Check for potential zip bomb patterns in command structure
        let token_count = input.split_whitespace().count();
        if token_count > 0 && input.len() / token_count < 2 {
            return Err(UCIError::Protocol {
                message: "Suspiciously dense token structure".to_string(),
            });
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_string_sanitization() {
        let sanitizer = InputSanitizer::default();
        
        // Normal input
        assert_eq!(
            sanitizer.sanitize_string("go movetime 1000").unwrap(),
            "go movetime 1000"
        );
        
        // Input with extra whitespace
        assert_eq!(
            sanitizer.sanitize_string("  go   movetime   1000  ").unwrap(),
            "go movetime 1000"
        );
        
        // Input with control characters
        assert!(sanitizer.sanitize_string("go\x00movetime").is_err());
    }

    #[test]
    fn test_command_validation() {
        let sanitizer = InputSanitizer::default();
        
        // Valid commands
        assert!(sanitizer.validate_command_structure("uci").is_ok());
        assert!(sanitizer.validate_command_structure("go movetime 1000").is_ok());
        
        // Commands are allowed through - validation happens in parser
        assert!(sanitizer.validate_command_structure("invalid_command").is_ok());
        assert!(sanitizer.validate_command_structure("").is_err());
    }

    #[test]
    fn test_fen_validation() {
        let sanitizer = InputSanitizer::default();
        
        // Valid FEN
        let valid_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        assert!(sanitizer.validate_fen(valid_fen).is_ok());
        
        // Invalid FEN - wrong structure
        assert!(sanitizer.validate_fen("invalid fen").is_err());
        
        // FEN too long
        let long_fen = "a".repeat(1000);
        assert!(sanitizer.validate_fen(&long_fen).is_err());
    }

    #[test]
    fn test_move_validation() {
        let sanitizer = InputSanitizer::default();
        
        // Valid moves
        assert!(sanitizer.validate_move("e2e4").is_ok());
        assert!(sanitizer.validate_move("e7e8q").is_ok());
        
        // Invalid moves
        assert!(sanitizer.validate_move("e2").is_err());  // Too short
        assert!(sanitizer.validate_move("e2e4x").is_err()); // Invalid character
    }

    #[test]
    fn test_move_list_validation() {
        let sanitizer = InputSanitizer::default();
        
        let moves = vec!["e2e4", "e7e5", "g1f3"];
        assert!(sanitizer.validate_move_list(&moves).is_ok());
        
        // Too many moves
        let many_moves: Vec<&str> = (0..1000).map(|_| "e2e4").collect();
        assert!(sanitizer.validate_move_list(&many_moves).is_err());
    }

    #[test]
    fn test_option_validation() {
        let sanitizer = InputSanitizer::default();
        
        // Valid option
        assert!(sanitizer.validate_option("Hash", Some("64")).is_ok());
        
        // Option name too long
        let long_name = "a".repeat(100);
        assert!(sanitizer.validate_option(&long_name, Some("value")).is_err());
        
        // Invalid characters in option name
        assert!(sanitizer.validate_option("Hash@Value", Some("64")).is_err());
    }

    #[test]
    fn test_resource_exhaustion_detection() {
        let sanitizer = InputSanitizer::default();
        
        // Normal input
        assert!(sanitizer.check_resource_exhaustion("go movetime 1000").is_ok());
        
        // Excessive repetition
        let repetitive = "a".repeat(200);
        assert!(sanitizer.check_resource_exhaustion(&repetitive).is_err());
    }
}