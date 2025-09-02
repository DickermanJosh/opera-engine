// UCI Handshake Integration Tests
//
// Comprehensive integration tests for UCI protocol basic command sequence,
// validating the complete handshake process and ensuring compliance with
// UCI specification requirements.

use std::time::Duration;
use tokio::time::timeout;
use tracing_test::traced_test;

use opera_uci::{UCIEngine, EngineIdentification, BasicCommandHandler, UCIResponse, ResponseFormatter, EngineState};

/// Test the complete UCI handshake sequence as specified in UCI protocol
#[tokio::test]
#[traced_test]
async fn test_complete_uci_handshake() {
    let engine = UCIEngine::new();
    let mut responses = engine.subscribe_responses();
    let mut state_changes = engine.subscribe_state_changes();
    
    // Initialize engine to ready state
    engine.initialize().await.expect("Engine initialization should succeed");
    
    // Wait for state transition to Ready
    let state_change = timeout(Duration::from_millis(100), state_changes.recv())
        .await
        .expect("Should receive state change")
        .expect("State change should be valid");
    assert_eq!(state_change.to, EngineState::Ready);
    
    // Step 1: Send 'uci' command
    engine.process_command("uci").await.expect("UCI command should succeed");
    
    // Collect all responses from uci command
    let mut uci_responses = Vec::new();
    let mut uciok_received = false;
    
    for _ in 0..20 { // Allow up to 20 responses (id + options + uciok)
        if let Ok(Ok(response)) = timeout(Duration::from_millis(50), responses.recv()).await {
            if response == "uciok" {
                uciok_received = true;
                uci_responses.push(response);
                break;
            } else {
                uci_responses.push(response);
            }
        } else {
            break; // No more responses
        }
    }
    
    // Validate UCI command response sequence
    assert!(!uci_responses.is_empty(), "UCI command should generate responses");
    assert!(uciok_received, "UCI command should end with 'uciok'");
    
    // Should have id name and id author at the beginning
    assert!(uci_responses.iter().any(|r| r.starts_with("id name")), "Should have id name response");
    assert!(uci_responses.iter().any(|r| r.starts_with("id author")), "Should have id author response");
    
    // Should have option declarations
    let option_responses: Vec<_> = uci_responses.iter().filter(|r| r.starts_with("option")).collect();
    assert!(!option_responses.is_empty(), "Should have at least one option declared");
    
    // Should end with uciok
    assert_eq!(uci_responses.last().unwrap(), "uciok", "Should end with uciok");
    
    // Step 2: Send 'isready' command
    engine.process_command("isready").await.expect("IsReady command should succeed");
    
    // Should receive 'readyok' response
    let ready_response = timeout(Duration::from_millis(100), responses.recv())
        .await
        .expect("Should receive ready response")
        .expect("Ready response should be valid");
    assert_eq!(ready_response, "readyok", "Should respond with readyok");
}

/// Test UCI identification responses match expected format
#[tokio::test]
#[traced_test]
async fn test_uci_identification() {
    let engine = UCIEngine::new();
    let mut responses = engine.subscribe_responses();
    
    engine.initialize().await.expect("Engine initialization should succeed");
    engine.process_command("uci").await.expect("UCI command should succeed");
    
    // Collect first few responses to check identification
    let mut id_responses = Vec::new();
    for _ in 0..10 {
        if let Ok(Ok(response)) = timeout(Duration::from_millis(10), responses.recv()).await {
            let is_uciok = response == "uciok";
            id_responses.push(response);
            if is_uciok {
                break;
            }
        } else {
            break;
        }
    }
    
    // Find and validate id name response
    let id_name = id_responses.iter()
        .find(|r| r.starts_with("id name"))
        .expect("Should have id name response");
    assert!(id_name.contains("Opera Engine"), "Should contain engine name");
    
    // Find and validate id author response
    let id_author = id_responses.iter()
        .find(|r| r.starts_with("id author"))
        .expect("Should have id author response");
    assert!(id_author.contains("Opera Engine Team"), "Should contain author name");
}

