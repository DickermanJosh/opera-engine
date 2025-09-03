// Comprehensive tests for UCI zero-copy command parser
//
// This test suite covers all aspects of the UCI parser including zero-copy parsing,
// input validation, sanitization, fuzzing resistance, and edge cases.

use opera_uci::uci::*;
use opera_uci::{UCIError, UCIResult};
use std::collections::HashMap;

/// Integration tests for the zero-copy parser
#[cfg(test)]
mod integration_tests {
    use super::*;

    #[test]
    fn test_parser_basic_functionality() {
        let mut parser = ZeroCopyParser::new();

        // Test all basic commands
        let commands = vec![
            ("uci", "Uci"),
            ("isready", "IsReady"),
            ("ucinewgame", "UciNewGame"),
            ("stop", "Stop"),
            ("ponderhit", "PonderHit"),
            ("quit", "Quit"),
        ];

        for (input, expected_type) in commands {
            let result = parser.parse_command(input);
            assert!(result.is_ok(), "Failed to parse '{}': {:?}", input, result);

            let cmd_type = match result.unwrap() {
                UCICommand::Uci => "Uci",
                UCICommand::IsReady => "IsReady",
                UCICommand::UciNewGame => "UciNewGame",
                UCICommand::Stop => "Stop",
                UCICommand::PonderHit => "PonderHit",
                UCICommand::Quit => "Quit",
                _ => "Other",
            };
            assert_eq!(
                cmd_type, expected_type,
                "Command type mismatch for '{}'",
                input
            );
        }
    }

    #[test]
    fn test_debug_command_variations() {
        let mut parser = ZeroCopyParser::new();

        let test_cases = vec![
            ("debug on", true),
            ("debug off", false),
            ("debug true", true),
            ("debug false", false),
            ("debug 1", true),
            ("debug 0", false),
            ("debug yes", true),
            ("debug no", false),
        ];

        for (input, expected) in test_cases {
            let result = parser.parse_command(input);
            assert!(result.is_ok(), "Failed to parse '{}': {:?}", input, result);

            if let UCICommand::Debug(flag) = result.unwrap() {
                assert_eq!(flag, expected, "Debug flag mismatch for '{}'", input);
            } else {
                panic!("Expected Debug command for '{}'", input);
            }
        }
    }

