#include "UCIBridge.h"
#include "Board.h"
#include "MoveGen.h"
#include "search/search_engine.h"
#include <iostream>
#include <sstream>
#include "rust/cxx.h"
#include "opera-uci/src/ffi.rs.h"  // Generated FFI definitions

namespace opera {

// ============================================================================
// SearchEngineWrapper Implementation
// ============================================================================

SearchEngineWrapper::SearchEngineWrapper(Board& board)
    : board(board), stop_flag(false) {
    engine = std::make_unique<SearchEngine>(this->board, stop_flag);
}

SearchEngineWrapper::~SearchEngineWrapper() {
    if (engine && engine->is_searching()) {
        stop();
    }
}

FFISearchResult SearchEngineWrapper::search(const FFISearchLimits& limits) {
    // Convert FFI limits to SearchEngine limits
    opera::SearchLimits search_limits;
    search_limits.max_depth = limits.max_depth;
    search_limits.max_nodes = limits.max_nodes;
    search_limits.max_time_ms = limits.max_time_ms;
    search_limits.infinite = limits.infinite;

    // Reset stop flag before search
    stop_flag.store(false);

    // Execute search (blocking call)
    SearchResult result = engine->search(search_limits);

    // Convert SearchResult to FFISearchResult
    FFISearchResult ffi_result;
    ffi_result.best_move = result.best_move.toString();
    ffi_result.ponder_move = result.ponder_move.toString();
    ffi_result.score = result.score;
    ffi_result.depth = result.depth;
    ffi_result.nodes = result.nodes;
    ffi_result.time_ms = result.time_ms;

    // Convert PV to string
    std::ostringstream pv_stream;
    for (size_t i = 0; i < result.principal_variation.size(); ++i) {
        if (i > 0) pv_stream << " ";
        pv_stream << result.principal_variation[i].toString();
    }
    ffi_result.pv = pv_stream.str();

    return ffi_result;
}

void SearchEngineWrapper::stop() {
    stop_flag.store(true);
    if (engine) {
        engine->stop();
    }
}

bool SearchEngineWrapper::is_searching() const {
    return engine && engine->is_searching();
}

void SearchEngineWrapper::reset() {
    if (engine) {
        engine->reset_statistics();
    }
    stop_flag.store(false);
}

} // namespace opera

// C++ Functions for Rust FFI implementation

// Board operations
std::unique_ptr<opera::Board> create_board() {
    try {
        return std::make_unique<opera::Board>();
    } catch (const std::exception&) {
        // Return nullptr on failure - Rust will handle error reporting
        return nullptr;
    }
}

bool board_set_fen(opera::Board& board, rust::Str fen) {
    try {
        std::string fen_str(fen);
        board.setFromFEN(fen_str);
        return true;
    } catch (const std::exception&) {
        // Return false on failure - Rust will handle error reporting
        return false;
    }
}

bool board_make_move(opera::Board& board, rust::Str move_str) {
    try {
        std::string text(move_str);
        opera::MoveGenList<256> moves;
        opera::generateAllLegalMoves(board, moves, board.getSideToMove());
        for (size_t i = 0; i < moves.size(); ++i) {
            const auto& mg = moves[i];
            opera::Move move(mg.from(), mg.to(), mg.isPromotion() ? opera::PROMOTION : opera::NORMAL,
                mg.isPromotion() ? opera::typeOf(mg.promotionPiece()) : opera::NO_PIECE_TYPE);
            if (move.toString() == text) return board.makeMove(mg);
        }
        return false;
    } catch (const std::exception&) { return false; }
}

rust::String board_get_fen(const opera::Board& board) {
    try {
        return rust::String(board.toFEN());
    } catch (const std::exception&) {
        return rust::String("");
    }
}

bool board_is_valid_move(const opera::Board& board, rust::Str move_str) {
    try {
        // Create a copy of the board to test the move
        opera::Board test_board = board;
        return board_make_move(test_board, move_str);
    } catch (const std::exception& e) {
        return false;
    }
}

void board_reset(opera::Board& board) {
    try {
        board.setFromFEN(opera::STARTING_FEN);
    } catch (const std::exception&) {
        // Ignore errors during reset - best effort
    }
}


