#include "UCIBridge.h"
#include "Board.h"
#include "MoveGen.h"
#include <iostream>
#include <sstream>
#include "rust/cxx.h"

namespace opera {

// Search class implementation
Search::Search() : searching(false), bestMove("e2e4") {
    // Initialize with some default values
    info.depth = 1;
    info.score = 0;
    info.time_ms = 0;
    info.nodes = 0;
    info.nps = 0;
    info.pv = "e2e4";
}

bool Search::startSearch(const Board& /*board*/, const SearchLimits& searchLimits) {
    limits = searchLimits;
    searching = true;
    info.depth = limits.depth > 0 ? limits.depth : 1;
    return true;
}

void Search::stop() {
    searching = false;
}

bool Search::isSearching() const {
    return searching;
}

const std::string& Search::getBestMove() const {
    return bestMove;
}

const SearchInfo& Search::getSearchInfo() const {
    return info;
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
    // Parse move string (simplified implementation for now)
    std::string move_string(move_str);
    
    try {
        if (move_string.length() < 4) {
            return false;
        }
        
        // Extract from/to squares from UCI format (e.g., "e2e4")
        int from_file = move_string[0] - 'a';
        int from_rank = move_string[1] - '1';
        int to_file = move_string[2] - 'a';
        int to_rank = move_string[3] - '1';
        
        if (from_file < 0 || from_file > 7 || from_rank < 0 || from_rank > 7 ||
            to_file < 0 || to_file > 7 || to_rank < 0 || to_rank > 7) {
            return false;
        }
        
        opera::Square from = static_cast<opera::Square>(from_rank * 8 + from_file);
        opera::Square to = static_cast<opera::Square>(to_rank * 8 + to_file);
        
        // Create MoveGen object
        opera::MoveGen::MoveType moveType = opera::MoveGen::MoveType::NORMAL;
        opera::Piece promotion = opera::NO_PIECE;
        
        // Handle promotion
        if (move_string.length() == 5) {
            moveType = opera::MoveGen::MoveType::PROMOTION;
            char promo = move_string[4];
            opera::Color color = board.getSideToMove();
            
            switch (promo) {
                case 'q': promotion = color == opera::WHITE ? opera::WHITE_QUEEN : opera::BLACK_QUEEN; break;
                case 'r': promotion = color == opera::WHITE ? opera::WHITE_ROOK : opera::BLACK_ROOK; break;
                case 'b': promotion = color == opera::WHITE ? opera::WHITE_BISHOP : opera::BLACK_BISHOP; break;
                case 'n': promotion = color == opera::WHITE ? opera::WHITE_KNIGHT : opera::BLACK_KNIGHT; break;
                default:
                    return false;
            }
        }
        
        opera::MoveGen move(from, to, moveType, promotion);
        return board.makeMove(move);
        
    } catch (const std::exception&) {
        return false;
    }
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

// Search operations (stub implementations)
std::unique_ptr<opera::Search> create_search() {
    try {
        return std::make_unique<opera::Search>();
    } catch (const std::exception&) {
        return nullptr;
    }
}

bool search_start(opera::Search& search, const opera::Board& board, int32_t depth, uint64_t time_ms) {
    try {
        opera::SearchLimits limits;
        limits.depth = depth;
        limits.time_ms = time_ms;
        limits.nodes = 0;
        limits.infinite = false;
        return search.startSearch(board, limits);
    } catch (const std::exception&) {
        return false;
    }
}

void search_stop(opera::Search& search) {
    try {
        search.stop();
    } catch (const std::exception&) {
        // Ignore errors during stop - best effort
    }
}

rust::String search_get_best_move(const opera::Search& search) {
    try {
        return rust::String(search.getBestMove());
    } catch (const std::exception&) {
        return rust::String("");
    }
}


bool search_is_searching(const opera::Search& search) {
    try {
        return search.isSearching();
    } catch (const std::exception&) {
        return false;
    }
}

// Engine configuration (stub implementations)
bool engine_set_hash_size(uint32_t size_mb) {
    // TODO: Implement hash table size setting
    std::cout << "Setting hash size to " << size_mb << " MB" << std::endl;
    return true;
}

bool engine_set_threads(uint32_t thread_count) {
    // TODO: Implement thread count setting  
    std::cout << "Setting thread count to " << thread_count << std::endl;
    return true;
}

bool engine_clear_hash() {
    // TODO: Implement hash table clearing
    std::cout << "Clearing hash tables" << std::endl;
    return true;
}

