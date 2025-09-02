// Basic UCI Command Handlers (uci, isready, quit)
//
// This module implements the fundamental UCI handshake commands that every UCI engine
// must support for proper GUI integration and protocol compliance.

use std::sync::Arc;
use tracing::{debug, info, instrument};

use crate::error::{UCIError, UCIResult};
use crate::uci::{EngineIdentification, UCIState};
use crate::uci::state::EngineState;
use crate::{uci_options, NAME, AUTHOR, VERSION};

/// Response generator for basic UCI commands
pub struct BasicCommandHandler {
    /// Engine identification information
    id_info: EngineIdentification,
    
    /// Reference to engine state for status queries
    state: Arc<UCIState>,
}

impl BasicCommandHandler {
    /// Create a new basic command handler
    pub fn new(id_info: EngineIdentification, state: Arc<UCIState>) -> Self {
        Self { id_info, state }
    }

    /// Handle the 'uci' command - engine identification and option registration
    ///
    /// The UCI specification requires engines to respond with:
    /// 1. id name <engine name>
    /// 2. id author <author name>
    /// 3. option name <option> type <type> [default <value>] [min <value>] [max <value>]
    /// 4. uciok
    #[instrument(skip(self))]
    pub fn handle_uci_command(&self) -> UCIResult<Vec<String>> {
        info!("Processing UCI identification command");
        
        let mut responses = Vec::new();
        
        // Engine identification
        responses.push(format!("id name {}", self.id_info.name));
        responses.push(format!("id author {}", self.id_info.author));
        
        debug!("Generated engine identification responses");
        
        // UCI option registration
        for (name, option_type, default_value) in uci_options::OPTIONS {
            let response = match *option_type {
                "spin" => {
                    // For spin options, we need min/max values
                    let (min, max) = self.get_spin_option_range(name);
                    format!(
                        "option name {} type spin default {} min {} max {}",
                        name, default_value, min, max
                    )
                }
                "check" => {
                    format!("option name {} type check default {}", name, default_value)
                }
                "string" => {
                    format!("option name {} type string default {}", name, default_value)
                }
                _ => {
                    return Err(UCIError::Configuration {
                        message: format!("Unknown option type '{}' for option '{}'", option_type, name),
                    });
                }
            };
            responses.push(response);
        }
        
        debug!("Generated {} UCI option responses", uci_options::OPTIONS.len());
        
        // Final UCI handshake completion
        responses.push("uciok".to_string());
        
        info!(
            responses_count = responses.len(),
            "UCI command processed successfully"
        );
        
        Ok(responses)
    }

    /// Handle the 'isready' command - engine readiness check
    ///
    /// This command tests whether the engine is ready to receive new commands.
    /// The engine should respond with 'readyok' when it's ready.
    #[instrument(skip(self))]
    pub fn handle_isready_command(&self) -> UCIResult<Vec<String>> {
        debug!("Processing isready command");
        
        // Check if engine is in a state where it can accept commands
        let current_state = self.state.current_state();
        let can_accept_commands = matches!(
            current_state,
            EngineState::Ready | EngineState::Searching
        );
        
        if !can_accept_commands {
            info!(
                current_state = ?current_state,
                "Engine not ready - still initializing"
            );
            
            return Err(UCIError::Protocol {
                message: format!(
                    "Engine not ready for commands (current state: {:?})",
                    current_state
                ),
            });
        }
        
        debug!("Engine is ready for commands");
        Ok(vec!["readyok".to_string()])
    }

    /// Handle the 'quit' command - graceful engine shutdown
    ///
    /// This command instructs the engine to shut down cleanly.
    /// No response is expected, but we return empty responses for consistency.
    #[instrument(skip(self))]
    pub fn handle_quit_command(&self) -> UCIResult<Vec<String>> {
        info!("Processing quit command - preparing for graceful shutdown");
        
        // Mark engine as shutting down if possible
        // Note: We don't change state here as the main loop will handle shutdown
        debug!("Quit command processed - engine will shut down");
        
        // No response expected for quit command
        Ok(vec![])
    }

    /// Get the valid range for spin options
    ///
    /// Returns (min, max) values for different spin option types
    fn get_spin_option_range(&self, option_name: &str) -> (i32, i32) {
        match option_name {
            "Hash" => (1, 8192), // Hash size in MB: 1MB to 8GB
            "Threads" => (1, 64), // Thread count: 1 to 64 threads
            "SacrificeThreshold" => (0, 500), // Centipawn threshold for sacrifices
            "TacticalDepth" => (0, 10), // Extra depth for tactical sequences
            _ => (0, 100), // Default range for unknown options
        }
    }
}

