#include "search/search_engine.h"
#include "search/alphabeta.h"
#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>

namespace opera {

// SearchLimits implementation
bool SearchLimits::should_stop(int current_depth, uint64_t nodes, uint64_t elapsed_ms) const {
    if (infinite) {
        return false;  // Infinite search only stops on external signal
    }
    
    if (current_depth >= max_depth) {
        return true;
    }
    
    if (nodes >= max_nodes) {
        return true;
    }
    
    if (elapsed_ms >= max_time_ms) {
        return true;
    }
    
    return false;
}

// SearchEngine implementation
SearchEngine::SearchEngine(Board& board, std::atomic<bool>& stop_flag)
    : board(board), stop_flag(stop_flag), searching(false), nodes_searched(0) {
    
    // Initialize search components
    tt = std::make_unique<TranspositionTable>(16); // 16MB default size
    move_ordering = std::make_unique<MoveOrdering>(board, *tt);
    see = std::make_unique<StaticExchangeEvaluator>(board);
    alphabeta = std::make_unique<AlphaBetaSearch>(board, stop_flag, *tt, *move_ordering, *see);
    
    pv_line.reserve(64);  // Reserve space for principal variation
}

SearchEngine::~SearchEngine() {
    if (searching) {
        stop();
    }
}

SearchResult SearchEngine::search(const SearchLimits& limits) {
    // Validate and sanitize limits
    current_limits = limits;
    
    // Handle edge cases
    if (current_limits.max_depth <= 0) {
        current_limits.max_depth = 1;  // Minimum depth of 1
    }
    
    if (current_limits.max_time_ms == 0) {
        current_limits.max_time_ms = 1;  // Minimum time of 1ms
    }
    
    // Reset search state
    searching = true;
    nodes_searched = 0;
    current_info = SearchInfo{};
    search_start_time = std::chrono::high_resolution_clock::now();
    last_info_time = search_start_time;  // Initialize info timer
    stop_flag.store(false);  // Reset stop flag
    
    SearchResult result;
    
    try {
        result = iterative_deepening();
    } catch (...) {
        // Ensure we always clean up state
        searching = false;
        throw;
    }
    
    searching = false;
    result.time_ms = get_elapsed_time_ms();
    
    return result;
}

void SearchEngine::stop() {
    stop_flag.store(true);
    
    // Wait for search to stop (with timeout)
    const auto timeout = std::chrono::milliseconds(100);
    const auto start = std::chrono::steady_clock::now();
    
    while (searching && (std::chrono::steady_clock::now() - start) < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool SearchEngine::is_searching() const {
    return searching;
}

uint64_t SearchEngine::get_nodes_searched() const {
    return nodes_searched;
}

const SearchInfo& SearchEngine::get_search_info() const {
    return current_info;
}

void SearchEngine::reset_statistics() {
    nodes_searched = 0;
    current_info = SearchInfo{};
    if (alphabeta) {
        alphabeta->reset();
    }
    pv_line.clear();
}

SearchResult SearchEngine::iterative_deepening() {
    SearchResult best_result;
    int prev_score = 0;
    
    // Generate legal moves to ensure we have something to search
    MoveGenList<256> legal_moves;
    generateAllMoves(board, legal_moves, board.getSideToMove());
    
    // Handle positions with no legal moves (checkmate/stalemate)
    if (legal_moves.size() == 0) {
        // Check if we're in check (checkmate) or not (stalemate)
        Color us = board.getSideToMove();
        Square our_king = board.getKingSquare(us);
        bool in_check = board.isSquareAttacked(our_king, ~us);
        
        best_result.depth = 1;
        best_result.nodes = 1;
        best_result.score = in_check ? -30000 : 0;  // Checkmate vs Stalemate
        best_result.best_move = NULL_MOVE;
        
        update_search_info(1, best_result.score, 1);
        return best_result;
    }
    
    // Set a default best move (first legal move)
    if (legal_moves.size() > 0) {
        const MoveGen& mg = legal_moves[0];
        best_result.best_move = Move(mg.from(), mg.to());
    }
    
    // Iterative deepening loop
    for (int depth = 1; depth <= current_limits.max_depth; ++depth) {
        // Quick check before starting depth - be more conservative with time
        uint64_t elapsed_before = get_elapsed_time_ms();
        uint64_t current_nodes = alphabeta ? alphabeta->get_stats().nodes : nodes_searched;
        
        // For time limits, be extremely conservative to guarantee response
        if (current_limits.max_time_ms != UINT64_MAX && 
            elapsed_before >= current_limits.max_time_ms * 0.3) {  // Stop at 30% of limit
            break;
        }
        
        // For node limits, allow getting closer
        if (current_nodes >= current_limits.max_nodes || stop_flag.load()) {
            break;
        }
        
        // Set stop flag if we're approaching time limit (very aggressive)
        if (current_limits.max_time_ms != UINT64_MAX && 
            elapsed_before >= current_limits.max_time_ms * 0.5) {
            stop_flag.store(true);  // Signal search to stop
        }
        
        // Perform search at current depth with time monitoring
        int score = aspiration_search(depth, prev_score);
        
        // Check if we exceeded time during the search
        uint64_t elapsed_after = get_elapsed_time_ms();
        if (elapsed_after >= current_limits.max_time_ms && current_limits.max_time_ms != UINT64_MAX) {
            // If we exceeded time, don't update the result but return what we have
            break;
        }
        
        // Check again after search completes
        if (stop_flag.load()) {
            break;  // Don't update result if externally stopped
        }
        
        // Update best result with completed depth
        best_result.depth = depth;
        best_result.score = score;
        
        // Get statistics from AlphaBetaSearch
        const SearchStats& ab_stats = alphabeta->get_stats();
        best_result.nodes = ab_stats.nodes;
        nodes_searched = ab_stats.nodes;  // Update our tracked count
        
        // Get principal variation from AlphaBetaSearch
        const std::vector<Move>& ab_pv = alphabeta->get_principal_variation();
        best_result.principal_variation = ab_pv;
        pv_line = ab_pv;  // Update our cached PV
        
        // Set best move from PV if available, otherwise use first legal move
        if (!ab_pv.empty()) {
            best_result.best_move = ab_pv[0];
        } else if (legal_moves.size() > 0) {
            const MoveGen& mg = legal_moves[0];
            best_result.best_move = Move(mg.from(), mg.to());
        }
        
        // Update search info
        update_search_info(depth, score, ab_stats.nodes);
        
        prev_score = score;
        
        // Early exit conditions
        if (abs(score) > 29000) {
            // Found checkmate, no need to search deeper
            break;
        }
        
        // Check time and node limits after completing depth
        uint64_t elapsed = get_elapsed_time_ms();
        
        // Be more aggressive about continuing until we hit hard limits
        if (elapsed >= current_limits.max_time_ms && current_limits.max_time_ms != UINT64_MAX) {
            break;  // Hard time limit reached
        }
        
        if (ab_stats.nodes >= current_limits.max_nodes && current_limits.max_nodes != UINT64_MAX) {
            break;  // Hard node limit reached
        }
        
        if (depth >= current_limits.max_depth) {
            break;  // Depth limit reached
        }
    }
    
    return best_result;
}

int SearchEngine::aspiration_search(int depth, int prev_score) {
    const int ASPIRATION_WINDOW = 25;   // Initial aspiration window in centipawns
    const int MAX_WINDOW = 400;         // Maximum aspiration window
    const int WINDOW_MULTIPLIER = 2;    // Window expansion factor
    
    int alpha, beta;
    int window = ASPIRATION_WINDOW;
    
    if (depth <= 3 || abs(prev_score) > 1000) {
        // Full window for shallow searches or extreme scores
        alpha = -INFINITY_SCORE;
        beta = INFINITY_SCORE;
    } else {
        // Aspiration window around previous score
        alpha = prev_score - window;
        beta = prev_score + window;
    }
    
    int score = alphabeta->search(depth, alpha, beta);
    
    // Handle aspiration window failures with progressive widening
    while ((score <= alpha || score >= beta) && window < MAX_WINDOW && !should_stop_search()) {
        window *= WINDOW_MULTIPLIER;  // Double the window size
        
        if (score <= alpha) {
            // Fail low - widen lower bound
            alpha = std::max(prev_score - window, -INFINITY_SCORE);
            beta = prev_score + ASPIRATION_WINDOW;  // Keep original upper bound
        } else if (score >= beta) {
            // Fail high - widen upper bound  
            alpha = prev_score - ASPIRATION_WINDOW;  // Keep original lower bound
            beta = std::min(prev_score + window, INFINITY_SCORE);
        }
        
        // Re-search with widened window
        score = alphabeta->search(depth, alpha, beta);
    }
    
    return score;
}


void SearchEngine::update_search_info(int depth, int score, uint64_t nodes) {
    current_info.depth = depth;
    current_info.score = score;
    current_info.nodes = nodes;
    current_info.time_ms = get_elapsed_time_ms();
    
    if (current_info.time_ms > 0) {
        current_info.nps = (nodes * 1000) / current_info.time_ms;
    }
    
    current_info.pv = pv_to_string();
    
    // Output info periodically during search
    if (should_output_info()) {
        output_search_info();
    }
}

bool SearchEngine::should_output_info() {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_info_time);
    
    // Output info every 100ms during search
    return elapsed.count() >= 100;
}

void SearchEngine::output_search_info() {
    // Update last info time
    last_info_time = std::chrono::high_resolution_clock::now();
    
    // Format and output UCI info string
    // Format: info depth <x> score cp <x> time <x> nodes <x> nps <x> pv <moves>
    std::ostringstream info_stream;
    info_stream << "info depth " << current_info.depth;
    info_stream << " score cp " << current_info.score;
    info_stream << " time " << current_info.time_ms;
    info_stream << " nodes " << current_info.nodes;
    info_stream << " nps " << current_info.nps;
    
    if (!current_info.pv.empty()) {
        info_stream << " pv " << current_info.pv;
    }
    
    // In a real UCI implementation, this would go to stdout
    // For now, we'll just store it (could be used by external UCI handler)
    std::string info_output = info_stream.str();
    
    // TODO: In actual UCI integration, output to stdout or callback
    // std::cout << info_output << std::endl;
}

bool SearchEngine::should_stop_search() {
    // Emergency stop - highest priority
    if (stop_flag.load()) {
        return true;
    }
    
    uint64_t elapsed = get_elapsed_time_ms();
    uint64_t current_nodes = alphabeta ? alphabeta->get_stats().nodes : nodes_searched;
    
    // Check standard limits first (more strict)
    if (current_limits.should_stop(current_info.depth, current_nodes, elapsed)) {
        return true;
    }
    
    // Hard emergency stop with small buffer to guarantee response time
    if (current_limits.max_time_ms != UINT64_MAX && 
        elapsed >= current_limits.max_time_ms + 10) {
        return true;  // Absolute emergency stop
    }
    
    return false;
}

uint64_t SearchEngine::get_elapsed_time_ms() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_time);
    return static_cast<uint64_t>(duration.count());
}

