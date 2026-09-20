// Safe Rust wrapper for C++ SearchEngine FFI integration
//
// This module provides a safe, ergonomic Rust interface for the C++ SearchEngine,
// with async support, comprehensive error handling, and atomic stop flag integration.

#![allow(clippy::missing_docs_in_private_items)]

use crate::bridge::Board;
use crate::error::{UCIError, UCIResult};
use crate::ffi::ffi;
use cxx::{let_cxx_string, UniquePtr};
use std::fmt;
use std::time::Duration;
use tracing::{debug, error, info, instrument, warn};

/// Search limits for configuring search depth, time, and nodes
#[derive(Debug, Clone, Copy)]
pub struct SearchLimits {
    /// Maximum search depth (default: 64, unlimited)
    pub max_depth: i32,
    /// Maximum nodes to search (default: u64::MAX, unlimited)
    pub max_nodes: u64,
    /// Maximum time in milliseconds (default: u64::MAX, unlimited)
    pub max_time_ms: u64,
    /// Infinite search (search until stopped)
    pub infinite: bool,
}

impl Default for SearchLimits {
    fn default() -> Self {
        Self {
            max_depth: 64,
            max_nodes: u64::MAX,
            max_time_ms: u64::MAX,
            infinite: false,
        }
    }
}

impl SearchLimits {
    /// Create limits for depth-limited search
    pub fn depth(depth: i32) -> Self {
        Self {
            max_depth: depth,
            ..Default::default()
        }
    }

    /// Create limits for time-limited search
    pub fn time(duration: Duration) -> Self {
        Self {
            max_time_ms: duration.as_millis() as u64,
            ..Default::default()
        }
    }

    /// Create limits for node-limited search
    pub fn nodes(nodes: u64) -> Self {
        Self {
            max_nodes: nodes,
            ..Default::default()
        }
    }

    /// Create infinite search limits (search until stopped)
    pub fn infinite() -> Self {
        Self {
            infinite: true,
            ..Default::default()
        }
    }

    /// Convert to FFI-compatible limits
    fn to_ffi(&self) -> ffi::FFISearchLimits {
        ffi::FFISearchLimits {
            max_depth: self.max_depth,
            max_nodes: self.max_nodes,
            max_time_ms: self.max_time_ms,
            infinite: self.infinite,
        }
    }
}

/// Search result containing best move, score, and search statistics
#[derive(Debug, Clone)]
pub struct SearchResult {
    /// Best move in UCI notation (e.g., "e2e4")
    pub best_move: String,
    /// Ponder move (move to think about during opponent's time)
    pub ponder_move: String,
    /// Evaluation score in centipawns from the root side-to-move's perspective.
    pub score: i32,
    /// Depth reached during search
    pub depth: i32,
    /// Total nodes searched
    pub nodes: u64,
    /// Time taken in milliseconds
    pub time_ms: u64,
    /// Principal variation (best line) as vector of UCI moves
    pub principal_variation: Vec<String>,
}

impl SearchResult {
    /// Create SearchResult from FFI result
    fn from_ffi(ffi_result: ffi::FFISearchResult) -> Self {
        // Parse PV string into vector of moves
        let pv = if ffi_result.pv.is_empty() {
            Vec::new()
        } else {
            ffi_result
                .pv
                .split_whitespace()
                .map(|s| s.to_string())
                .collect()
        };

        Self {
            best_move: ffi_result.best_move.to_string(),
            ponder_move: ffi_result.ponder_move.to_string(),
            score: ffi_result.score,
            depth: ffi_result.depth,
            nodes: ffi_result.nodes,
            time_ms: ffi_result.time_ms,
            principal_variation: pv,
        }
    }

    /// Check if this is a valid search result (not an error result)
    pub fn is_valid(&self) -> bool {
        // "0000" is the null move indicator used for error results
        self.best_move != "0000"
    }

    /// Get nodes per second
    pub fn nps(&self) -> u64 {
        if self.time_ms > 0 {
            (self.nodes * 1000) / self.time_ms
        } else {
            0
        }
    }
}