impl Default for BasicCommandHandler {
    fn default() -> Self {
        let id_info = EngineIdentification {
            name: NAME.to_string(),
            author: AUTHOR.to_string(),
            version: VERSION.to_string(),
        };
        
        // Create a default state for testing/standalone use
        let state = Arc::new(UCIState::new());
        
        Self::new(id_info, state)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Helper to create handler with test state
    fn create_test_handler() -> BasicCommandHandler {
        let id_info = EngineIdentification {
            name: "Test Engine".to_string(),
            author: "Test Author".to_string(),
            version: "1.0.0".to_string(),
        };
        let state = Arc::new(UCIState::new());
        // Transition to Ready state for testing
        state.transition_to(EngineState::Ready, "Test initialization").expect("Failed to set ready state");
        BasicCommandHandler::new(id_info, state)
    }

    #[test]
    fn test_uci_command_response() {
        let handler = create_test_handler();
        let responses = handler.handle_uci_command().expect("UCI command should succeed");
        
        // Should have identification + options + uciok
        assert!(!responses.is_empty());
        assert!(responses.len() >= 3); // At least id name, id author, uciok
        
        // Check identification responses
        assert!(responses.iter().any(|r| r.starts_with("id name")));
        assert!(responses.iter().any(|r| r.starts_with("id author")));
        
        // Check final response
        assert_eq!(responses.last().unwrap(), "uciok");
        
        // Check option responses are present
        let option_responses: Vec<_> = responses.iter()
            .filter(|r| r.starts_with("option"))
            .collect();
        assert_eq!(option_responses.len(), uci_options::OPTIONS.len());
        
        // Verify specific options exist
        assert!(responses.iter().any(|r| r.contains("option name Hash")));
        assert!(responses.iter().any(|r| r.contains("option name MorphyStyle")));
    }

    #[test]
    fn test_isready_command_when_ready() {
        let handler = create_test_handler();
        let responses = handler.handle_isready_command().expect("isready should succeed");
        
        assert_eq!(responses.len(), 1);
        assert_eq!(responses[0], "readyok");
    }

    #[test]
    fn test_isready_command_when_initializing() {
        let id_info = EngineIdentification {
            name: "Test Engine".to_string(),
            author: "Test Author".to_string(),
            version: "1.0.0".to_string(),
        };
        let state = Arc::new(UCIState::new());
        // Leave in Initializing state
        let handler = BasicCommandHandler::new(id_info, state);
        
        let result = handler.handle_isready_command();
        assert!(result.is_err());
        
        if let Err(UCIError::Protocol { message }) = result {
            assert!(message.contains("not ready"));
        } else {
            panic!("Expected Protocol error");
        }
    }

    #[test]
    fn test_quit_command_response() {
        let handler = create_test_handler();
        let responses = handler.handle_quit_command().expect("quit should succeed");
        
        // Quit command expects no response
        assert!(responses.is_empty());
    }

    #[test]
    fn test_spin_option_ranges() {
        let handler = create_test_handler();
        
        // Test known options
        assert_eq!(handler.get_spin_option_range("Hash"), (1, 8192));
        assert_eq!(handler.get_spin_option_range("Threads"), (1, 64));
        assert_eq!(handler.get_spin_option_range("SacrificeThreshold"), (0, 500));
        assert_eq!(handler.get_spin_option_range("TacticalDepth"), (0, 10));
        
        // Test unknown option
        assert_eq!(handler.get_spin_option_range("UnknownOption"), (0, 100));
    }

    #[test]
    fn test_option_formatting() {
        let handler = create_test_handler();
        let responses = handler.handle_uci_command().expect("UCI command should succeed");
        
        // Find Hash option
        let hash_option = responses.iter()
            .find(|r| r.contains("option name Hash"))
            .expect("Hash option should be present");
        
        // Should be properly formatted spin option
        assert!(hash_option.contains("type spin"));
        assert!(hash_option.contains("default 128"));
        assert!(hash_option.contains("min 1"));
        assert!(hash_option.contains("max 8192"));
        
        // Find MorphyStyle option
        let morphy_option = responses.iter()
            .find(|r| r.contains("option name MorphyStyle"))
            .expect("MorphyStyle option should be present");
        
        // Should be properly formatted check option
        assert!(morphy_option.contains("type check"));
        assert!(morphy_option.contains("default false"));
    }

    #[test]
    fn test_handler_creation() {
        let handler = BasicCommandHandler::default();
        
        // Should create handler with default values
        assert_eq!(handler.id_info.name, NAME);
        assert_eq!(handler.id_info.author, AUTHOR);
        assert_eq!(handler.id_info.version, VERSION);
    }

    #[test]
    fn test_all_uci_options_covered() {
        let handler = create_test_handler();
        let responses = handler.handle_uci_command().expect("UCI command should succeed");
        
        // Verify all options from uci_options::OPTIONS are included
        for (option_name, _, _) in uci_options::OPTIONS {
            assert!(
                responses.iter().any(|r| r.contains(&format!("option name {}", option_name))),
                "Option '{}' should be in UCI response",
                option_name
            );
        }
    }

    #[test]
    fn test_uci_response_order() {
        let handler = create_test_handler();
        let responses = handler.handle_uci_command().expect("UCI command should succeed");
        
        // First should be id name
        assert!(responses[0].starts_with("id name"));
        
        // Second should be id author
        assert!(responses[1].starts_with("id author"));
        
        // Last should be uciok
        assert_eq!(responses.last().unwrap(), "uciok");
        
        // Options should be between identification and uciok
        let option_start = 2;
        let option_end = responses.len() - 1;
        for i in option_start..option_end {
            assert!(responses[i].starts_with("option"), 
                   "Response at index {} should be an option: {}", i, responses[i]);
        }
    }
}