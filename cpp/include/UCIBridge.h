#pragma once

#include "Board.h"
#include "MoveGen.h"
#include "Types.h"
#include <memory>
#include <string>
#include <vector>

namespace opera {

// C++ struct definitions that match Rust FFI interface (forward declared)
struct SearchLimits {
    int32_t depth;
    uint64_t nodes;
    uint64_t time_ms;
    bool infinite;
    
    SearchLimits() : depth(0), nodes(0), time_ms(0), infinite(false) {}
};

struct SearchInfo {
    int32_t depth;
    int32_t score;
    uint64_t time_ms;
    uint64_t nodes;
    uint64_t nps;
    std::string pv;
    
    SearchInfo() : depth(0), score(0), time_ms(0), nodes(0), nps(0) {}
};

// Search class (stub implementation for FFI integration)
class Search {
private:
    bool searching;
    SearchLimits limits;
    std::string bestMove;
    SearchInfo info;
    
public:
    Search();
    bool startSearch(const Board& board, const SearchLimits& searchLimits);
    void stop();
    bool isSearching() const;
    const std::string& getBestMove() const;
    const SearchInfo& getSearchInfo() const;
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

// Search operations - simplified interface
std::unique_ptr<opera::Search> create_search();
bool search_start(opera::Search& search, const opera::Board& board, int32_t depth, uint64_t time_ms);
void search_stop(opera::Search& search);
rust::String search_get_best_move(const opera::Search& search);
bool search_is_searching(const opera::Search& search);

// Engine configuration
bool engine_set_hash_size(uint32_t size_mb);
bool engine_set_threads(uint32_t thread_count);
bool engine_clear_hash();

// Rust callback declarations (implemented in Rust)
void on_search_progress(const opera::SearchInfo& info);
void on_engine_error(const std::string& error_msg);