    #[test]
    fn test_position_command_comprehensive() {
        let mut parser = ZeroCopyParser::new();

        // Test startpos
        let result = parser.parse_command("position startpos");
        assert!(result.is_ok());
        if let UCICommand::Position { position, moves } = result.unwrap() {
            assert!(matches!(position, Position::StartPos));
            assert!(moves.is_empty());
        } else {
            panic!("Expected Position command");
        }

        // Test startpos with moves
        let result = parser.parse_command("position startpos moves e2e4 e7e5 nf3 nc6");
        assert!(result.is_ok());
        if let UCICommand::Position { position, moves } = result.unwrap() {
            assert!(matches!(position, Position::StartPos));
            assert_eq!(moves.len(), 4);
            assert_eq!(moves[0].from_square, "e2");
            assert_eq!(moves[0].to_square, "e4");
            assert_eq!(moves[1].from_square, "e7");
            assert_eq!(moves[1].to_square, "e5");
        } else {
            panic!("Expected Position command");
        }

        // Test FEN position (simplified for testing)
        let result = parser
            .parse_command("position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        assert!(result.is_ok());
        if let UCICommand::Position { position, .. } = result.unwrap() {
            assert!(matches!(position, Position::Fen(_)));
        } else {
            panic!("Expected Position command");
        }
    }

    #[test]
    fn test_go_command_comprehensive() {
        let mut parser = ZeroCopyParser::new();

        // Test movetime
        let result = parser.parse_command("go movetime 5000");
        assert!(result.is_ok());
        if let UCICommand::Go(tc) = result.unwrap() {
            assert_eq!(tc.move_time_ms, Some(5000));
        } else {
            panic!("Expected Go command");
        }

        // Test depth
        let result = parser.parse_command("go depth 12");
        assert!(result.is_ok());
        if let UCICommand::Go(tc) = result.unwrap() {
            assert_eq!(tc.depth, Some(12));
        } else {
            panic!("Expected Go command");
        }

        // Test infinite
        let result = parser.parse_command("go infinite");
        assert!(result.is_ok());
        if let UCICommand::Go(tc) = result.unwrap() {
            assert!(tc.infinite);
        } else {
            panic!("Expected Go command");
        }

        // Test ponder
        let result = parser.parse_command("go ponder");
        assert!(result.is_ok());
        if let UCICommand::Go(tc) = result.unwrap() {
            assert!(tc.ponder);
        } else {
            panic!("Expected Go command");
        }

        // Test complex time control
        let result =
            parser.parse_command("go wtime 300000 btime 280000 winc 2000 binc 2000 movestogo 40");
        assert!(result.is_ok());
        if let UCICommand::Go(tc) = result.unwrap() {
            assert_eq!(tc.white_time_ms, Some(300000));
            assert_eq!(tc.black_time_ms, Some(280000));
            assert_eq!(tc.white_increment_ms, Some(2000));
            assert_eq!(tc.black_increment_ms, Some(2000));
            assert_eq!(tc.moves_to_go, Some(40));
        } else {
            panic!("Expected Go command");
        }

        // Test nodes and mate
        let result = parser.parse_command("go nodes 1000000 mate 3");
        assert!(result.is_ok());
        if let UCICommand::Go(tc) = result.unwrap() {
            assert_eq!(tc.nodes, Some(1000000));
            assert_eq!(tc.mate, Some(3));
        } else {
            panic!("Expected Go command");
        }
    }

    #[test]
    fn test_setoption_command() {
        let mut parser = ZeroCopyParser::new();

        // Test Hash option
        let result = parser.parse_command("setoption name Hash value 256");
        assert!(result.is_ok());
        if let UCICommand::SetOption { name, value } = result.unwrap() {
            assert_eq!(name, "Hash");
            assert_eq!(value, Some("256"));
        } else {
            panic!("Expected SetOption command");
        }

        // Test boolean option
        let result = parser.parse_command("setoption name MorphyStyle value true");
        assert!(result.is_ok());
        if let UCICommand::SetOption { name, value } = result.unwrap() {
            assert_eq!(name, "MorphyStyle");
            assert_eq!(value, Some("true"));
        } else {
            panic!("Expected SetOption command");
        }

        // Test option without value (button type)
        let result = parser.parse_command("setoption name Clear Hash");
        assert!(result.is_ok());
        if let UCICommand::SetOption { name, value } = result.unwrap() {
            assert_eq!(name, "Clear");
            assert_eq!(value, Some("Hash"));
        } else {
            panic!("Expected SetOption command");
        }
    }

    #[test]
    fn test_register_command() {
        let mut parser = ZeroCopyParser::new();

        // Test register later
        let result = parser.parse_command("register later");
        assert!(result.is_ok());
        if let UCICommand::Register { later, name, code } = result.unwrap() {
            assert!(later);
            assert!(name.is_none());
            assert!(code.is_none());
        } else {
            panic!("Expected Register command");
        }

        // Test register with name and code
        let result = parser.parse_command("register name MyName code 12345");
        assert!(result.is_ok());
        if let UCICommand::Register { later, name, code } = result.unwrap() {
            assert!(!later);
            assert_eq!(name, Some("MyName"));
            assert_eq!(code, Some("12345"));
        } else {
            panic!("Expected Register command");
        }
    }
}

/// Error handling and edge case tests
#[cfg(test)]
mod error_handling_tests {
    use super::*;

    #[test]
    fn test_invalid_commands() {
        let mut parser = ZeroCopyParser::new();

        let invalid_commands = vec![
            "",
            "   ",
            "invalid_command",
            "uci extra_args",
            "isready extra_args",
            "stop extra_args",
            "ponderhit extra_args",
            "quit extra_args",
            "ucinewgame extra_args",
        ];

        for cmd in invalid_commands {
            let result = parser.parse_command(cmd);
            assert!(result.is_err(), "Command '{}' should be invalid", cmd);
        }
    }

    #[test]
    fn test_malformed_go_commands() {
        let mut parser = ZeroCopyParser::new();

        let invalid_go_commands = vec![
            "go wtime",             // Missing value
            "go depth abc",         // Invalid number
            "go movetime -100",     // Negative number (would fail in safe_parse)
            "go invalid_param 100", // Unknown parameter
        ];

        for cmd in invalid_go_commands {
            let result = parser.parse_command(cmd);
            assert!(result.is_err(), "Go command '{}' should be invalid", cmd);
        }
    }

