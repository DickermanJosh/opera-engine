#include <gtest/gtest.h>
#include "Board.h"
#include "MoveGen.h"
#include <chrono>

using namespace opera;

class KingMoveTest : public ::testing::Test {
protected:
    void SetUp() override {
        board.setFromFEN(STARTING_FEN);
    }
    
    Board board;
    MoveGenList<> moves;
    
    // Helper function to count moves from a specific square
    int countMovesFromSquare(const MoveGenList<>& moves, Square fromSquare) {
        int count = 0;
        for (size_t i = 0; i < moves.size(); ++i) {
            if (moves[i].from() == fromSquare) {
                count++;
            }
        }
        return count;
    }
    
    // Helper function to check if a move exists
    bool containsMove(const MoveGenList<>& moves, Square from, Square to) {
        for (size_t i = 0; i < moves.size(); ++i) {
            if (moves[i].from() == from && moves[i].to() == to) {
                return true;
            }
        }
        return false;
    }
    
    // Helper function to check if a castling move exists
    bool containsCastlingMove(const MoveGenList<>& moves, Square from, Square to) {
        for (size_t i = 0; i < moves.size(); ++i) {
            if (moves[i].from() == from && moves[i].to() == to && moves[i].isCastling()) {
                return true;
            }
        }
        return false;
    }
    
    // Helper function to count capture moves
    int countCaptureMovesFromSquare(const MoveGenList<>& moves, Square fromSquare) {
        int count = 0;
        for (size_t i = 0; i < moves.size(); ++i) {
            if (moves[i].from() == fromSquare && moves[i].isCapture()) {
                count++;
            }
        }
        return count;
    }
};

// ============================================================================
// KING MOVE GENERATION TESTS
// ============================================================================

// Test 1: Starting position - no king moves available
TEST_F(KingMoveTest, StartingPositionKingMovesWhite) {
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // No king moves available in starting position (blocked by own pieces)
    EXPECT_EQ(moves.size(), 0);
}

TEST_F(KingMoveTest, StartingPositionKingMovesBlack) {
    moves.clear();
    generateKingMoves(board, moves, BLACK);
    
    // No king moves available in starting position (blocked by own pieces)
    EXPECT_EQ(moves.size(), 0);
}

// Test 2: Center king with maximum mobility
TEST_F(KingMoveTest, CenterKingMaximumMoves) {
    // Place white king on d4 (center square)
    board.setFromFEN("8/8/8/8/3K4/8/8/8 w - - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // King on d4 should have 8 possible moves (all adjacent squares)
    EXPECT_EQ(moves.size(), 8);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 8);
    
    // Check all 8 directions from d4
    EXPECT_TRUE(containsMove(moves, D4, C3)); // Southwest
    EXPECT_TRUE(containsMove(moves, D4, C4)); // West
    EXPECT_TRUE(containsMove(moves, D4, C5)); // Northwest
    EXPECT_TRUE(containsMove(moves, D4, D3)); // South
    EXPECT_TRUE(containsMove(moves, D4, D5)); // North
    EXPECT_TRUE(containsMove(moves, D4, E3)); // Southeast
    EXPECT_TRUE(containsMove(moves, D4, E4)); // East
    EXPECT_TRUE(containsMove(moves, D4, E5)); // Northeast
}

