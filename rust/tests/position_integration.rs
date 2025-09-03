// Integration tests for Position Command Handler
//
// Tests the complete position handling functionality including
// position setup, move application, and state management.

use opera_uci::{PositionCommandHandler, UCICommand, Position, ChessMove};

#[test]
fn test_complete_position_workflow() {
    let mut handler = PositionCommandHandler::new().expect("Should create handler");
    
    // Test 1: Set starting position
    let startpos_cmd = UCICommand::Position {
        position: Position::StartPos,
        moves: vec![],
    };
    
    handler.handle_position_command(&startpos_cmd).expect("Should handle startpos");
    
    let fen = handler.get_current_position().expect("Should get FEN");
    assert_eq!(
        fen,
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    );
    assert_eq!(handler.get_move_history().len(), 0);
    
    // Test 2: Apply valid moves
    let moves_cmd = UCICommand::Position {
        position: Position::StartPos,
        moves: vec![
            ChessMove {
                from_square: "e2",
                to_square: "e4",
                promotion: None,
            },
            ChessMove {
                from_square: "e7",
                to_square: "e5",
                promotion: None,
            },
        ],
    };
    
    handler.handle_position_command(&moves_cmd).expect("Should apply moves");
    assert_eq!(handler.get_move_history().len(), 2);
    assert_eq!(handler.get_move_history()[0], "e2e4");
    assert_eq!(handler.get_move_history()[1], "e7e5");
    
    // Test 3: Position queries
    let in_check = handler.is_in_check().expect("Should check if in check");
    assert!(!in_check, "Position should not be in check");
    
    let checkmate = handler.is_checkmate().expect("Should check if checkmate");
    assert!(!checkmate, "Position should not be checkmate");
    
    // Test 4: Reset position
    handler.reset_position().expect("Should reset position");
    assert_eq!(handler.get_move_history().len(), 0);
    
    let reset_fen = handler.get_current_position().expect("Should get reset FEN");
    assert_eq!(
        reset_fen,
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    );
}

#[test]
fn test_fen_position_handling() {
    let mut handler = PositionCommandHandler::new().expect("Should create handler");
    
    // Test custom FEN position
    let test_fen = "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 4 4";
    let fen_cmd = UCICommand::Position {
        position: Position::Fen(test_fen),
        moves: vec![],
    };
    
    handler.handle_position_command(&fen_cmd).expect("Should handle FEN position");
    
    let current_fen = handler.get_current_position().expect("Should get current FEN");
    assert_eq!(current_fen, test_fen);
    
    // Test reset to FEN position
    handler.reset_position().expect("Should reset to FEN");
    let reset_fen = handler.get_current_position().expect("Should get reset FEN");
    assert_eq!(reset_fen, test_fen, "Should reset to stored FEN, not starting position");
}

#[test]
fn test_error_handling() {
    let mut handler = PositionCommandHandler::new().expect("Should create handler");
    
    // Test invalid FEN
    let invalid_fen_cmd = UCICommand::Position {
        position: Position::Fen("invalid_fen"),
        moves: vec![],
    };
    
    let result = handler.handle_position_command(&invalid_fen_cmd);
    assert!(result.is_err(), "Should reject invalid FEN");
    
    // Position should remain unchanged after error
    let fen = handler.get_current_position().expect("Should get FEN after error");
    assert_eq!(
        fen,
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "Position should remain at starting position"
    );
}

#[test]
fn test_board_access() {
    let handler = PositionCommandHandler::new().expect("Should create handler");
    
    // Test immutable board access
    let board = handler.board();
    let fen = board.get_fen().expect("Should get FEN through board reference");
    assert_eq!(
        fen,
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    );
}

#[test]
fn test_position_handler_default() {
    let handler = PositionCommandHandler::default();
    let fen = handler.get_current_position().expect("Should get default FEN");
    assert_eq!(
        fen,
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    );
}

#[test]
fn test_move_sequence_from_fen() {
    let mut handler = PositionCommandHandler::new().expect("Should create handler");
    
    // Start from a FEN position and apply moves
    let test_fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
    let cmd = UCICommand::Position {
        position: Position::Fen(test_fen),
        moves: vec![
            ChessMove {
                from_square: "e7",
                to_square: "e5",
                promotion: None,
            },
        ],
    };
    
    handler.handle_position_command(&cmd).expect("Should handle FEN + moves");
    assert_eq!(handler.get_move_history().len(), 1);
    assert_eq!(handler.get_move_history()[0], "e7e5");
    
    // Verify the position changed from the original FEN
    let final_fen = handler.get_current_position().expect("Should get final FEN");
    assert_ne!(final_fen, test_fen, "Position should have changed after move");
}