    #[test]
    fn test_malformed_position_commands() {
        let mut parser = ZeroCopyParser::new();

        let invalid_position_commands = vec![
            "position",                             // No arguments
            "position invalid_type",                // Unknown position type
            "position fen",                         // FEN without string
            "position startpos moves",              // Moves without move list
            "position startpos moves invalid_move", // Invalid move format
        ];

        for cmd in invalid_position_commands {
            let result = parser.parse_command(cmd);
            assert!(
                result.is_err(),
                "Position command '{}' should be invalid",
                cmd
            );
        }
    }

    #[test]
    fn test_malformed_setoption_commands() {
        let mut parser = ZeroCopyParser::new();

        let invalid_setoption_commands = vec![
            "setoption",           // No arguments
            "setoption value 123", // Missing name
            "setoption name",      // Name without value
        ];

        for cmd in invalid_setoption_commands {
            let result = parser.parse_command(cmd);
            assert!(
                result.is_err(),
                "SetOption command '{}' should be invalid",
                cmd
            );
        }
    }

    #[test]
    fn test_malformed_debug_commands() {
        let mut parser = ZeroCopyParser::new();

        let invalid_debug_commands = vec![
            "debug",        // No arguments
            "debug maybe",  // Invalid boolean value
            "debug on off", // Too many arguments
        ];

        for cmd in invalid_debug_commands {
            let result = parser.parse_command(cmd);
            assert!(result.is_err(), "Debug command '{}' should be invalid", cmd);
        }
    }

    #[test]
    fn test_malformed_register_commands() {
        let mut parser = ZeroCopyParser::new();

        let invalid_register_commands = vec![
            "register",         // No arguments
            "register name",    // Name without value
            "register code",    // Code without value
            "register invalid", // Invalid parameter
        ];

        for cmd in invalid_register_commands {
            let result = parser.parse_command(cmd);
            assert!(
                result.is_err(),
                "Register command '{}' should be invalid",
                cmd
            );
        }
    }
}

/// Input sanitization and security tests
#[cfg(test)]
mod sanitization_tests {
    use super::*;

    #[test]
    fn test_whitespace_normalization() {
        let mut parser = ZeroCopyParser::new();

        let whitespace_tests = vec![
            ("  uci  ", "uci"),
            ("uci\t", "uci"),
            ("   go   movetime   1000   ", "go movetime 1000"),
            ("\tposition\t\tstartpos\t", "position startpos"),
        ];

        for (input, expected_cmd) in whitespace_tests {
            let result = parser.parse_command(input);
            assert!(result.is_ok(), "Failed to parse '{}'", input);

            // The exact command type check depends on the expected command
            match expected_cmd {
                "uci" => assert!(matches!(result.unwrap(), UCICommand::Uci)),
                _ => {} // For more complex commands, just verify parsing succeeded
            }
        }
    }

    #[test]
    fn test_dangerous_input_rejection() {
        let mut parser = ZeroCopyParser::new();

        let dangerous_inputs = vec![
            "uci\0",           // Null byte
            "uci\x01\x02",     // Control characters
            &"x".repeat(5000), // Extremely long command
        ];

        for input in dangerous_inputs {
            let result = parser.parse_command(input);
            assert!(
                result.is_err(),
                "Dangerous input '{}' should be rejected",
                input
            );
        }
    }

    #[test]
    fn test_resource_exhaustion_protection() {
        let mut parser = ZeroCopyParser::new();

        // Test token explosion
        let many_tokens = format!("go {}", "param 1 ".repeat(1000));
        let result = parser.parse_command(&many_tokens);
        assert!(
            result.is_err(),
            "Should reject commands with too many tokens"
        );

        // Test excessive repetition
        let repetitive = format!("go {}", "a".repeat(1000));
        let result = parser.parse_command(&repetitive);
        assert!(
            result.is_err(),
            "Should reject excessively repetitive input"
        );
    }

