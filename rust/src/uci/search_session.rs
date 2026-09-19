//! Owns one search job and its independent, race-free cancellation channel.
use super::{
    commands::{ChessMove, Position, TimeControl},
    state::{SearchContext, UCIState},
};
use crate::{
    bridge::{Board, SearchEngine, SearchLimits},
    error::{UCIError, UCIResult},
};
use std::sync::{
    atomic::{AtomicBool, AtomicU64, Ordering},
    Arc,
};
use std::time::{Duration, Instant};
use tokio::sync::broadcast;

const START: &str = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
fn error(message: impl Into<String>) -> UCIError {
    UCIError::Protocol {
        message: message.into(),
    }
}

/// Shared control owns no C++ pointer. Only atomics cross the search/control boundary.
pub struct SearchControl {
    stop: AtomicBool,
    ponder: AtomicBool,
    infinite: bool,
    deadline: AtomicU64,
    start: Instant,
    budget: u64,
    output: broadcast::Sender<String>,
}
impl SearchControl {
    /// Called inside C++ recursive search, as well as between iterations.
    pub fn cancelled(&self) -> bool {
        self.stop.load(Ordering::Relaxed)
            || (!self.ponder.load(Ordering::Acquire)
                && !self.infinite
                && self.start.elapsed().as_millis()
                    >= self.deadline.load(Ordering::Relaxed) as u128)
    }
    /// Routes completed-depth information to the sole protocol output writer.
    pub fn report(&self, info: &crate::ffi::ffi::FFISearchInfo) {
        let _ = self.output.send(format!(
            "info depth {} score {} time {} nodes {} nps {} pv {}",
            info.depth,
            score(info.score),
            info.time_ms,
            info.nodes,
            info.nps,
            info.pv
        ));
    }
    fn hold(&self) -> bool {
        !self.stop.load(Ordering::Relaxed) && (self.infinite || self.ponder.load(Ordering::Relaxed))
    }
}
fn score(cp: i32) -> String {
    if cp.abs() >= 29000 {
        format!("mate {}", cp.signum() * ((30000 - cp.abs() + 1) / 2))
    } else {
        format!("cp {}", cp)
    }
}
struct Job {
    control: Arc<SearchControl>,
    task: tokio::task::JoinHandle<()>,
}
/// Mutable session data is serialized by the coordinator; C++ objects live only on a worker.
pub struct SearchSession {
    fen: String,
    initial_fen: String,
    history: Vec<String>,
    job: Option<Job>,
    pub hash_mb: u32,
    pub morphy: bool,
    pub overhead_ms: u64,
}
impl Default for SearchSession {
    fn default() -> Self {
        Self {
            fen: START.into(),
            initial_fen: START.into(),
            history: Vec::new(),
            job: None,
            hash_mb: 16,
            morphy: false,
            overhead_ms: 10,
        }
    }
}
impl Drop for SearchSession {
    fn drop(&mut self) {
        if let Some(job) = &self.job {
            job.control.stop.store(true, Ordering::Relaxed);
        }
    }
}
impl SearchSession {
    pub async fn stop(&mut self) -> UCIResult<()> {
        if let Some(job) = self.job.take() {
            job.control.stop.store(true, Ordering::Relaxed);
            job.task
                .await
                .map_err(|e| error(format!("search worker: {e}")))?;
        }
        Ok(())
    }
    pub fn ponderhit(&self) {
        if let Some(job) = &self.job {
            let c = &job.control;
            if !c.ponder.load(Ordering::Relaxed) {
                return;
            }
            c.deadline.store(
                (c.start.elapsed().as_millis() as u64).saturating_add(c.budget),
                Ordering::Relaxed,
            );
            // Publish the new deadline before the worker leaves ponder mode.
            c.ponder.store(false, Ordering::Release);
        }
    }
    pub async fn new_game(&mut self) -> UCIResult<()> {
        self.stop().await?;
        self.fen = START.into();
        self.initial_fen = START.into();
        self.history.clear();
        Ok(())
    }
    pub async fn position(
        &mut self,
        position: Position<'_>,
        moves: Vec<ChessMove<'_>>,
    ) -> UCIResult<()> {
        let fen = match position {
            Position::StartPos => START,
            Position::Fen(f) => f,
        };
        let normalized = fen.split_whitespace().collect::<Vec<_>>().join(" ");
        let fen = normalized.as_str();
        validate_fen(fen)?;
        let history: Vec<String> = moves
            .iter()
            .map(|m| m.to_string().to_ascii_lowercase())
            .collect();
        let candidate = {
            let mut board = Board::new()?;
            board.set_from_fen(fen)?;
            if crate::ffi::ffi::board_previous_side_in_check(board.inner()) {
                return Err(error("side not to move is in check"));
            }
            for mv in &history {
                board.make_move(mv)?;
            }
            board.get_fen()?
        };
        // Do not disturb the previous position/search unless the whole command is valid.
        self.stop().await?;
        self.fen = candidate;
        self.initial_fen = fen.into();
        self.history = history;
        Ok(())
    }
    pub async fn go(
        &mut self,
        time: TimeControl,
        state: Arc<UCIState>,
        output: broadcast::Sender<String>,
    ) -> UCIResult<()> {
        if time.depth == Some(0)
            || time.depth.map_or(false, |d| d > 128)
            || time.mate == Some(0)
            || time.mate.map_or(false, |d| d > 64)
            || time.moves_to_go == Some(0)
        {
            return Err(error("invalid depth/mate/movestogo limit"));
        }
        {
            let mut validation = Board::new()?;
            validation.set_from_fen(&self.fen)?;
            for mv in &time.search_moves {
                if !validation.is_valid_move(mv)? {
                    return Err(error(format!("illegal searchmoves move {mv}")));
                }
            }
        }
        self.stop().await?;
        let white = self.fen.split_whitespace().nth(1) == Some("w");
        let remaining = if white {
            time.white_time_ms
        } else {
            time.black_time_ms
        };
        let inc = if white {
            time.white_increment_ms
        } else {
            time.black_increment_ms
        }
        .unwrap_or(0);
        let budget = if let Some(ms) = time.move_time_ms {
            ms.saturating_sub(self.overhead_ms).max(1)
        } else if let Some(ms) = remaining {
            (ms / u64::from(time.moves_to_go.unwrap_or(30)))
                .saturating_add(inc.saturating_mul(3) / 4)
                .min(ms.saturating_sub(self.overhead_ms))
                .max(1)
        } else {
            u64::MAX
        };
        let control = Arc::new(SearchControl {
            stop: AtomicBool::new(false),
            ponder: AtomicBool::new(time.ponder),
            infinite: time.infinite,
            deadline: AtomicU64::new(budget),
            start: Instant::now(),
            budget,
            output: output.clone(),
        });
        state.start_search(SearchContext {
            start_time: Instant::now(),
            time_control: time.clone(),
            max_depth: time.depth,
            max_nodes: time.nodes,
            is_infinite: time.infinite,
            is_ponder: time.ponder,
        })?;
        let limits = SearchLimits {
            max_depth: time
                .depth
                .unwrap_or(128)
                .min(time.mate.map_or(128, |m| m * 2)) as i32,
            max_nodes: time.nodes.unwrap_or(u64::MAX),
            max_time_ms: u64::MAX,
            // The session owns infinite/ponder output holding and clock policy.
            // C++ must still honor the requested node and depth bounds.
            infinite: false,
        };
        let fen = self.initial_fen.clone();
        let history = self.history.clone();
        let hash = self.hash_mb;
        let morphy = self.morphy;
        let roots = time.search_moves.join(" ");
        let send_ponder = state.config().ponder_enabled;
        let worker_control = control.clone();
        let task = tokio::task::spawn_blocking(move || {
            let c = worker_control;
            let result = (|| {
                let mut board = Board::new()?;
                board.set_from_fen(&fen)?;
                for mv in history {
                    board.make_move(&mv)?;
                }
                let mut engine = SearchEngine::new(&mut board)?;
                engine.search_controlled(limits, &c, hash, morphy, &roots)
            })();
            while result.is_ok() && c.hold() {
                std::thread::sleep(Duration::from_millis(1));
            }
            match result {
                Ok(r) => {
                    let _ = output.send(format!(
                        "info depth {} score {} time {} nodes {} nps {} pv {}",
                        r.depth,
                        score(r.score),
                        r.time_ms,
                        r.nodes,
                        r.nps(),
                        r.principal_variation.join(" ")
                    ));
                    let _ = state.complete_search(r.nodes);
                    let mut bestmove = format!("bestmove {}", r.best_move);
                    if send_ponder && r.principal_variation.first() == Some(&r.best_move) {
                        if let Some(reply) = r.principal_variation.get(1) {
                            bestmove.push_str(&format!(" ponder {reply}"));
                        }
                    }
                    let _ = output.send(bestmove);
                }
                Err(e) => {
                    let _ = state.complete_search(0);
                    let _ = output.send(format!("info string ERROR: {e}"));
                    let _ = output.send("bestmove 0000".into());
                }
            }
        });
        self.job = Some(Job { control, task });
        Ok(())
    }
}

