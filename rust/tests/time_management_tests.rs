//! Integration tests for time management system
//!
//! These tests verify the complete time management functionality including:
//! - Time limit calculation for various scenarios
//! - Early stopping logic
//! - Edge cases and safety guarantees
//! - Integration with tokio::time

use opera_uci::time::{
    FixedTimePolicy, InfiniteTimePolicy, PositionInfo, SearchParams, SearchProgress,
    StandardTimePolicy, TimePolicy, TimeLimits,
};
use std::time::Duration;

// StandardTimePolicy Integration Tests

#[test]
fn test_standard_policy_full_game_simulation() {
    let policy = StandardTimePolicy::new(50, 0.35);

    // Opening: Lots of time remaining
    let opening_params = SearchParams {
        wtime: Some(300_000), // 5 minutes
        winc: Some(2000),     // 2 second increment
        ..Default::default()
    };
    let opening_info = PositionInfo {
        is_opening: true,
        move_number: 5,
        ..Default::default()
    };

    let opening_limits = policy.calculate_time_limit(&opening_params, &opening_info);
    assert!(opening_limits.soft_limit_ms() > 1000); // Should allocate reasonable time
    assert!(opening_limits.soft_limit_ms() < 10_000); // But not too much in opening

    // Middlegame: Moderate time
    let middle_params = SearchParams {
        wtime: Some(180_000), // 3 minutes
        winc: Some(2000),
        ..Default::default()
    };
    let middle_info = PositionInfo {
        move_number: 20,
        ..Default::default()
    };

    let middle_limits = policy.calculate_time_limit(&middle_params, &middle_info);
    assert!(middle_limits.soft_limit_ms() > 1500);

    // Endgame: Less time, but fewer moves remaining
    let endgame_params = SearchParams {
        wtime: Some(60_000), // 1 minute
        winc: Some(2000),
        ..Default::default()
    };
    let endgame_info = PositionInfo {
        is_endgame: true,
        move_number: 50,
        ..Default::default()
    };

    let endgame_limits = policy.calculate_time_limit(&endgame_params, &endgame_info);
    // Endgame should allocate more time per move (fewer moves remaining)
    assert!(endgame_limits.soft_limit_ms() > middle_limits.soft_limit_ms());
}

#[test]
fn test_standard_policy_time_trouble() {
    let policy = StandardTimePolicy::new(50, 0.35);

    // Very low time - should use emergency allocation
    let params = SearchParams {
        wtime: Some(100), // 100ms remaining
        ..Default::default()
    };
    let position_info = PositionInfo::default();

    let limits = policy.calculate_time_limit(&params, &position_info);

    // Should be in emergency mode
    assert_eq!(limits.soft_limit_ms(), 10);
    assert!(limits.hard_limit_ms() <= 90);
}

#[test]
fn test_standard_policy_movestogo_sudden_death() {
    let policy = StandardTimePolicy::new(50, 0.35);

    // 40 moves in 2 hours, then sudden death
    let params = SearchParams {
        wtime: Some(7_200_000), // 2 hours
        movestogo: Some(40),
        ..Default::default()
    };
    let position_info = PositionInfo::default();

    let limits = policy.calculate_time_limit(&params, &position_info);

    // Should divide time evenly among 40 moves
    // (7200000 - 50) / 40 = 179987
    // soft = 179987 * 0.35 = 62995
    assert_eq!(limits.soft_limit_ms(), 62995);
}

#[test]
fn test_standard_policy_with_generous_increment() {
    let policy = StandardTimePolicy::new(50, 0.35);

    let params = SearchParams {
        wtime: Some(60_000),
        winc: Some(30_000), // 30 second increment!
        ..Default::default()
    };
    let position_info = PositionInfo::default();

    let limits = policy.calculate_time_limit(&params, &position_info);

    // Should be able to use increment time
    // base = (60000 - 50) / 30 + 30000 = 1998 + 30000 = 31998
    // soft = 31998 * 0.35 = 11199
    assert_eq!(limits.soft_limit_ms(), 11199);
}

#[test]
fn test_standard_policy_early_stopping_criteria() {
    let policy = StandardTimePolicy::default_policy();

    // Test progression from unstable to stable search
    let depths = vec![
        (4, false, 0, false),  // Depth 4, unstable - don't stop
        (6, false, 0, false),  // Depth 6, unstable - don't stop
        (8, true, 1, false),   // Depth 8, stable once - don't stop
        (10, true, 2, false),  // Depth 10, stable twice - don't stop
        (12, true, 3, true),   // Depth 12, stable 3 times - STOP
        (14, true, 5, true),   // Depth 14, very stable - STOP
    ];

    for (depth, stable, count, should_stop) in depths {
        let progress = SearchProgress {
            depth,
            best_move_stable: stable,
            stability_count: count,
            ..Default::default()
        };

        let result = policy.should_stop_early(Duration::from_millis(500), &progress);
        assert_eq!(
            result, should_stop,
            "At depth {}, stability {}, expected stop={}",
            depth, count, should_stop
        );
    }
}

