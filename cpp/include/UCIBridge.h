#pragma once

#include "Board.h"
#include "MoveGen.h"
#include "Types.h"
#include <memory>
#include <string>
#include <vector>
#include <atomic>

// Forward declare SearchEngine to avoid circular dependencies
namespace opera {
    class SearchEngine;
    struct SearchResult;
}

namespace opera {

// FFI-compatible search result struct (no std::vector)
struct FFISearchResult {
    std::string best_move;      // UCI move string (e.g., "e2e4")
    std::string ponder_move;    // UCI ponder move string
    int32_t score;              // Centipawn score
    int32_t depth;              // Depth searched
    uint64_t nodes;             // Nodes searched
    uint64_t time_ms;           // Time taken in milliseconds
    std::string pv;             // Principal variation as UCI string

    FFISearchResult() : score(0), depth(0), nodes(0), time_ms(0) {}
};

// FFI-compatible search limits (matches Rust side)
struct FFISearchLimits {
    int32_t max_depth;
    uint64_t max_nodes;
    uint64_t max_time_ms;
    bool infinite;

    FFISearchLimits() : max_depth(64), max_nodes(UINT64_MAX), max_time_ms(UINT64_MAX), infinite(false) {}
};

// FFI-compatible search info for progress reporting
struct FFISearchInfo {
    int32_t depth;
    int32_t score;
    uint64_t time_ms;
    uint64_t nodes;
    uint64_t nps;
    std::string pv;

    FFISearchInfo() : depth(0), score(0), time_ms(0), nodes(0), nps(0) {}
};

// SearchEngineWrapper manages SearchEngine lifecycle for FFI
// Owns the stop flag and search engine instance
class SearchEngineWrapper {
private:
    std::unique_ptr<SearchEngine> engine;
    std::atomic<bool> stop_flag;
    FFISearchResult last_result;

public:
    explicit SearchEngineWrapper(Board& board);
    ~SearchEngineWrapper();

    // Non-copyable
    SearchEngineWrapper(const SearchEngineWrapper&) = delete;
    SearchEngineWrapper& operator=(const SearchEngineWrapper&) = delete;

    // Start search (blocking call)
    FFISearchResult search(const FFISearchLimits& limits);

    // Stop search immediately
    void stop();

    // Check if currently searching
    bool is_searching() const;

    // Get last search result
    const FFISearchResult& get_last_result() const;

    // Reset for new game
    void reset();
};

} // namespace opera

// C++ Functions for Rust FFI (cxx compatible)
// These functions will be called from Rust through the cxx bridge

// Need to include rust::Str for cxx compatibility
#include "rust/cxx.h"

// Board operations - simplified for initial FFI
std::unique_ptr<opera::Board> create_board();
bool board_set_fen(opera::Board& board, rust::Str fen);
bool board_make_move(opera::Board& board, rust::Str move_str);
rust::String board_get_fen(const opera::Board& board);
bool board_is_valid_move(const opera::Board& board, rust::Str move_str);
void board_reset(opera::Board& board);
bool board_is_in_check(const opera::Board& board);
bool board_is_checkmate(const opera::Board& board);
bool board_is_stalemate(const opera::Board& board);

// Search operations - real SearchEngine integration
std::unique_ptr<opera::SearchEngineWrapper> create_search_engine(opera::Board& board);
opera::FFISearchResult search_engine_search(opera::SearchEngineWrapper& engine, const opera::FFISearchLimits& limits);
void search_engine_stop(opera::SearchEngineWrapper& engine);
bool search_engine_is_searching(const opera::SearchEngineWrapper& engine);
opera::FFISearchResult search_engine_get_result(const opera::SearchEngineWrapper& engine);
void search_engine_reset(opera::SearchEngineWrapper& engine);

// Engine configuration
bool engine_set_hash_size(uint32_t size_mb);
bool engine_set_threads(uint32_t thread_count);
bool engine_clear_hash();

// Rust callback declarations (implemented in Rust)
void on_search_progress(const opera::SearchInfo& info);
void on_engine_error(const std::string& error_msg);