impl fmt::Display for SearchResult {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "bestmove {} ponder {} score {} depth {} nodes {} time {}ms nps {}",
            self.best_move,
            self.ponder_move,
            self.score,
            self.depth,
            self.nodes,
            self.time_ms,
            self.nps()
        )
    }
}

/// Safe wrapper around the C++ SearchEngine with async support
///
/// This wrapper ensures proper lifecycle management, provides atomic stop flag
/// integration for async cancellation, and offers ergonomic Rust APIs.
pub struct SearchEngine {
    /// The underlying C++ SearchEngineWrapper instance
    /// The wrapper owns a board snapshot; no caller-owned reference is retained.
    inner: UniquePtr<ffi::SearchEngineWrapper>,
    /// Last search result (cached for retrieval)
    last_result: Option<SearchResult>,
}

impl SearchEngine {
    /// Create a new SearchEngine for the given board
    ///
    /// # Arguments
    ///
    /// * `board` - Mutable reference to the Board to search on
    ///
    /// # Returns
    ///
    /// - `Ok(SearchEngine)` - Successfully created search engine
    /// - `Err(UCIError::Ffi)` - Failed to create C++ SearchEngine instance
    ///
    /// # Safety
    ///
    /// The C++ wrapper copies the board and its history at construction. Subsequent
    /// changes to the original board do not affect this search instance.
    ///
    /// # Examples
    ///
    /// ```
    /// use opera_uci::bridge::{Board, SearchEngine, SearchLimits};
    ///
    /// let mut board = Board::new()?;
    /// let mut engine = SearchEngine::new(&mut board)?;
    /// let result = engine.search(SearchLimits::depth(5))?;
    /// # Ok::<(), opera_uci::UCIError>(())
    /// ```
    #[instrument(level = "debug", skip(board))]
    pub fn new(board: &mut Board) -> UCIResult<Self> {
        debug!("Creating new SearchEngine");

        let inner = ffi::create_search_engine(board.inner_mut());
        if inner.is_null() {
            error!("Failed to create C++ SearchEngine instance - null pointer returned");
            return Err(UCIError::Ffi {
                message: "Failed to create C++ SearchEngine instance".to_string(),
            });
        }

        debug!("Successfully created SearchEngine");
        Ok(SearchEngine {
            inner,
            last_result: None,
        })
    }

    /// Perform a synchronous blocking search
    ///
    /// # Arguments
    ///
    /// * `limits` - Search limits (depth, time, nodes, infinite)
    ///
    /// # Returns
    ///
    /// - `Ok(SearchResult)` - Search completed successfully
    /// - `Err(UCIError::Search)` - Search failed or was stopped
    /// - `Err(UCIError::Ffi)` - FFI communication error
    ///
    /// # Examples
    ///
    /// ```
    /// # use opera_uci::bridge::{Board, SearchEngine, SearchLimits};
    /// # let mut board = Board::new()?;
    /// # let mut engine = SearchEngine::new(&mut board)?;
    /// let result = engine.search(SearchLimits::depth(3))?;
    /// println!("Best move: {}", result.best_move);
    /// # Ok::<(), opera_uci::UCIError>(())
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn search(&mut self, limits: SearchLimits) -> UCIResult<SearchResult> {
        info!(
            max_depth = limits.max_depth,
            max_time_ms = limits.max_time_ms,
            max_nodes = limits.max_nodes,
            infinite = limits.infinite,
            "Starting search"
        );

        let ffi_limits = limits.to_ffi();

        // Execute blocking search with output parameter
        let mut ffi_result = ffi::FFISearchResult {
            best_move: String::new(),
            ponder_move: String::new(),
            score: 0,
            depth: 0,
            nodes: 0,
            time_ms: 0,
            pv: String::new(),
        };
        ffi::search_engine_search(self.inner.pin_mut(), &ffi_limits, &mut ffi_result);

        let result = SearchResult::from_ffi(ffi_result);

        // 0000 is a normal result for mate/stalemate, not an FFI failure.
        info!(
            best_move = %result.best_move,
            score = result.score,
            depth = result.depth,
            nodes = result.nodes,
            nps = result.nps(),
            "Search completed"
        );

        self.last_result = Some(result.clone());
        Ok(result)
    }