// FixedTimePolicy Integration Tests

#[test]
fn test_fixed_policy_consistency() {
    let policy = FixedTimePolicy::from_millis(5000);

    // Should always return same limits regardless of parameters
    let scenarios = vec![
        SearchParams::infinite(),
        SearchParams::movetime(1000),
        SearchParams::depth(20),
        SearchParams {
            wtime: Some(300_000),
            winc: Some(5000),
            ..Default::default()
        },
    ];

    for params in scenarios {
        let limits = policy.calculate_time_limit(&params, &PositionInfo::default());
        assert_eq!(limits.soft_limit_ms(), 4950); // 5000 - 50
        assert_eq!(limits.hard_limit_ms(), 5000);
    }
}

#[test]
fn test_fixed_policy_never_stops_early() {
    let policy = FixedTimePolicy::from_millis(10_000);

    // Even with very stable search, should never stop early
    let progress = SearchProgress {
        depth: 20,
        best_move_stable: true,
        stability_count: 100,
        ..Default::default()
    };

    assert!(!policy.should_stop_early(Duration::from_millis(100), &progress));
    assert!(!policy.should_stop_early(Duration::from_millis(5000), &progress));
    assert!(!policy.should_stop_early(Duration::from_millis(9999), &progress));
}

// InfiniteTimePolicy Integration Tests

#[test]
fn test_infinite_policy_always_infinite() {
    let policy = InfiniteTimePolicy::new();

    let scenarios = vec![
        SearchParams::infinite(),
        SearchParams::movetime(5000),
        SearchParams::depth(10),
        SearchParams {
            wtime: Some(100),
            ..Default::default()
        },
    ];

    for params in scenarios {
        let limits = policy.calculate_time_limit(&params, &PositionInfo::default());
        assert!(limits.is_infinite());
    }
}

#[test]
fn test_infinite_policy_never_stops_early() {
    let policy = InfiniteTimePolicy::default();

    let progress = SearchProgress {
        depth: 30,
        best_move_stable: true,
        stability_count: 50,
        ..Default::default()
    };

    // Should never stop, even after hours
    assert!(!policy.should_stop_early(Duration::from_secs(3600), &progress));
}

// Edge Cases and Safety Tests

#[test]
fn test_time_limits_overflow_safety() {
    let policy = StandardTimePolicy::new(50, 0.35);

    // Very large time values
    let params = SearchParams {
        wtime: Some(u64::MAX / 2),
        winc: Some(u64::MAX / 4),
        ..Default::default()
    };
    let position_info = PositionInfo::default();

    // Should handle without panicking or overflowing
    let limits = policy.calculate_time_limit(&params, &position_info);
    assert!(limits.soft_limit_ms() > 0);
    assert!(limits.hard_limit_ms() >= limits.soft_limit_ms());
}

#[test]
fn test_negative_time_safety() {
    let policy = StandardTimePolicy::new(50, 0.35);

    // Time less than safety margin
    let params = SearchParams {
        wtime: Some(10), // Less than 50ms safety margin
        ..Default::default()
    };
    let position_info = PositionInfo::default();

    let limits = policy.calculate_time_limit(&params, &position_info);

    // Should handle gracefully with minimal time
    assert_eq!(limits.soft_limit_ms(), 10);
    assert_eq!(limits.hard_limit_ms(), 0); // 10 - 10 = 0 (saturating)
}

#[test]
fn test_position_info_affects_calculation() {
    let policy = StandardTimePolicy::new(50, 0.35);
    let params = SearchParams {
        wtime: Some(180_000),
        winc: Some(2000),
        ..Default::default()
    };

    // Opening position
    let opening_info = PositionInfo {
        is_opening: true,
        move_number: 5,
        legal_moves: 20,
        ..Default::default()
    };

    // Endgame position
    let endgame_info = PositionInfo {
        is_endgame: true,
        move_number: 60,
        legal_moves: 5,
        ..Default::default()
    };

    let opening_limits = policy.calculate_time_limit(&params, &opening_info);
    let endgame_limits = policy.calculate_time_limit(&params, &endgame_info);

    // Endgame should get more time (fewer moves remaining)
    assert!(endgame_limits.soft_limit_ms() > opening_limits.soft_limit_ms());
}

// SearchParams Builder Tests

#[test]
fn test_search_params_builders() {
    let infinite = SearchParams::infinite();
    assert!(infinite.is_infinite());
    assert!(!infinite.has_time_control());

    let movetime = SearchParams::movetime(5000);
    assert!(movetime.is_movetime());
    assert!(movetime.has_time_control());
    assert_eq!(movetime.movetime, Some(5000));

    let depth = SearchParams::depth(15);
    assert_eq!(depth.depth, Some(15));

    let nodes = SearchParams::nodes(1_000_000);
    assert_eq!(nodes.nodes, Some(1_000_000));
}