// Test 3: Corner king with limited moves
TEST_F(KingMoveTest, CornerKingLimitedMoves) {
    // Place white king on a1 (corner)
    board.setFromFEN("8/8/8/8/8/8/8/K7 w - - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // King on a1 should have only 3 possible moves
    EXPECT_EQ(moves.size(), 3);
    EXPECT_EQ(countMovesFromSquare(moves, A1), 3);
    
    // Check the 3 available moves from corner
    EXPECT_TRUE(containsMove(moves, A1, A2)); // North
    EXPECT_TRUE(containsMove(moves, A1, B1)); // East
    EXPECT_TRUE(containsMove(moves, A1, B2)); // Northeast
}

// Test 4: Edge king mobility
TEST_F(KingMoveTest, EdgeKingMoves) {
    // Place white king on d1 (edge of board)
    board.setFromFEN("8/8/8/8/8/8/8/3K4 w - - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // King on d1 should have 5 possible moves
    EXPECT_EQ(moves.size(), 5);
    EXPECT_EQ(countMovesFromSquare(moves, D1), 5);
    
    // Check edge king moves
    EXPECT_TRUE(containsMove(moves, D1, C1)); // West
    EXPECT_TRUE(containsMove(moves, D1, C2)); // Northwest
    EXPECT_TRUE(containsMove(moves, D1, D2)); // North
    EXPECT_TRUE(containsMove(moves, D1, E1)); // East
    EXPECT_TRUE(containsMove(moves, D1, E2)); // Northeast
}

// Test 5: King blocked by own pieces
TEST_F(KingMoveTest, KingBlockedByOwnPieces) {
    // Place white king on d4 with white pieces blocking some squares
    board.setFromFEN("8/8/8/2PPP3/2PKP3/2PPP3/8/8 w - - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // King should have no moves (all adjacent squares blocked by own pieces)
    EXPECT_EQ(moves.size(), 0);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 0);
}

// Test 6: King captures enemy pieces
TEST_F(KingMoveTest, KingCapturesEnemyPieces) {
    // Place white king on d4 with black pieces on some adjacent squares
    board.setFromFEN("8/8/8/2ppp3/2pKp3/2ppp3/8/8 w - - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // King should be able to capture all 8 enemy pieces
    EXPECT_EQ(moves.size(), 8);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 8);
    EXPECT_EQ(countCaptureMovesFromSquare(moves, D4), 8);
    
    // Should be able to capture all adjacent enemy pieces
    EXPECT_TRUE(containsMove(moves, D4, C3));
    EXPECT_TRUE(containsMove(moves, D4, C4));
    EXPECT_TRUE(containsMove(moves, D4, C5));
    EXPECT_TRUE(containsMove(moves, D4, D3));
    EXPECT_TRUE(containsMove(moves, D4, D5));
    EXPECT_TRUE(containsMove(moves, D4, E3));
    EXPECT_TRUE(containsMove(moves, D4, E4));
    EXPECT_TRUE(containsMove(moves, D4, E5));
}

// Test 7: King with mixed pieces around
TEST_F(KingMoveTest, KingMixedPiecesAround) {
    // Place white king on d4 with mix of own and enemy pieces
    // Block unwanted moves: D5 with white pawn, E3 with white pawn, E5 with white pawn
    board.setFromFEN("8/8/8/2pPP3/3K1p2/2P1P3/8/8 w - - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // King should be able to move to empty squares and capture enemies
    EXPECT_EQ(moves.size(), 4);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 4);
    EXPECT_EQ(countCaptureMovesFromSquare(moves, D4), 1);
    
    // Should be able to move to empty squares
    EXPECT_TRUE(containsMove(moves, D4, C4)); // West (empty)
    EXPECT_TRUE(containsMove(moves, D4, D3)); // South (empty)
    
    // Should be able to capture enemy pieces
    EXPECT_TRUE(containsMove(moves, D4, C5)); // Northwest (enemy pawn)
}

// ============================================================================
// CASTLING TESTS
// ============================================================================

// Test 8: Kingside castling available
TEST_F(KingMoveTest, KingsideCastlingWhite) {
    // Clear path for kingside castling
    board.setFromFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // Should include kingside castling move
    EXPECT_TRUE(containsCastlingMove(moves, E1, G1));
    
    // Should also include normal king moves (if not blocked)
    EXPECT_TRUE(containsMove(moves, E1, F1)); // Adjacent square
}

// Test 9: Queenside castling available
TEST_F(KingMoveTest, QueensideCastlingWhite) {
    // Clear path for queenside castling
    board.setFromFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // Should include queenside castling move
    EXPECT_TRUE(containsCastlingMove(moves, E1, C1));
}

// Test 10: Castling blocked by pieces
TEST_F(KingMoveTest, CastlingBlockedByPieces) {
    // Pieces blocking castling paths
    board.setFromFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/RN2KB1R w KQkq - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // Should NOT include castling moves (blocked by knight and bishop)
    EXPECT_FALSE(containsCastlingMove(moves, E1, G1)); // Kingside blocked by bishop
    EXPECT_FALSE(containsCastlingMove(moves, E1, C1)); // Queenside blocked by knight
}

// Test 11: Castling rights lost
TEST_F(KingMoveTest, CastlingRightsLost) {
    // No castling rights available
    board.setFromFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w - - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // Should NOT include any castling moves
    EXPECT_FALSE(containsCastlingMove(moves, E1, G1));
    EXPECT_FALSE(containsCastlingMove(moves, E1, C1));
}

// Test 12: Black castling
TEST_F(KingMoveTest, BlackCastling) {
    // Black king castling
    board.setFromFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
    moves.clear();
    generateKingMoves(board, moves, BLACK);
    
    // Should include both castling moves for black
    EXPECT_TRUE(containsCastlingMove(moves, E8, G8)); // Kingside
    EXPECT_TRUE(containsCastlingMove(moves, E8, C8)); // Queenside
}

// ============================================================================
// EDGE CASE AND VALIDATION TESTS
// ============================================================================

// Test 13: Move type validation
TEST_F(KingMoveTest, MoveTypesAreCorrect) {
    // Place king for various move scenarios
    board.setFromFEN("8/8/8/2p5/3K4/8/8/8 w - - 0 1");
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    
    // All non-castling king moves should be NORMAL type
    for (size_t i = 0; i < moves.size(); ++i) {
        if (!moves[i].isCastling()) {
            EXPECT_EQ(moves[i].type(), MoveGen::MoveType::NORMAL);
            EXPECT_FALSE(moves[i].isPromotion());
            EXPECT_FALSE(moves[i].isEnPassant());
            EXPECT_FALSE(moves[i].isDoublePawnPush());
        }
    }
}

// Test 14: King move generation with different colors
TEST_F(KingMoveTest, DifferentColorKings) {
    // Place both kings on board
    board.setFromFEN("3k4/8/8/8/8/8/8/3K4 w - - 0 1");
    
    // Test white king moves
    moves.clear();
    generateKingMoves(board, moves, WHITE);
    int whiteMoves = moves.size();
    EXPECT_GT(whiteMoves, 0);
    
    // Test black king moves
    moves.clear();
    generateKingMoves(board, moves, BLACK);
    int blackMoves = moves.size();
    EXPECT_GT(blackMoves, 0);
    
    // Both should have same number of moves (symmetric position)
    EXPECT_EQ(whiteMoves, blackMoves);
}

// Test 15: Performance validation
TEST_F(KingMoveTest, PerformanceValidation) {
    // Complex position to test performance
    board.setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 4 4");
    
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        moves.clear();
        generateKingMoves(board, moves, WHITE);
        generateKingMoves(board, moves, BLACK);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    long long avgTimeNs = duration / iterations;
    
    // King move generation should be very fast (< 10 microseconds)
    EXPECT_LT(avgTimeNs, 10000); // 10 microseconds
}