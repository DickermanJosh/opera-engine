// UCI command types and enums for zero-copy parsing
//
// This module defines all UCI command variants with zero-allocation string slicing
// and comprehensive input validation for never-panic operation.

use std::collections::HashMap;
use std::fmt;
// use std::str::FromStr; // TODO: Re-enable when FromStr implementations are added

use crate::error::{UCIError, UCIResult};

/// Represents a chess move in algebraic notation (e.g., "e2e4", "e7e8q")
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ChessMove<'a> {
    pub from_square: &'a str,
    pub to_square: &'a str, 
    pub promotion: Option<&'a str>,
}

impl<'a> ChessMove<'a> {
    /// Create a new chess move from zero-copy string slices
    pub fn new(move_str: &'a str) -> UCIResult<Self> {
        if move_str.len() < 4 || move_str.len() > 5 {
            return Err(UCIError::Move {
                message: format!("Invalid move length: '{}' (expected 4-5 chars)", move_str),
            });
        }

        // Validate characters are algebraic notation
        if !move_str.chars().all(|c| c.is_ascii_alphanumeric()) {
            return Err(UCIError::Move {
                message: format!("Invalid move characters: '{}'", move_str),
            });
        }

        let from_square = &move_str[0..2];
        let to_square = &move_str[2..4];
        let promotion = if move_str.len() == 5 {
            Some(&move_str[4..5])
        } else {
            None
        };

        // Validate square format (letter + number)
        Self::validate_square(from_square)?;
        Self::validate_square(to_square)?;

        // Validate promotion piece if present
        if let Some(promo) = promotion {
            if !matches!(promo, "q" | "r" | "b" | "n" | "Q" | "R" | "B" | "N") {
                return Err(UCIError::Move {
                    message: format!("Invalid promotion piece: '{}'", promo),
                });
            }
        }

        Ok(Self {
            from_square,
            to_square,
            promotion,
        })
    }

    fn validate_square(square: &str) -> UCIResult<()> {
        if square.len() != 2 {
            return Err(UCIError::Move {
                message: format!("Invalid square format: '{}' (expected 2 chars)", square),
            });
        }

        let file = square.chars().next().unwrap();
        let rank = square.chars().nth(1).unwrap();

        if !matches!(file, 'a'..='h' | 'A'..='H') {
            return Err(UCIError::Move {
                message: format!("Invalid file: '{}' (expected a-h)", file),
            });
        }

        if !matches!(rank, '1'..='8') {
            return Err(UCIError::Move {
                message: format!("Invalid rank: '{}' (expected 1-8)", rank),
            });
        }

        Ok(())
    }
}

impl<'a> fmt::Display for ChessMove<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}{}", self.from_square, self.to_square)?;
        if let Some(promo) = self.promotion {
            write!(f, "{}", promo)?;
        }
        Ok(())
    }
}

/// Time control parameters for search commands
#[derive(Debug, Clone, PartialEq)]
pub struct TimeControl {
    pub white_time_ms: Option<u64>,
    pub black_time_ms: Option<u64>,
    pub white_increment_ms: Option<u64>,
    pub black_increment_ms: Option<u64>,
    pub moves_to_go: Option<u32>,
    pub move_time_ms: Option<u64>,
    pub infinite: bool,
    pub ponder: bool,
    pub depth: Option<u32>,
    pub nodes: Option<u64>,
    pub mate: Option<u32>,
}

impl Default for TimeControl {
    fn default() -> Self {
        Self {
            white_time_ms: None,
            black_time_ms: None,
            white_increment_ms: None,
            black_increment_ms: None,
            moves_to_go: None,
            move_time_ms: None,
            infinite: false,
            ponder: false,
            depth: None,
            nodes: None,
            mate: None,
        }
    }
}

/// UCI option types with validation
#[derive(Debug, Clone, PartialEq)]
pub enum OptionValue<'a> {
    Check(bool),
    Spin { value: i32, min: i32, max: i32 },
    Combo { value: &'a str, choices: Vec<&'a str> },
    Button,
    String(&'a str),
}

/// All UCI commands supported by the parser
#[derive(Debug, Clone, PartialEq)]
pub enum UCICommand<'a> {
    /// Initialize UCI protocol
    Uci,
    
    /// Query if engine is ready
    IsReady,
    
    /// Set up new game
    UciNewGame,
    
    /// Set board position
    Position {
        /// Starting position: "startpos" or FEN string
        position: Position<'a>,
        /// List of moves to apply
        moves: Vec<ChessMove<'a>>,
    },
    
    /// Start searching
    Go(TimeControl),
    
    /// Stop current search
    Stop,
    
    /// Handle ponderhit during pondering
    PonderHit,
    
    /// Set engine option
    SetOption {
        name: &'a str,
        value: Option<&'a str>,
    },
    
    /// Exit program
    Quit,
    
    /// Debug mode toggle
    Debug(bool),
    
    /// Register engine (for copy protection)
    Register {
        later: bool,
        name: Option<&'a str>,
        code: Option<&'a str>,
    },
}