#[test]
fn test_search_params_has_time_control() {
    assert!(SearchParams::movetime(1000).has_time_control());

    assert!(SearchParams {
        wtime: Some(60000),
        ..Default::default()
    }
    .has_time_control());

    assert!(SearchParams {
        btime: Some(60000),
        ..Default::default()
    }
    .has_time_control());

    assert!(!SearchParams::infinite().has_time_control());
    assert!(!SearchParams::depth(10).has_time_control());
}

// TimeLimits Tests

#[test]
fn test_time_limits_conversions() {
    let limits = TimeLimits::from_millis(1000, 2000);
    assert_eq!(limits.soft_limit_ms(), 1000);
    assert_eq!(limits.hard_limit_ms(), 2000);
    assert!(!limits.is_infinite());

    let infinite = TimeLimits::infinite();
    assert!(infinite.is_infinite());
    assert!(infinite.soft_limit.as_secs() > 86400);
}

#[test]
fn test_time_limits_ordering() {
    let limits = TimeLimits::new(Duration::from_millis(500), Duration::from_millis(1000));

    // Soft should be less than or equal to hard
    assert!(limits.soft_limit <= limits.hard_limit);
    assert!(limits.soft_limit_ms() <= limits.hard_limit_ms());
}

// Real-world Scenario Tests

#[test]
fn test_bullet_chess_scenario() {
    let policy = StandardTimePolicy::new(30, 0.4); // Faster for bullet

    // 1+0 bullet
    let params = SearchParams {
        wtime: Some(60_000), // 1 minute
        winc: Some(0),
        ..Default::default()
    };
    let position_info = PositionInfo::default();

    let limits = policy.calculate_time_limit(&params, &position_info);

    // Should allocate time conservatively in bullet
    assert!(limits.soft_limit_ms() < 3000); // Less than 3 seconds
    assert!(limits.hard_limit_ms() < 10_000); // Less than 10 seconds
}

#[test]
fn test_rapid_chess_scenario() {
    let policy = StandardTimePolicy::default_policy();

    // 15+10 rapid
    let params = SearchParams {
        wtime: Some(900_000), // 15 minutes
        winc: Some(10_000),   // 10 seconds
        ..Default::default()
    };
    let position_info = PositionInfo::default();

    let limits = policy.calculate_time_limit(&params, &position_info);

    // Should have comfortable time in rapid
    assert!(limits.soft_limit_ms() > 3000); // More than 3 seconds
    assert!(limits.soft_limit_ms() < 20_000); // But not excessive
}

#[test]
fn test_classical_chess_scenario() {
    let policy = StandardTimePolicy::default_policy();

    // 90+30 classical
    let params = SearchParams {
        wtime: Some(5_400_000), // 90 minutes
        winc: Some(30_000),     // 30 seconds
        ..Default::default()
    };
    let position_info = PositionInfo::default();

    let limits = policy.calculate_time_limit(&params, &position_info);

    // Should have generous time in classical
    assert!(limits.soft_limit_ms() > 10_000); // More than 10 seconds
}

#[test]
fn test_fischer_increment_scenario() {
    let policy = StandardTimePolicy::default_policy();

    // 3+2 Fischer (blitz with increment)
    let params = SearchParams {
        wtime: Some(180_000), // 3 minutes
        winc: Some(2000),     // 2 seconds
        ..Default::default()
    };
    let position_info = PositionInfo::default();

    let limits = policy.calculate_time_limit(&params, &position_info);

    // Increment should allow reasonable thinking time
    assert!(limits.soft_limit_ms() > 1000);
    assert!(limits.soft_limit_ms() < 5000);
}

// Property-Based Tests (Manual)

#[test]
fn test_time_always_positive_property() {
    let policy = StandardTimePolicy::new(50, 0.35);

    let test_cases = vec![
        (0, 0),
        (100, 0),
        (1000, 100),
        (10_000, 1000),
        (60_000, 2000),
        (300_000, 5000),
        (u64::MAX / 1000, 0),
    ];

    for (wtime, winc) in test_cases {
        let params = SearchParams {
            wtime: Some(wtime),
            winc: Some(winc),
            ..Default::default()
        };
        let limits = policy.calculate_time_limit(&params, &PositionInfo::default());

        // Time should always be non-negative
        assert!(limits.soft_limit_ms() >= 0);
        assert!(limits.hard_limit_ms() >= 0);
    }
}

#[test]
fn test_soft_less_than_hard_property() {
    let policy = StandardTimePolicy::new(50, 0.35);

    let test_cases = vec![
        SearchParams::movetime(1000),
        SearchParams {
            wtime: Some(60_000),
            winc: Some(1000),
            ..Default::default()
        },
        SearchParams {
            wtime: Some(300_000),
            movestogo: Some(20),
            ..Default::default()
        },
    ];

    for params in test_cases {
        let limits = policy.calculate_time_limit(&params, &PositionInfo::default());

        // Soft limit should always be less than or equal to hard limit
        assert!(
            limits.soft_limit <= limits.hard_limit,
            "Soft limit {:?} should be <= hard limit {:?}",
            limits.soft_limit,
            limits.hard_limit
        );
    }
}