    #[test]
    fn test_case_insensitive_commands() {
        let mut parser = ZeroCopyParser::new();

        let case_variations = vec!["UCI", "IsReady", "STOP", "Quit", "Debug ON", "GO infinite"];

        for cmd in case_variations {
            let result = parser.parse_command(cmd);
            assert!(
                result.is_ok(),
                "Case variation '{}' should be accepted",
                cmd
            );
        }
    }
}

/// Chess-specific validation tests
#[cfg(test)]
mod chess_validation_tests {
    use super::*;

    #[test]
    fn test_move_validation() {
        let mut parser = ZeroCopyParser::new();

        // Valid moves
        let valid_moves = vec![
            "position startpos moves e2e4",
            "position startpos moves a1h8",
            "position startpos moves e7e8q",             // Promotion
            "position startpos moves h7h8R",             // Promotion (uppercase)
            "position startpos moves e2e4 e7e5 nf3 nc6", // Multiple moves
        ];

        for cmd in valid_moves {
            let result = parser.parse_command(cmd);
            assert!(result.is_ok(), "Valid move command '{}' should parse", cmd);
        }

        // Invalid moves
        let invalid_moves = vec![
            "position startpos moves e2",     // Too short
            "position startpos moves e2e4e5", // Too long
            "position startpos moves i2e4",   // Invalid file
            "position startpos moves e9e4",   // Invalid rank
            "position startpos moves e2e4x",  // Invalid promotion
            "position startpos moves e2!e4",  // Invalid characters
        ];

        for cmd in invalid_moves {
            let result = parser.parse_command(cmd);
            assert!(
                result.is_err(),
                "Invalid move command '{}' should be rejected",
                cmd
            );
        }
    }

    #[test]
    fn test_fen_validation() {
        let sanitizer = InputSanitizer::default();

        // Valid FEN
        let valid_fens = vec![
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", // Starting position
            "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 4 4", // Italian Game
            "8/8/8/8/8/8/8/8 w - - 0 1",                                // Empty board
        ];

        for fen in valid_fens {
            let result = sanitizer.validate_fen(fen);
            assert!(result.is_ok(), "Valid FEN '{}' should be accepted", fen);
        }

        // Invalid FEN
        let invalid_fens = vec![
            "invalid",                                                    // Not a FEN
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",                // Missing parts
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w",              // Missing parts
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",   // Invalid active color
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w XYZ - 0 1",    // Invalid castling
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq z9 0 1",  // Invalid en passant
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - abc 1", // Invalid halfmove
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 xyz", // Invalid fullmove
        ];

        for fen in invalid_fens {
            let result = sanitizer.validate_fen(fen);
            assert!(result.is_err(), "Invalid FEN '{}' should be rejected", fen);
        }
    }
}

/// Performance and statistics tests
#[cfg(test)]
mod performance_tests {
    use super::*;

    #[test]
    fn test_parser_statistics() {
        let mut parser = ZeroCopyParser::new();

        // Parse some commands
        let _ = parser.parse_command("uci");
        let _ = parser.parse_command("invalid_command");
        let _ = parser.parse_command("isready");

        let stats = parser.stats();
        assert_eq!(stats.commands_parsed, 3);
        assert_eq!(stats.parse_errors, 1);
        assert_eq!(stats.zero_copy_hits, 2);

        // Reset and verify
        parser.reset_stats();
        let stats = parser.stats();
        assert_eq!(stats.commands_parsed, 0);
        assert_eq!(stats.parse_errors, 0);
        assert_eq!(stats.zero_copy_hits, 0);
    }

    #[test]
    fn test_zero_copy_efficiency() {
        let mut parser = ZeroCopyParser::new();

        // These commands should use zero-copy parsing
        let zero_copy_commands = vec![
            "uci",
            "isready",
            "stop",
            "quit",
            "debug on",
            "go infinite",
            "position startpos",
        ];

        for cmd in zero_copy_commands {
            parser.reset_stats();
            let result = parser.parse_command(cmd);
            assert!(result.is_ok());

            let stats = parser.stats();
            assert_eq!(
                stats.zero_copy_hits, 1,
                "Command '{}' should use zero-copy",
                cmd
            );
            assert_eq!(
                stats.allocation_fallbacks, 0,
                "Command '{}' should not allocate",
                cmd
            );
        }
    }

