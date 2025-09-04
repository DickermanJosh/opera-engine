#include "search/search_engine.h"
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
        if (should_stop_search()) {
            break;
        }
        
        // Perform search at current depth
        int score = aspiration_search(depth, prev_score);
        
        if (should_stop_search()) {
            break;  // Don't update result if search was interrupted
        }
        
        // Update best result with completed depth
        best_result.depth = depth;
        best_result.score = score;
        best_result.nodes = nodes_searched;
        
        // Find best move from current search (simplified - just pick first legal move for now)
        if (legal_moves.size() > 0) {
            // Convert MoveGen to Move for the result
            const MoveGen& mg = legal_moves[0];
            best_result.best_move = Move(mg.from(), mg.to());
        }
        
        // Update search info
        update_search_info(depth, score, nodes_searched);
        
        prev_score = score;
        
        // Early exit conditions
        if (abs(score) > 29000) {
            // Found checkmate, no need to search deeper
            break;
        }
        
        // Check time and node limits after completing depth
        uint64_t elapsed = get_elapsed_time_ms();
        if (current_limits.should_stop(depth, nodes_searched, elapsed)) {
            break;
        }
    }
    
    return best_result;
}

int SearchEngine::aspiration_search(int depth, int prev_score) {
    const int ASPIRATION_WINDOW = 25;  // centipawns
    
    int alpha, beta;
    
    if (depth <= 3 || abs(prev_score) > 1000) {
        // Full window for shallow searches or extreme scores
        alpha = -30000;
        beta = 30000;
    } else {
        // Aspiration window around previous score
        alpha = prev_score - ASPIRATION_WINDOW;
        beta = prev_score + ASPIRATION_WINDOW;
    }
    
    int score = alpha_beta(depth, alpha, beta);
    
    // Handle aspiration window failures
    if (score <= alpha) {
        // Fail low - research with lower bound
        score = alpha_beta(depth, -30000, beta);
    } else if (score >= beta) {
        // Fail high - research with upper bound
        score = alpha_beta(depth, alpha, 30000);
    }
    
    return score;
}

int SearchEngine::alpha_beta(int depth, int alpha, int beta) {
    nodes_searched++;
    
    // Check for stop conditions periodically
    if ((nodes_searched & 1023) == 0) {  // Check every 1024 nodes
        if (should_stop_search()) {
            return alpha;  // Return quickly on stop
        }
    }
    
    // Terminal node (simplified evaluation)
    if (depth <= 0) {
        // Very basic evaluation - just return material count
        // This will be replaced with proper evaluation later
        int material = 0;
        
        // Count material for both sides
        Color us = board.getSideToMove();
        for (int pieceType = PAWN; pieceType <= QUEEN; ++pieceType) {
            material += __builtin_popcountll(board.getPieceBitboard(us, static_cast<PieceType>(pieceType))) * 
                       (pieceType == PAWN ? 100 : pieceType == KNIGHT ? 320 : pieceType == BISHOP ? 330 : 
                        pieceType == ROOK ? 500 : 900);
            material -= __builtin_popcountll(board.getPieceBitboard(~us, static_cast<PieceType>(pieceType))) * 
                       (pieceType == PAWN ? 100 : pieceType == KNIGHT ? 320 : pieceType == BISHOP ? 330 : 
                        pieceType == ROOK ? 500 : 900);
        }
        
        return material;
    }
    
    // Generate moves
    MoveGenList<256> moves;
    generateAllMoves(board, moves, board.getSideToMove());
    
    // No legal moves - checkmate or stalemate
    if (moves.size() == 0) {
        Color us = board.getSideToMove();
        Square our_king = board.getKingSquare(us);
        bool in_check = board.isSquareAttacked(our_king, ~us);
        
        return in_check ? -30000 + (64 - depth) : 0;  // Checkmate vs Stalemate
    }
    
    int best_score = alpha;
    
    // Search all moves
    for (size_t i = 0; i < moves.size(); ++i) {
        const MoveGen& move = moves[i];
        
        if (should_stop_search()) {
            break;
        }
        
        // Make move
        if (!board.makeMove(move)) {
            continue;  // Illegal move, skip
        }
        
        // Recursive search
        int score = -alpha_beta(depth - 1, -beta, -best_score);
        
        // Unmake move
        board.unmakeMove(move);
        
        if (score > best_score) {
            best_score = score;
            
            if (best_score >= beta) {
                return beta;  // Beta cutoff
            }
        }
    }
    
    return best_score;
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
}

bool SearchEngine::should_stop_search() {
    if (stop_flag.load()) {
        return true;
    }
    
    uint64_t elapsed = get_elapsed_time_ms();
    return current_limits.should_stop(current_info.depth, nodes_searched, elapsed);
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

} // namespace opera