/// Test UCI options are properly declared
#[tokio::test]
#[traced_test]
async fn test_uci_options_declaration() {
    let engine = UCIEngine::new();
    let mut responses = engine.subscribe_responses();
    
    engine.initialize().await.expect("Engine initialization should succeed");
    engine.process_command("uci").await.expect("UCI command should succeed");
    
    // Collect all responses
    let mut all_responses = Vec::new();
    for _ in 0..20 {
        if let Ok(Ok(response)) = timeout(Duration::from_millis(10), responses.recv()).await {
            let is_uciok = response == "uciok";
            all_responses.push(response);
            if is_uciok {
                break;
            }
        } else {
            break;
        }
    }
    
    // Check for required options
    let option_responses: Vec<_> = all_responses.iter()
        .filter(|r| r.starts_with("option"))
        .collect();
    
    // Should have Hash option
    assert!(option_responses.iter().any(|r| r.contains("name Hash")), 
           "Should have Hash option");
    
    // Should have Threads option  
    assert!(option_responses.iter().any(|r| r.contains("name Threads")),
           "Should have Threads option");
    
    // Validate option format (at least one should be properly formatted)
    let hash_option = option_responses.iter()
        .find(|r| r.contains("name Hash"))
        .expect("Hash option should exist");
    assert!(hash_option.contains("type spin"), "Hash should be spin type");
    assert!(hash_option.contains("min"), "Hash should have min value");
    assert!(hash_option.contains("max"), "Hash should have max value");
}

/// Test isready command in different states
#[tokio::test]
#[traced_test]
async fn test_isready_various_states() {
    let engine = UCIEngine::new();
    let mut responses = engine.subscribe_responses();
    
    // Test isready in Ready state
    engine.initialize().await.expect("Engine initialization should succeed");
    engine.process_command("isready").await.expect("IsReady should succeed in Ready state");
    
    let response = timeout(Duration::from_millis(100), responses.recv())
        .await
        .expect("Should receive response")
        .expect("Response should be valid");
    assert_eq!(response, "readyok", "Should be ready in Ready state");
}

/// Test quit command handling
#[tokio::test]
#[traced_test]
async fn test_quit_command() {
    let engine = UCIEngine::new();
    let mut state_changes = engine.subscribe_state_changes();
    
    engine.initialize().await.expect("Engine initialization should succeed");
    
    // Wait for ready state
    timeout(Duration::from_millis(100), state_changes.recv()).await.ok();
    
    // Send quit command
    engine.process_command("quit").await.expect("Quit command should succeed");
    
    // Should transition to Stopping state
    let state_change = timeout(Duration::from_millis(100), state_changes.recv())
        .await
        .expect("Should receive state change")
        .expect("State change should be valid");
    assert_eq!(state_change.to, EngineState::Stopping, "Should transition to Stopping");
}

/// Test BasicCommandHandler directly (unit-level integration)
#[tokio::test]
#[traced_test] 
async fn test_basic_command_handler_integration() {
    let id_info = EngineIdentification {
        name: "Test Opera Engine".to_string(),
        author: "Test Team".to_string(),
        version: "1.0.0".to_string(),
    };
    
    let state = std::sync::Arc::new(opera_uci::UCIState::new());
    state.transition_to(opera_uci::EngineState::Ready, "Test setup").expect("Should set ready state");
    
    let handler = BasicCommandHandler::new(id_info, state);
    
    // Test UCI command
    let uci_responses = handler.handle_uci_command()
        .expect("UCI command should succeed");
    
    assert!(!uci_responses.is_empty(), "UCI should generate responses");
    assert!(uci_responses.iter().any(|r| r.contains("id name")), "Should have id name");
    assert!(uci_responses.iter().any(|r| r.contains("id author")), "Should have id author");
    assert_eq!(uci_responses.last().unwrap(), "uciok", "Should end with uciok");
    
    // Test isready command
    let ready_responses = handler.handle_isready_command()
        .expect("IsReady command should succeed");
    
    assert_eq!(ready_responses.len(), 1, "Should have one response");
    assert_eq!(ready_responses[0], "readyok", "Should respond readyok");
    
    // Test quit command
    let quit_responses = handler.handle_quit_command()
        .expect("Quit command should succeed");
    
    assert!(quit_responses.is_empty(), "Quit should have no response");
}

/// Test response formatting integration
#[test]
fn test_response_formatting_integration() {
    // Test creating responses using the new system
    let responses = vec![
        UCIResponse::id("Opera Engine", "Opera Team"),
        UCIResponse::spin_option("Hash", 128, 1, 8192),
        UCIResponse::check_option("MorphyStyle", false),
        UCIResponse::ready(),
        UCIResponse::uciok(),
    ];
    
    let formatted = ResponseFormatter::format_batch(&responses)
        .expect("Should format responses successfully");
    
    let lines: Vec<&str> = formatted.lines().collect();
    assert!(lines.len() >= 5, "Should have at least 5 lines"); // id name, id author, option, option, ready, uciok
    
    // Check specific response formats
    assert!(lines.iter().any(|line| line.starts_with("id name")));
    assert!(lines.iter().any(|line| line.starts_with("id author")));
    assert!(lines.iter().any(|line| line.contains("option name Hash")));
    assert!(lines.iter().any(|line| *line == "readyok"));
    assert!(lines.iter().any(|line| *line == "uciok"));
}