    /// Search with a separately owned cancellation/progress channel.
    pub fn search_controlled(
        &mut self,
        limits: SearchLimits,
        control: &crate::uci::search_session::SearchControl,
        hash_mb: u32,
        morphy: bool,
        roots: &str,
    ) -> UCIResult<SearchResult> {
        let mut result = ffi::FFISearchResult {
            best_move: String::new(),
            ponder_move: String::new(),
            score: 0,
            depth: 0,
            nodes: 0,
            time_ms: 0,
            pv: String::new(),
        };
        ffi::search_engine_controlled(
            self.inner.pin_mut(),
            &limits.to_ffi(),
            control,
            hash_mb,
            morphy,
            roots,
            &mut result,
        )
        .map_err(|e| UCIError::Search {
            message: e.to_string(),
        })?;
        Ok(SearchResult::from_ffi(result))
    }

    /// Stop the current search immediately
    ///
    /// This sets the atomic stop flag and signals the C++ search engine
    /// to terminate the search as soon as possible. The search will return
    /// the best move found so far.
    ///
    /// # Examples
    ///
    /// ```
    /// # use opera_uci::bridge::{Board, SearchEngine, SearchLimits};
    /// # let mut board = Board::new()?;
    /// # let mut engine = SearchEngine::new(&mut board)?;
    /// engine.stop();
    /// # Ok::<(), opera_uci::UCIError>(())
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn stop(&mut self) {
        debug!("Stopping search");
        ffi::search_engine_stop(self.inner.pin_mut());
        info!("Search stop signal sent");
    }

    /// Check if the engine is currently searching
    ///
    /// # Returns
    ///
    /// - `true` - Engine is currently searching
    /// - `false` - Engine is idle
    ///
    /// # Examples
    ///
    /// ```
    /// # use opera_uci::bridge::{Board, SearchEngine, SearchLimits};
    /// # let mut board = Board::new()?;
    /// # let mut engine = SearchEngine::new(&mut board)?;
    /// if engine.is_searching() {
    ///     engine.stop();
    /// }
    /// # Ok::<(), opera_uci::UCIError>(())
    /// ```
    pub fn is_searching(&self) -> bool {
        ffi::search_engine_is_searching(&*self.inner)
    }

    /// Get the last search result without performing a new search
    ///
    /// # Returns
    ///
    /// - `Some(SearchResult)` - Last search result
    /// - `None` - No search has been performed yet
    ///
    /// # Examples
    ///
    /// ```
    /// # use opera_uci::bridge::{Board, SearchEngine, SearchLimits};
    /// # let mut board = Board::new()?;
    /// # let mut engine = SearchEngine::new(&mut board)?;
    /// if let Some(result) = engine.get_last_result() {
    ///     println!("Last best move: {}", result.best_move);
    /// }
    /// # Ok::<(), opera_uci::UCIError>(())
    /// ```
    pub fn get_last_result(&self) -> Option<&SearchResult> {
        self.last_result.as_ref()
    }

    /// Reset the search engine statistics for a new game
    ///
    /// This resets reported search statistics and the cached result. UCI
    /// `ucinewgame` separately recreates the worker search state.
    ///
    /// # Examples
    ///
    /// ```
    /// # use opera_uci::bridge::{Board, SearchEngine, SearchLimits};
    /// # let mut board = Board::new()?;
    /// # let mut engine = SearchEngine::new(&mut board)?;
    /// engine.reset();
    /// # Ok::<(), opera_uci::UCIError>(())
    /// ```
    #[instrument(level = "debug", skip(self))]
    pub fn reset(&mut self) {
        debug!("Resetting search engine");
        ffi::search_engine_reset(self.inner.pin_mut());
        self.last_result = None;
        info!("Search engine reset complete");
    }
}

