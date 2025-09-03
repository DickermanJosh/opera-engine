// UCI Response Formatting and Output System
//
// This module provides structured response generation for the UCI protocol,
// ensuring all responses are properly formatted and compliant with the specification.

use std::fmt;
use std::time::Duration;

use tracing::{debug, instrument};

use crate::error::UCIResult;

/// UCI response types with structured formatting
#[derive(Debug, Clone, PartialEq)]
pub enum UCIResponse {
    /// Engine identification response
    Id { name: String, author: String },

    /// UCI option declaration
    Option {
        name: String,
        option_type: OptionType,
        default: Option<String>,
        min: Option<i32>,
        max: Option<i32>,
    },

    /// Engine ready confirmation
    Ready,

    /// UCI handshake completion
    UciOk,

    /// Search information output
    Info {
        depth: Option<u8>,
        score: Option<i32>,
        time: Option<Duration>,
        nodes: Option<u64>,
        nps: Option<u64>,
        pv: Option<Vec<String>>,
        additional: Vec<InfoField>,
    },

    /// Best move result
    BestMove {
        best_move: String,
        ponder: Option<String>,
    },

    /// General information string
    InfoString { message: String },

    /// Error response (non-standard but useful for debugging)
    Error { message: String },
}

/// UCI option types
#[derive(Debug, Clone, PartialEq)]
pub enum OptionType {
    Check,
    Spin { min: i32, max: i32 },
    Combo { values: Vec<String> },
    Button,
    String,
}

/// Additional info fields for search information
#[derive(Debug, Clone, PartialEq)]
pub enum InfoField {
    SelDepth(u8),
    MultiPv(u8),
    CurrMove(String),
    CurrMoveNumber(u16),
    HashFull(u16),
    Tbhits(u64),
    Cpuload(u16),
    Refutation(Vec<String>),
    CurrLine(Vec<String>),
}

impl UCIResponse {
    /// Create an engine identification response
    pub fn id(name: impl Into<String>, author: impl Into<String>) -> Self {
        Self::Id {
            name: name.into(),
            author: author.into(),
        }
    }

    /// Create a spin option response
    pub fn spin_option(name: impl Into<String>, default: i32, min: i32, max: i32) -> Self {
        Self::Option {
            name: name.into(),
            option_type: OptionType::Spin { min, max },
            default: Some(default.to_string()),
            min: Some(min),
            max: Some(max),
        }
    }

    /// Create a check option response
    pub fn check_option(name: impl Into<String>, default: bool) -> Self {
        Self::Option {
            name: name.into(),
            option_type: OptionType::Check,
            default: Some(default.to_string()),
            min: None,
            max: None,
        }
    }

    /// Create a string option response
    pub fn string_option(name: impl Into<String>, default: impl Into<String>) -> Self {
        Self::Option {
            name: name.into(),
            option_type: OptionType::String,
            default: Some(default.into()),
            min: None,
            max: None,
        }
    }

    /// Create a ready response
    pub fn ready() -> Self {
        Self::Ready
    }

    /// Create a uciok response
    pub fn uciok() -> Self {
        Self::UciOk
    }

    /// Create a search info response
    pub fn info() -> InfoBuilder {
        InfoBuilder::new()
    }

    /// Create a best move response
    pub fn best_move(best_move: impl Into<String>) -> BestMoveBuilder {
        BestMoveBuilder::new(best_move.into())
    }

    /// Create an info string response
    pub fn info_string(message: impl Into<String>) -> Self {
        Self::InfoString {
            message: message.into(),
        }
    }

    /// Create an error response (for debugging)
    pub fn error(message: impl Into<String>) -> Self {
        Self::Error {
            message: message.into(),
        }
    }

