//! Integration regressions check returned moves against the current legal board,
//! rather than accepting a fixed bestmove or a successful process exit.
#![cfg(feature = "ffi")]
use opera_uci::{bridge::Board, uci::UCIEngine};
use std::time::Duration;

#[tokio::test]
async fn ponder_replies_are_legal_and_terminal_positions_have_none() {
    let engine = UCIEngine::new();
    engine.initialize().await.unwrap();
    engine
        .process_command("setoption name Ponder value true")
        .await
        .unwrap();
    let mut output = engine.subscribe_responses();
    for fen in [
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
        "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1",
    ] {
        engine
            .process_command(&format!("position fen {fen}"))
            .await
            .unwrap();
        engine.process_command("go depth 2").await.unwrap();
        let line = tokio::time::timeout(Duration::from_secs(2), async {
            loop {
                let line = output.recv().await.unwrap();
                assert!(!line.starts_with("info string ERROR"), "{line}");
                if line.starts_with("bestmove ") {
                    break line;
                }
            }
        })
        .await
        .unwrap();
        let mut board = Board::new().unwrap();
        board.set_from_fen(fen).unwrap();
        if board.is_checkmate().unwrap() {
            assert_eq!(line, "bestmove 0000");
        } else {
            let tokens: Vec<_> = line.split_whitespace().collect();
            assert_eq!(tokens.len(), 4, "{line}");
            assert_eq!(tokens[2], "ponder");
            board.make_move(tokens[1]).unwrap();
            assert!(board.is_valid_move(tokens[3]).unwrap(), "{line}");
        }
    }
    engine.process_command("quit").await.unwrap();
}

#[tokio::test]
async fn legal_moves_and_pv_across_a_game() {
    let engine = UCIEngine::new();
    engine.initialize().await.unwrap();
    let mut output = engine.subscribe_responses();
    let mut board = Board::new().unwrap();
    let mut moves = Vec::new();
    for ply in 0..60 {
        if board.is_checkmate().unwrap() || board.is_stalemate().unwrap() {
            break;
        }
        let position = if moves.is_empty() {
            "position startpos".to_owned()
        } else {
            format!("position startpos moves {}", moves.join(" "))
        };
        engine.process_command(&position).await.unwrap();
        engine.process_command("go depth 2").await.unwrap();
        let best = tokio::time::timeout(Duration::from_secs(2), async {
            loop {
                let line = output.recv().await.unwrap();
                assert!(!line.starts_with("info string ERROR"), "{line}");
                if let Some((_, pv)) = line.split_once(" pv ") {
                    let mut variation = Board::new().unwrap();
                    variation.set_from_fen(&board.get_fen().unwrap()).unwrap();
                    for mv in pv.split_whitespace() {
                        assert!(
                            variation.is_valid_move(mv).unwrap(),
                            "illegal PV {line} at ply {ply}"
                        );
                        variation.make_move(mv).unwrap();
                    }
                }
                if let Some(best) = line.strip_prefix("bestmove ") {
                    break best.to_owned();
                }
            }
        })
        .await
        .expect("depth two must finish within the external bound");
        assert!(
            board.is_valid_move(&best).unwrap(),
            "illegal bestmove {best} at ply {ply}"
        );
        board.make_move(&best).unwrap();
        moves.push(best);
    }
    assert!(moves.len() >= 2);
    engine.process_command("quit").await.unwrap();
}

#[test]
fn special_moves_update_the_whole_board() {
    let mut board = Board::new().unwrap();
    board
        .set_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1")
        .unwrap();
    board.make_move("e1g1").unwrap();
    assert!(board
        .get_fen()
        .unwrap()
        .starts_with("r3k2r/8/8/8/8/8/8/R4RK1 b kq"));
    board
        .set_from_fen("7k/8/8/3pP3/8/8/8/7K w - d6 0 1")
        .unwrap();
    board.make_move("e5d6").unwrap();
    assert!(board
        .get_fen()
        .unwrap()
        .starts_with("7k/8/3P4/8/8/8/8/7K b - -"));
    board.set_from_fen("7k/P7/8/8/8/8/8/7K w - - 0 1").unwrap();
    board.make_move("a7a8n").unwrap();
    assert!(board.get_fen().unwrap().starts_with("N6k/8/8/8/8/8/8/7K b"));
}