/// Validate structure before entering the permissive C++ FEN parser.
fn validate_fen(fen: &str) -> UCIResult<()> {
    Position::validate_fen(fen)?;
    let fields: Vec<_> = fen.split_whitespace().collect();
    let mut kings = [None, None];
    let mut squares = [[' '; 8]; 8];
    for (rank, row) in fields[0].split('/').enumerate() {
        let mut file = 0usize;
        for c in row.chars() {
            if ('1'..='8').contains(&c) {
                file += c.to_digit(10).unwrap_or(0) as usize;
            } else if "PNBRQKpnbrqk".contains(c) {
                if file >= 8 {
                    return Err(error("FEN rank overflow"));
                }
                squares[rank][file] = c;
                if c == 'K' || c == 'k' {
                    let i = if c == 'K' { 0 } else { 1 };
                    if kings[i].is_some() {
                        return Err(error("multiple kings"));
                    }
                    kings[i] = Some((rank as i32, file as i32));
                }
                if (rank == 0 || rank == 7) && (c == 'P' || c == 'p') {
                    return Err(error("pawn on promotion rank"));
                }
                file += 1;
            } else {
                return Err(error("invalid FEN piece"));
            }
        }
        if file != 8 {
            return Err(error("FEN rank must contain eight squares"));
        }
    }
    match (kings[0], kings[1]) {
        (Some((r, f)), Some((r2, f2))) if (r - r2).abs() > 1 || (f - f2).abs() > 1 => (),
        _ => return Err(error("FEN requires two nonadjacent kings")),
    }
    let mut rights = std::collections::HashSet::new();
    for c in fields[2].chars().filter(|c| *c != '-') {
        if !rights.insert(c) {
            return Err(error("duplicate castling right"));
        }
        let (rank, rook, king, corner) = match c {
            'K' => (7, 'R', 'K', 7),
            'Q' => (7, 'R', 'K', 0),
            'k' => (0, 'r', 'k', 7),
            _ => (0, 'r', 'k', 0),
        };
        if squares[rank][4] != king || squares[rank][corner] != rook {
            return Err(error("castling right without king and rook"));
        }
    }
    if fields[4].parse::<u16>().is_err() || fields[5].parse::<u16>().map_or(true, |n| n == 0) {
        return Err(error("invalid FEN counters"));
    }
    if fields[3] != "-" && !matches!(fields[3].as_bytes()[1], b'3' | b'6') {
        return Err(error("invalid en passant rank"));
    }
    if fields[3] != "-" {
        let file = (fields[3].as_bytes()[0].to_ascii_lowercase() - b'a') as usize;
        let (target, pawn, piece, rank) = if fields[1] == "w" {
            (2, 3, 'p', b'6')
        } else {
            (5, 4, 'P', b'3')
        };
        if fields[3].as_bytes()[1] != rank
            || squares[target][file] != ' '
            || squares[pawn][file] != piece
        {
            return Err(error("inconsistent en passant target"));
        }
    }
    Ok(())
}