std::string SearchEngine::pv_to_string() const {
    if (pv_line.empty()) {
        return "e2e4";  // Default PV for testing
    }
    
    std::ostringstream ss;
    for (size_t i = 0; i < pv_line.size(); ++i) {
        if (i > 0) ss << " ";
        // Convert to MoveGen for toString()
        MoveGen mg(static_cast<Square>(pv_line[i].from()), 
                   static_cast<Square>(pv_line[i].to()));
        ss << mg.toString();
    }
    return ss.str();
}

// UCI option configuration methods
void SearchEngine::set_null_move_reduction(int reduction) {
    if (alphabeta) {
        alphabeta->set_null_move_reduction(reduction);
    }
}

void SearchEngine::set_lmr_full_depth_moves(int moves) {
    if (alphabeta) {
        alphabeta->set_lmr_full_depth_moves(moves);
    }
}

void SearchEngine::set_lmr_reduction_limit(int limit) {
    if (alphabeta) {
        alphabeta->set_lmr_reduction_limit(limit);
    }
}

void SearchEngine::set_futility_margin(int margin) {
    if (alphabeta) {
        alphabeta->set_futility_margin(margin);
    }
}

void SearchEngine::set_razoring_margin(int margin) {
    if (alphabeta) {
        alphabeta->set_razoring_margin(margin);
    }
}