impl fmt::Debug for SearchEngine {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("SearchEngine")
            .field("is_searching", &self.is_searching())
            .field("last_result", &self.last_result)
            .finish()
    }
}

// Safety: UniquePtr exclusively owns a C++ wrapper, its board snapshot, stop flag,
// and all mutable search data. Moving ownership does not share references. The
// wrapper is deliberately not Sync; control uses a separate atomic Rust token.
#[allow(unsafe_code)] // Required for FFI Send implementation
unsafe impl Send for SearchEngine {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_search_limits_defaults() {
        let limits = SearchLimits::default();
        assert_eq!(limits.max_depth, 64);
        assert_eq!(limits.max_nodes, u64::MAX);
        assert_eq!(limits.max_time_ms, u64::MAX);
        assert!(!limits.infinite);
    }

    #[test]
    fn test_search_limits_depth() {
        let limits = SearchLimits::depth(10);
        assert_eq!(limits.max_depth, 10);
        assert!(!limits.infinite);
    }

    #[test]
    fn test_search_limits_time() {
        let limits = SearchLimits::time(Duration::from_secs(5));
        assert_eq!(limits.max_time_ms, 5000);
    }

    #[test]
    fn test_search_limits_nodes() {
        let limits = SearchLimits::nodes(1000000);
        assert_eq!(limits.max_nodes, 1000000);
    }

    #[test]
    fn test_search_limits_infinite() {
        let limits = SearchLimits::infinite();
        assert!(limits.infinite);
    }

    #[test]
    fn test_search_result_validity() {
        let valid_result = SearchResult {
            best_move: "e2e4".to_string(),
            ponder_move: "e7e5".to_string(),
            score: 50,
            depth: 10,
            nodes: 100000,
            time_ms: 1000,
            principal_variation: vec!["e2e4".to_string(), "e7e5".to_string()],
        };
        assert!(valid_result.is_valid());

        let invalid_result = SearchResult {
            best_move: "0000".to_string(),
            ponder_move: "".to_string(),
            score: 0,
            depth: 0,
            nodes: 0,
            time_ms: 0,
            principal_variation: vec![],
        };
        assert!(!invalid_result.is_valid());
    }

    #[test]
    fn test_search_result_nps() {
        let result = SearchResult {
            best_move: "e2e4".to_string(),
            ponder_move: "e7e5".to_string(),
            score: 50,
            depth: 10,
            nodes: 100000,
            time_ms: 1000,
            principal_variation: vec![],
        };
        assert_eq!(result.nps(), 100000);

        let zero_time_result = SearchResult {
            best_move: "e2e4".to_string(),
            ponder_move: "e7e5".to_string(),
            score: 50,
            depth: 10,
            nodes: 100000,
            time_ms: 0,
            principal_variation: vec![],
        };
        assert_eq!(zero_time_result.nps(), 0);
    }

    #[test]
    fn test_search_result_from_ffi() {
        let ffi_result = ffi::FFISearchResult {
            best_move: "e2e4".to_string(),
            ponder_move: "e7e5".to_string(),
            score: 50,
            depth: 10,
            nodes: 100000,
            time_ms: 1000,
            pv: "e2e4 e7e5 g1f3 b8c6".to_string(),
        };

        let result = SearchResult::from_ffi(ffi_result);
        assert_eq!(result.best_move, "e2e4");
        assert_eq!(result.ponder_move, "e7e5");
        assert_eq!(result.score, 50);
        assert_eq!(result.depth, 10);
        assert_eq!(result.nodes, 100000);
        assert_eq!(result.time_ms, 1000);
        assert_eq!(result.principal_variation.len(), 4);
        assert_eq!(result.principal_variation[0], "e2e4");
        assert_eq!(result.principal_variation[3], "b8c6");
    }
}