    #[test]
    fn test_batch_parsing() {
        let mut batch_parser = BatchParser::new();

        let commands = vec![
            "uci".to_string(),
            "isready".to_string(),
            "position startpos".to_string(),
            "go movetime 1000".to_string(),
            "stop".to_string(),
            "quit".to_string(),
        ];

        let results = batch_parser.parse_batch(&commands);
        assert_eq!(results.len(), 6);

        // All commands should parse successfully
        for (i, result) in results.iter().enumerate() {
            assert!(result.is_ok(), "Command {} should parse successfully", i);
        }
    }
}

/// Fuzz testing and property-based tests
#[cfg(test)]
mod fuzz_tests {
    use super::*;

    #[test]
    fn test_parser_never_panics() {
        let mut parser = ZeroCopyParser::new();

        // Test with random-ish input that could cause panics
        let problematic_inputs = vec![
            "\0\0\0\0",
            &"x".repeat(10000),
            "go " + &"param ".repeat(100),
            "position fen " + &"x".repeat(300),
            "\x01\x02\x03\x04",
            "uci\nuci\nuci",
            "go wtime 18446744073709551615", // Max u64
        ];

        for input in problematic_inputs {
            // Parser should never panic, only return errors
            let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                parser.parse_command(input)
            }));

            assert!(result.is_ok(), "Parser panicked on input: '{}'", input);

            // The actual parse result can be either Ok or Err
            let _parse_result = result.unwrap();
        }
    }

    #[test]
    fn test_move_parsing_edge_cases() {
        let test_cases = vec![
            ("e2e4", true),    // Normal move
            ("a1a1", true),    // Same square (technically valid format)
            ("h8h8", true),    // Same square (technically valid format)
            ("e7e8q", true),   // Promotion
            ("E7E8Q", true),   // Uppercase promotion
            ("e7e8z", false),  // Invalid promotion piece
            ("z2e4", false),   // Invalid from square
            ("e2z4", false),   // Invalid to square
            ("e0e4", false),   // Invalid rank
            ("e2e9", false),   // Invalid rank
            ("2e4", false),    // Too short
            ("ee2e4", false),  // Too long without promotion
            ("e2e4qq", false), // Too long with promotion
        ];

        for (move_str, should_be_valid) in test_cases {
            let result = ChessMove::new(move_str);
            if should_be_valid {
                assert!(result.is_ok(), "Move '{}' should be valid", move_str);
            } else {
                assert!(result.is_err(), "Move '{}' should be invalid", move_str);
            }
        }
    }
}

/// Real-world usage simulation tests
#[cfg(test)]
mod integration_simulation_tests {
    use super::*;

    #[test]
    fn test_typical_uci_session() {
        let mut parser = ZeroCopyParser::new();

        // Simulate a typical UCI session
        let session_commands = vec![
            "uci",
            "setoption name Hash value 128",
            "setoption name Threads value 1",
            "isready",
            "ucinewgame",
            "position startpos",
            "go movetime 1000",
            "stop",
            "position startpos moves e2e4",
            "go wtime 300000 btime 300000 winc 2000 binc 2000",
            "stop",
            "quit",
        ];

        for (i, cmd) in session_commands.iter().enumerate() {
            let result = parser.parse_command(cmd);
            assert!(
                result.is_ok(),
                "Session command {} '{}' should parse: {:?}",
                i,
                cmd,
                result
            );
        }

        // Verify stats
        let stats = parser.stats();
        assert_eq!(stats.commands_parsed, session_commands.len() as u64);
        assert_eq!(stats.parse_errors, 0);
    }

    #[test]
    fn test_chess_game_simulation() {
        let mut parser = ZeroCopyParser::new();

        // Simulate parsing moves from a short chess game
        let game_moves = "e2e4 e7e5 nf3 nc6 bb5 a6 ba4 nf6 o-o be7";
        let position_cmd = format!("position startpos moves {}", game_moves);

        let result = parser.parse_command(&position_cmd);
        assert!(result.is_ok(), "Game position should parse successfully");

        if let UCICommand::Position { moves, .. } = result.unwrap() {
            assert!(moves.len() >= 8, "Should have parsed multiple moves");
        } else {
            panic!("Expected Position command");
        }
    }
}