/// Position specification for position command
#[derive(Debug, Clone, PartialEq)]
pub enum Position<'a> {
    /// Starting position
    StartPos,
    /// Position from FEN string
    Fen(&'a str),
}

impl<'a> Position<'a> {
    /// Validate FEN string format (basic validation)
    pub fn validate_fen(fen: &str) -> UCIResult<()> {
        let parts: Vec<&str> = fen.split_whitespace().collect();
        
        if parts.len() != 6 {
            return Err(UCIError::Position {
                message: format!("Invalid FEN: expected 6 parts, got {}", parts.len()),
            });
        }

        // Basic piece placement validation
        let piece_placement = parts[0];
        if piece_placement.split('/').count() != 8 {
            return Err(UCIError::Position {
                message: "Invalid FEN: piece placement must have 8 ranks".to_string(),
            });
        }

        // Active color validation
        if !matches!(parts[1], "w" | "b") {
            return Err(UCIError::Position {
                message: format!("Invalid FEN: active color must be 'w' or 'b', got '{}'", parts[1]),
            });
        }

        // Castling availability validation
        let castling = parts[2];
        if castling != "-" && !castling.chars().all(|c| matches!(c, 'K' | 'Q' | 'k' | 'q')) {
            return Err(UCIError::Position {
                message: format!("Invalid FEN: invalid castling rights '{}'", castling),
            });
        }

        // En passant validation
        let en_passant = parts[3];
        if en_passant != "-" {
            if en_passant.len() != 2 {
                return Err(UCIError::Position {
                    message: format!("Invalid FEN: invalid en passant square '{}'", en_passant),
                });
            }
            // Validate it's a valid square
            ChessMove::validate_square(en_passant)?;
        }

        // Halfmove and fullmove validation
        parts[4].parse::<u32>().map_err(|_| UCIError::Position {
            message: format!("Invalid FEN: halfmove clock must be a number, got '{}'", parts[4]),
        })?;

        parts[5].parse::<u32>().map_err(|_| UCIError::Position {
            message: format!("Invalid FEN: fullmove number must be a number, got '{}'", parts[5]),
        })?;

        Ok(())
    }
}

/// Raw UCI command line with metadata for parsing
#[derive(Debug, Clone)]
pub struct RawCommand<'a> {
    /// Original command line
    pub raw: &'a str,
    /// Command name (first token)
    pub command: &'a str,
    /// Command arguments (remaining tokens)
    pub args: Vec<&'a str>,
    /// Length of original command for bounds checking
    pub length: usize,
}

impl<'a> RawCommand<'a> {
    /// Create a new raw command from input line with zero-copy parsing
    pub fn new(line: &'a str) -> UCIResult<Self> {
        let trimmed = line.trim();
        
        if trimmed.is_empty() {
            return Err(UCIError::Protocol {
                message: "Empty command line".to_string(),
            });
        }

        // Validate command length to prevent resource exhaustion
        if trimmed.len() > 4096 {
            return Err(UCIError::Protocol {
                message: format!("Command too long: {} chars (max 4096)", trimmed.len()),
            });
        }

        // Split into tokens using zero-copy string slicing
        let tokens: Vec<&str> = trimmed.split_whitespace().collect();
        
        if tokens.is_empty() {
            return Err(UCIError::Protocol {
                message: "No command tokens found".to_string(),
            });
        }

        let command = tokens[0];
        let args = if tokens.len() > 1 {
            tokens[1..].to_vec()
        } else {
            Vec::new()
        };

        Ok(Self {
            raw: trimmed,
            command,
            args,
            length: trimmed.len(),
        })
    }

    /// Get argument at index with bounds checking
    pub fn get_arg(&self, index: usize) -> Option<&'a str> {
        self.args.get(index).copied()
    }

    /// Get remaining args starting from index
    pub fn get_args_from(&self, index: usize) -> &[&'a str] {
        if index < self.args.len() {
            &self.args[index..]
        } else {
            &[]
        }
    }

    /// Parse key-value pairs from arguments (e.g., "name Hash value 64")
    pub fn parse_key_value_pairs(&self) -> HashMap<&'a str, &'a str> {
        let mut pairs = HashMap::new();
        let mut i = 0;
        
        while i < self.args.len() {
            if i + 4 <= self.args.len() && self.args[i] == "name" && self.args[i + 2] == "value" {
                // UCI setoption pattern: "name <key> value <value>"
                let option_name = self.args[i + 1];
                let option_value = self.args[i + 3];
                pairs.insert("name", option_name);
                pairs.insert(option_name, option_value);
                i += 4;
            } else if i + 2 < self.args.len() && self.args[i + 1] == "value" {
                // Pattern: "<key> value <value>"
                pairs.insert(self.args[i], self.args[i + 2]);
                i += 3;
            } else if i + 1 < self.args.len() {
                // Pattern: "<key> <value>"
                pairs.insert(self.args[i], self.args[i + 1]);
                i += 2;
            } else {
                // Single argument (flag)
                pairs.insert(self.args[i], "");
                i += 1;
            }
        }
        
        pairs
    }
}