bool board_is_in_check(const opera::Board& board) {
    try {
        return board.isInCheck(board.getSideToMove());
    } catch (const std::exception&) {
        return false;
    }
}

bool board_is_checkmate(const opera::Board& board) {
    try {
        opera::Color side = board.getSideToMove();
        return board.isInCheck(side) && board.isCheckmate(side);
    } catch (const std::exception&) {
        return false;
    }
}

bool board_is_stalemate(const opera::Board& board) {
    try {
        opera::Color side = board.getSideToMove();
        return !board.isInCheck(side) && board.isStalemate(side);
    } catch (const std::exception&) {
        return false;
    }
}

// Engine configuration (stub implementations)
bool engine_set_hash_size(uint32_t size_mb) {
    // TODO: Implement hash table size setting
    return size_mb >= 1 && size_mb <= 2048;
}

bool engine_set_threads(uint32_t thread_count) {
    // TODO: Implement thread count setting  
    return thread_count == 1;
}

bool engine_clear_hash() {
    // TODO: Implement hash table clearing
    return false; // No global engine instance; use per-session reset.
}


// ============================================================================
// Search FFI Operations - Real SearchEngine Integration  
// ============================================================================

std::unique_ptr<opera::SearchEngineWrapper> create_search_engine(opera::Board& board) {
    try {
        return std::make_unique<opera::SearchEngineWrapper>(board);
    } catch (const std::exception&) {
        return nullptr;
    }
}

void search_engine_search(opera::SearchEngineWrapper& engine, const opera::FFISearchLimits& limits, opera::FFISearchResult& result) {
    try {
        result = engine.search(limits);
    } catch (const std::exception& e) {
        // Set error result on exception
        result.best_move = "0000";  // Null move indicator
        result.ponder_move = "";
        result.score = 0;
        result.depth = 0;
        result.nodes = 0;
        result.time_ms = 0;
        result.pv = "";
    }
}

void search_engine_stop(opera::SearchEngineWrapper& engine) {
    try {
        engine.stop();
    } catch (const std::exception&) {
        // Ignore errors during stop - best effort
    }
}

bool search_engine_is_searching(const opera::SearchEngineWrapper& engine) {
    try {
        return engine.is_searching();
    } catch (const std::exception&) {
        return false;
    }
}

void search_engine_reset(opera::SearchEngineWrapper& engine) {
    try {
        engine.reset();
    } catch (const std::exception&) {
        // Ignore errors during reset
    }
}

namespace opera {
FFISearchResult SearchEngineWrapper::controlled_search(const FFISearchLimits& limits,
        const ::SearchControl& control, uint32_t hash_mb, bool morphy, rust::Str root_moves) {
    if (hash_mb != 16) engine->set_hash_size(hash_mb);
    engine->set_use_morphy_style(morphy);
    std::istringstream input{std::string(root_moves)};
    std::vector<std::string> roots;
    for (std::string move; input >> move;) roots.push_back(move);
    engine->set_root_moves(roots);
    engine->external_stop = [&control] { return control.cancelled(); };
    engine->progress = [&control](const SearchInfo& info) {
        FFISearchInfo ffi_info;
        ffi_info.depth = info.depth; ffi_info.score = info.score;
        ffi_info.time_ms = info.time_ms; ffi_info.nodes = info.nodes;
        ffi_info.nps = info.nps; ffi_info.pv = info.pv;
        control.report(ffi_info);
    };
    try {
        auto result = search(limits);
        engine->external_stop = {}; engine->progress = {};
        return result;
    } catch (...) { engine->external_stop = {}; engine->progress = {}; throw; }
}
}
void search_engine_controlled(opera::SearchEngineWrapper& engine, const opera::FFISearchLimits& limits,
        const SearchControl& control, uint32_t hash_mb, bool morphy, rust::Str root_moves, opera::FFISearchResult& result) {
    result = engine.controlled_search(limits, control, hash_mb, morphy, root_moves);
}

bool board_previous_side_in_check(const opera::Board& board) { return board.isInCheck(~board.getSideToMove()); }