/// Test error handling in basic commands
#[tokio::test]
#[traced_test]
async fn test_basic_command_error_handling() {
    // Test handler with engine in wrong state
    let id_info = EngineIdentification {
        name: "Test Engine".to_string(),
        author: "Test Author".to_string(),
        version: "1.0.0".to_string(),
    };
    
    let state = std::sync::Arc::new(opera_uci::UCIState::new());
    // Leave in Initializing state (not ready)
    
    let handler = BasicCommandHandler::new(id_info, state);
    
    // UCI command should work regardless of state
    let uci_result = handler.handle_uci_command();
    assert!(uci_result.is_ok(), "UCI command should work in any state");
    
    // IsReady should fail in Initializing state
    let ready_result = handler.handle_isready_command();
    assert!(ready_result.is_err(), "IsReady should fail when not ready");
    
    // Quit should work in any state
    let quit_result = handler.handle_quit_command();
    assert!(quit_result.is_ok(), "Quit should work in any state");
}

/// Test concurrent command processing
#[tokio::test]
#[traced_test]
async fn test_concurrent_basic_commands() {
    let engine = UCIEngine::new();
    engine.initialize().await.expect("Engine initialization should succeed");
    
    // Process multiple commands concurrently
    let engine = std::sync::Arc::new(engine);
    let tasks = vec![
        tokio::spawn({
            let engine_clone = engine.clone();
            async move {
                for _ in 0..5 {
                    engine_clone.process_command("isready").await.unwrap();
                    tokio::time::sleep(Duration::from_millis(1)).await;
                }
            }
        }),
        tokio::spawn({
            let engine_clone = engine.clone();
            async move {
                engine_clone.process_command("uci").await.unwrap();
            }
        }),
    ];
    
    // Wait for all tasks to complete
    for task in tasks {
        task.await.expect("Task should complete successfully");
    }
}

/// Test full UCI session simulation
#[tokio::test]
#[traced_test]
async fn test_full_uci_session() {
    let engine = UCIEngine::new();
    let mut responses = engine.subscribe_responses();
    
    // Simulate complete UCI session as a chess GUI would do
    engine.initialize().await.expect("Engine should initialize");
    
    // 1. GUI sends uci
    engine.process_command("uci").await.expect("UCI command should work");
    
    // Wait for uciok
    let mut uciok_received = false;
    for _ in 0..20 {
        if let Ok(Ok(response)) = timeout(Duration::from_millis(10), responses.recv()).await {
            if response == "uciok" {
                uciok_received = true;
                break;
            }
        }
    }
    assert!(uciok_received, "Should receive uciok");
    
    // 2. GUI checks if engine is ready
    engine.process_command("isready").await.expect("IsReady should work");
    
    let ready_response = timeout(Duration::from_millis(100), responses.recv())
        .await
        .expect("Should get ready response")
        .expect("Response should be valid");
    assert_eq!(ready_response, "readyok");
    
    // 3. GUI sets some options
    engine.process_command("setoption name Hash value 64").await.expect("SetOption should work");
    engine.process_command("setoption name Threads value 2").await.expect("SetOption should work");
    
    // 4. GUI checks ready again
    engine.process_command("isready").await.expect("IsReady should work after options");
    
    let ready_response2 = timeout(Duration::from_millis(100), responses.recv())
        .await
        .expect("Should get second ready response")
        .expect("Response should be valid");
    assert_eq!(ready_response2, "readyok");
    
    // 5. GUI starts new game
    engine.process_command("ucinewgame").await.expect("UciNewGame should work");
    
    // 6. GUI quits
    engine.process_command("quit").await.expect("Quit should work");
}

/// Benchmark basic command processing performance
#[tokio::test]
#[traced_test]
async fn test_basic_command_performance() {
    let engine = UCIEngine::new();
    engine.initialize().await.expect("Engine should initialize");
    
    let start_time = std::time::Instant::now();
    
    // Process 100 isready commands
    for _ in 0..100 {
        engine.process_command("isready").await.expect("IsReady should work");
    }
    
    let elapsed = start_time.elapsed();
    
    // Should be able to process commands quickly
    assert!(elapsed < Duration::from_millis(100), 
           "100 isready commands should complete in under 100ms, took {:?}", elapsed);
    
    // Performance should be consistent
    let start_time2 = std::time::Instant::now();
    
    for _ in 0..10 {
        engine.process_command("uci").await.expect("UCI should work");
    }
    
    let elapsed2 = start_time2.elapsed();
    assert!(elapsed2 < Duration::from_millis(50),
           "10 UCI commands should complete quickly, took {:?}", elapsed2);
}