/// Safe string to number parsing with bounds checking
pub trait SafeParse<T> {
    fn safe_parse(s: &str, context: &str) -> UCIResult<T>;
}

impl SafeParse<u32> for u32 {
    fn safe_parse(s: &str, context: &str) -> UCIResult<u32> {
        s.parse().map_err(|_| UCIError::Protocol {
            message: format!("Invalid {} number: '{}'", context, s),
        })
    }
}

impl SafeParse<u64> for u64 {
    fn safe_parse(s: &str, context: &str) -> UCIResult<u64> {
        s.parse().map_err(|_| UCIError::Protocol {
            message: format!("Invalid {} number: '{}'", context, s),
        })
    }
}

impl SafeParse<i32> for i32 {
    fn safe_parse(s: &str, context: &str) -> UCIResult<i32> {
        s.parse().map_err(|_| UCIError::Protocol {
            message: format!("Invalid {} number: '{}'", context, s),
        })
    }
}

impl SafeParse<bool> for bool {
    fn safe_parse(s: &str, context: &str) -> UCIResult<bool> {
        match s.to_lowercase().as_str() {
            "true" | "1" | "yes" | "on" => Ok(true),
            "false" | "0" | "no" | "off" => Ok(false),
            _ => Err(UCIError::Protocol {
                message: format!("Invalid {} boolean: '{}' (expected true/false)", context, s),
            }),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_chess_move_creation() {
        let mv = ChessMove::new("e2e4").unwrap();
        assert_eq!(mv.from_square, "e2");
        assert_eq!(mv.to_square, "e4");
        assert_eq!(mv.promotion, None);

        let mv = ChessMove::new("e7e8q").unwrap();
        assert_eq!(mv.from_square, "e7");
        assert_eq!(mv.to_square, "e8");
        assert_eq!(mv.promotion, Some("q"));
    }

    #[test]
    fn test_chess_move_validation() {
        // Valid moves
        assert!(ChessMove::new("a1h8").is_ok());
        assert!(ChessMove::new("e7e8Q").is_ok());
        
        // Invalid moves
        assert!(ChessMove::new("e2").is_err());  // Too short
        assert!(ChessMove::new("e2e4e5").is_err());  // Too long
        assert!(ChessMove::new("i2e4").is_err());  // Invalid file
        assert!(ChessMove::new("e9e4").is_err());  // Invalid rank
        assert!(ChessMove::new("e2e4x").is_err());  // Invalid promotion
    }

    #[test]
    fn test_raw_command_parsing() {
        let cmd = RawCommand::new("go movetime 1000").unwrap();
        assert_eq!(cmd.command, "go");
        assert_eq!(cmd.args, vec!["movetime", "1000"]);
        
        let cmd = RawCommand::new("  position startpos moves e2e4 e7e5  ").unwrap();
        assert_eq!(cmd.command, "position");
        assert_eq!(cmd.args, vec!["startpos", "moves", "e2e4", "e7e5"]);
    }

    #[test]
    fn test_raw_command_validation() {
        // Empty command
        assert!(RawCommand::new("").is_err());
        assert!(RawCommand::new("   ").is_err());
        
        // Command too long
        let long_cmd = "go ".repeat(2000);
        assert!(RawCommand::new(&long_cmd).is_err());
    }

    #[test]
    fn test_key_value_parsing() {
        let cmd = RawCommand::new("setoption name Hash value 64").unwrap();
        let pairs = cmd.parse_key_value_pairs();
        assert_eq!(pairs.get("name"), Some(&"Hash"));
        assert_eq!(pairs.get("Hash"), Some(&"64"));
    }

    #[test]
    fn test_fen_validation() {
        // Valid FEN
        let fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        assert!(Position::validate_fen(fen).is_ok());
        
        // Invalid FEN - wrong number of parts
        assert!(Position::validate_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR").is_err());
        
        // Invalid FEN - wrong active color
        let fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1";
        assert!(Position::validate_fen(fen).is_err());
    }

    #[test]
    fn test_safe_parsing() {
        assert_eq!(u32::safe_parse("123", "test").unwrap(), 123);
        assert!(u32::safe_parse("abc", "test").is_err());
        
        assert_eq!(bool::safe_parse("true", "test").unwrap(), true);
        assert_eq!(bool::safe_parse("false", "test").unwrap(), false);
        assert!(bool::safe_parse("maybe", "test").is_err());
    }
}