    /// Convert response to UCI protocol string
    #[instrument(skip(self))]
    pub fn to_uci_string(&self) -> UCIResult<String> {
        let result = match self {
            UCIResponse::Id { name, author } => {
                format!("id name {}\nid author {}", name, author)
            }

            UCIResponse::Option {
                name,
                option_type,
                default,
                min: _,
                max: _,
            } => {
                let mut parts = vec![format!("option name {}", name)];

                match option_type {
                    OptionType::Check => {
                        parts.push("type check".to_string());
                        if let Some(def) = default {
                            parts.push(format!("default {}", def));
                        }
                    }
                    OptionType::Spin {
                        min: spin_min,
                        max: spin_max,
                    } => {
                        parts.push("type spin".to_string());
                        if let Some(def) = default {
                            parts.push(format!("default {}", def));
                        }
                        parts.push(format!("min {}", spin_min));
                        parts.push(format!("max {}", spin_max));
                    }
                    OptionType::String => {
                        parts.push("type string".to_string());
                        if let Some(def) = default {
                            parts.push(format!("default {}", def));
                        }
                    }
                    OptionType::Combo { values } => {
                        parts.push("type combo".to_string());
                        for value in values {
                            parts.push(format!("var {}", value));
                        }
                        if let Some(def) = default {
                            parts.push(format!("default {}", def));
                        }
                    }
                    OptionType::Button => {
                        parts.push("type button".to_string());
                    }
                }

                parts.join(" ")
            }

            UCIResponse::Ready => "readyok".to_string(),

            UCIResponse::UciOk => "uciok".to_string(),

            UCIResponse::Info {
                depth,
                score,
                time,
                nodes,
                nps,
                pv,
                additional,
            } => {
                let mut parts = vec!["info".to_string()];

                if let Some(d) = depth {
                    parts.push(format!("depth {}", d));
                }

                if let Some(s) = score {
                    parts.push(format!("score cp {}", s));
                }

                if let Some(t) = time {
                    parts.push(format!("time {}", t.as_millis()));
                }

                if let Some(n) = nodes {
                    parts.push(format!("nodes {}", n));
                }

                if let Some(nps_val) = nps {
                    parts.push(format!("nps {}", nps_val));
                }

                if let Some(pv_moves) = pv {
                    if !pv_moves.is_empty() {
                        parts.push(format!("pv {}", pv_moves.join(" ")));
                    }
                }

                // Add additional info fields
                for field in additional {
                    match field {
                        InfoField::SelDepth(sd) => parts.push(format!("seldepth {}", sd)),
                        InfoField::MultiPv(mpv) => parts.push(format!("multipv {}", mpv)),
                        InfoField::CurrMove(cm) => parts.push(format!("currmove {}", cm)),
                        InfoField::CurrMoveNumber(cmn) => {
                            parts.push(format!("currmovenumber {}", cmn))
                        }
                        InfoField::HashFull(hf) => parts.push(format!("hashfull {}", hf)),
                        InfoField::Tbhits(tb) => parts.push(format!("tbhits {}", tb)),
                        InfoField::Cpuload(cpu) => parts.push(format!("cpuload {}", cpu)),
                        InfoField::Refutation(ref_moves) => {
                            if !ref_moves.is_empty() {
                                parts.push(format!("refutation {}", ref_moves.join(" ")));
                            }
                        }
                        InfoField::CurrLine(line_moves) => {
                            if !line_moves.is_empty() {
                                parts.push(format!("currline {}", line_moves.join(" ")));
                            }
                        }
                    }
                }

                parts.join(" ")
            }

            UCIResponse::BestMove { best_move, ponder } => {
                let mut response = format!("bestmove {}", best_move);
                if let Some(ponder_move) = ponder {
                    response.push_str(&format!(" ponder {}", ponder_move));
                }
                response
            }

            UCIResponse::InfoString { message } => {
                format!("info string {}", message)
            }

            UCIResponse::Error { message } => {
                format!("info string ERROR: {}", message)
            }
        };

        debug!(response = %result, "Generated UCI response");
        Ok(result)
    }
}

impl fmt::Display for UCIResponse {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.to_uci_string() {
            Ok(s) => write!(f, "{}", s),
            Err(e) => write!(f, "ERROR: Failed to format UCI response: {}", e),
        }
    }
}