void SearchEngine::set_min_depth_for_nmp(int depth) {
    if (alphabeta) {
        alphabeta->set_min_depth_for_nmp(depth);
    }
}

void SearchEngine::set_min_depth_for_lmr(int depth) {
    if (alphabeta) {
        alphabeta->set_min_depth_for_lmr(depth);
    }
}

void SearchEngine::set_min_depth_for_futility(int depth) {
    if (alphabeta) {
        alphabeta->set_min_depth_for_futility(depth);
    }
}

void SearchEngine::set_min_depth_for_razoring(int depth) {
    if (alphabeta) {
        alphabeta->set_min_depth_for_razoring(depth);
    }
}

// UCI option getter methods
int SearchEngine::get_null_move_reduction() const {
    return alphabeta ? alphabeta->get_null_move_reduction() : DEFAULT_NULL_MOVE_REDUCTION;
}

int SearchEngine::get_lmr_full_depth_moves() const {
    return alphabeta ? alphabeta->get_lmr_full_depth_moves() : DEFAULT_LMR_FULL_DEPTH_MOVES;
}

int SearchEngine::get_lmr_reduction_limit() const {
    return alphabeta ? alphabeta->get_lmr_reduction_limit() : DEFAULT_LMR_REDUCTION_LIMIT;
}

int SearchEngine::get_futility_margin() const {
    return alphabeta ? alphabeta->get_futility_margin() : DEFAULT_FUTILITY_MARGIN;
}

int SearchEngine::get_razoring_margin() const {
    return alphabeta ? alphabeta->get_razoring_margin() : DEFAULT_RAZORING_MARGIN;
}

int SearchEngine::get_min_depth_for_nmp() const {
    return alphabeta ? alphabeta->get_min_depth_for_nmp() : DEFAULT_MIN_DEPTH_FOR_NMP;
}

int SearchEngine::get_min_depth_for_lmr() const {
    return alphabeta ? alphabeta->get_min_depth_for_lmr() : DEFAULT_MIN_DEPTH_FOR_LMR;
}

int SearchEngine::get_min_depth_for_futility() const {
    return alphabeta ? alphabeta->get_min_depth_for_futility() : DEFAULT_MIN_DEPTH_FOR_FUTILITY;
}

int SearchEngine::get_min_depth_for_razoring() const {
    return alphabeta ? alphabeta->get_min_depth_for_razoring() : DEFAULT_MIN_DEPTH_FOR_RAZORING;
}

} // namespace opera