/// Builder for constructing info responses
pub struct InfoBuilder {
    depth: Option<u8>,
    score: Option<i32>,
    time: Option<Duration>,
    nodes: Option<u64>,
    nps: Option<u64>,
    pv: Option<Vec<String>>,
    additional: Vec<InfoField>,
}

impl InfoBuilder {
    pub fn new() -> Self {
        Self {
            depth: None,
            score: None,
            time: None,
            nodes: None,
            nps: None,
            pv: None,
            additional: Vec::new(),
        }
    }

    pub fn depth(mut self, depth: u8) -> Self {
        self.depth = Some(depth);
        self
    }

    pub fn score(mut self, score: i32) -> Self {
        self.score = Some(score);
        self
    }

    pub fn time(mut self, time: Duration) -> Self {
        self.time = Some(time);
        self
    }

    pub fn nodes(mut self, nodes: u64) -> Self {
        self.nodes = Some(nodes);
        self
    }

    pub fn nps(mut self, nps: u64) -> Self {
        self.nps = Some(nps);
        self
    }

    pub fn pv(mut self, moves: Vec<String>) -> Self {
        self.pv = Some(moves);
        self
    }

    pub fn seldepth(mut self, seldepth: u8) -> Self {
        self.additional.push(InfoField::SelDepth(seldepth));
        self
    }

    pub fn hashfull(mut self, hashfull: u16) -> Self {
        self.additional.push(InfoField::HashFull(hashfull));
        self
    }

    pub fn build(self) -> UCIResponse {
        UCIResponse::Info {
            depth: self.depth,
            score: self.score,
            time: self.time,
            nodes: self.nodes,
            nps: self.nps,
            pv: self.pv,
            additional: self.additional,
        }
    }
}

impl Default for InfoBuilder {
    fn default() -> Self {
        Self::new()
    }
}

/// Builder for constructing bestmove responses
pub struct BestMoveBuilder {
    best_move: String,
    ponder: Option<String>,
}

impl BestMoveBuilder {
    pub fn new(best_move: String) -> Self {
        Self {
            best_move,
            ponder: None,
        }
    }

    pub fn ponder(mut self, ponder_move: impl Into<String>) -> Self {
        self.ponder = Some(ponder_move.into());
        self
    }

    pub fn build(self) -> UCIResponse {
        UCIResponse::BestMove {
            best_move: self.best_move,
            ponder: self.ponder,
        }
    }
}

/// Response formatter for batch operations
pub struct ResponseFormatter;

impl ResponseFormatter {
    /// Format multiple responses as separate lines
    pub fn format_batch(responses: &[UCIResponse]) -> UCIResult<String> {
        let mut lines = Vec::new();

        for response in responses {
            let formatted = response.to_uci_string()?;
            // Split multi-line responses (like id responses)
            for line in formatted.lines() {
                lines.push(line.to_string());
            }
        }

        Ok(lines.join("\n"))
    }

    /// Format responses and send to output (for testing)
    pub fn format_and_collect(responses: &[UCIResponse]) -> UCIResult<Vec<String>> {
        let mut result = Vec::new();

        for response in responses {
            let formatted = response.to_uci_string()?;
            for line in formatted.lines() {
                result.push(line.to_string());
            }
        }

        Ok(result)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::Duration;

    #[test]
    fn test_id_response() {
        let response = UCIResponse::id("Test Engine", "Test Author");
        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert_eq!(formatted, "id name Test Engine\nid author Test Author");
    }

    #[test]
    fn test_spin_option_response() {
        let response = UCIResponse::spin_option("Hash", 128, 1, 8192);
        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert_eq!(
            formatted,
            "option name Hash type spin default 128 min 1 max 8192"
        );
    }

    #[test]
    fn test_check_option_response() {
        let response = UCIResponse::check_option("MorphyStyle", false);
        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert_eq!(
            formatted,
            "option name MorphyStyle type check default false"
        );
    }

    #[test]
    fn test_ready_response() {
        let response = UCIResponse::ready();
        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert_eq!(formatted, "readyok");
    }

    #[test]
    fn test_uciok_response() {
        let response = UCIResponse::uciok();
        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert_eq!(formatted, "uciok");
    }

    #[test]
    fn test_info_response_builder() {
        let response = UCIResponse::info()
            .depth(10)
            .score(150)
            .time(Duration::from_millis(5000))
            .nodes(100000)
            .nps(20000)
            .pv(vec!["e2e4".to_string(), "e7e5".to_string()])
            .build();

        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert!(formatted.contains("info"));
        assert!(formatted.contains("depth 10"));
        assert!(formatted.contains("score cp 150"));
        assert!(formatted.contains("time 5000"));
        assert!(formatted.contains("nodes 100000"));
        assert!(formatted.contains("nps 20000"));
        assert!(formatted.contains("pv e2e4 e7e5"));
    }

    #[test]
    fn test_bestmove_response() {
        let response = UCIResponse::best_move("e2e4").ponder("e7e5").build();

        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");
        assert_eq!(formatted, "bestmove e2e4 ponder e7e5");
    }

    #[test]
    fn test_bestmove_response_no_ponder() {
        let response = UCIResponse::best_move("e2e4").build();

        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");
        assert_eq!(formatted, "bestmove e2e4");
    }

    #[test]
    fn test_info_string_response() {
        let response = UCIResponse::info_string("Engine initialized successfully");
        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert_eq!(formatted, "info string Engine initialized successfully");
    }

    #[test]
    fn test_error_response() {
        let response = UCIResponse::error("Invalid FEN position");
        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert_eq!(formatted, "info string ERROR: Invalid FEN position");
    }

    #[test]
    fn test_response_formatter_batch() {
        let responses = vec![
            UCIResponse::id("Test Engine", "Test Author"),
            UCIResponse::ready(),
            UCIResponse::uciok(),
        ];

        let formatted =
            ResponseFormatter::format_batch(&responses).expect("Should format batch successfully");

        let lines: Vec<&str> = formatted.lines().collect();
        assert_eq!(lines.len(), 4); // id name, id author, readyok, uciok
        assert_eq!(lines[0], "id name Test Engine");
        assert_eq!(lines[1], "id author Test Author");
        assert_eq!(lines[2], "readyok");
        assert_eq!(lines[3], "uciok");
    }

    #[test]
    fn test_response_formatter_collect() {
        let responses = vec![
            UCIResponse::spin_option("Hash", 128, 1, 8192),
            UCIResponse::check_option("MorphyStyle", false),
        ];

        let collected = ResponseFormatter::format_and_collect(&responses)
            .expect("Should collect responses successfully");

        assert_eq!(collected.len(), 2);
        assert_eq!(
            collected[0],
            "option name Hash type spin default 128 min 1 max 8192"
        );
        assert_eq!(
            collected[1],
            "option name MorphyStyle type check default false"
        );
    }

    #[test]
    fn test_display_trait() {
        let response = UCIResponse::ready();
        let displayed = format!("{}", response);

        assert_eq!(displayed, "readyok");
    }

    #[test]
    fn test_info_with_additional_fields() {
        let response = UCIResponse::info()
            .depth(8)
            .score(75)
            .seldepth(12)
            .hashfull(500)
            .build();

        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert!(formatted.contains("depth 8"));
        assert!(formatted.contains("score cp 75"));
        assert!(formatted.contains("seldepth 12"));
        assert!(formatted.contains("hashfull 500"));
    }

    #[test]
    fn test_empty_info_response() {
        let response = UCIResponse::info().build();
        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert_eq!(formatted, "info");
    }

    #[test]
    fn test_string_option_response() {
        let response = UCIResponse::string_option("BookFile", "book.bin");
        let formatted = response
            .to_uci_string()
            .expect("Should format successfully");

        assert_eq!(
            formatted,
            "option name BookFile type string default book.bin"
        );
    }
}
