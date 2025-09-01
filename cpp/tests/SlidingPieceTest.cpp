#include <gtest/gtest.h>
#include "Board.h"
#include "MoveGen.h"

using namespace opera;

class SlidingPieceTest : public ::testing::Test {
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
// BISHOP MOVE GENERATION TESTS
// ============================================================================

// Test 1: Starting position - no bishop moves available
TEST_F(SlidingPieceTest, StartingPositionBishopMovesWhite) {
    moves.clear();
    generateBishopMoves(board, moves, WHITE);
    
    // No bishop moves available in starting position (blocked by pawns)
    EXPECT_EQ(moves.size(), 0);
}

TEST_F(SlidingPieceTest, StartingPositionBishopMovesBlack) {
    moves.clear();
    generateBishopMoves(board, moves, BLACK);
    
    // No bishop moves available in starting position (blocked by pawns)
    EXPECT_EQ(moves.size(), 0);
}

// Test 2: Center bishop with maximum mobility
TEST_F(SlidingPieceTest, CenterBishopMaximumMoves) {
    // Place white bishop on d4 (center square)
    board.setFromFEN("8/8/8/8/3B4/8/8/8 w - - 0 1");
    moves.clear();
    generateBishopMoves(board, moves, WHITE);
    
    // Bishop on d4 should have 13 possible diagonal moves
    EXPECT_EQ(moves.size(), 13);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 13);
    
    // Check all four diagonal directions from d4
    // Northeast diagonal: e5, f6, g7, h8
    EXPECT_TRUE(containsMove(moves, D4, E5));
    EXPECT_TRUE(containsMove(moves, D4, F6));
    EXPECT_TRUE(containsMove(moves, D4, G7));
    EXPECT_TRUE(containsMove(moves, D4, H8));
    
    // Northwest diagonal: c5, b6, a7
    EXPECT_TRUE(containsMove(moves, D4, C5));
    EXPECT_TRUE(containsMove(moves, D4, B6));
    EXPECT_TRUE(containsMove(moves, D4, A7));
    
    // Southeast diagonal: e3, f2, g1
    EXPECT_TRUE(containsMove(moves, D4, E3));
    EXPECT_TRUE(containsMove(moves, D4, F2));
    EXPECT_TRUE(containsMove(moves, D4, G1));
    
    // Southwest diagonal: c3, b2, a1
    EXPECT_TRUE(containsMove(moves, D4, C3));
    EXPECT_TRUE(containsMove(moves, D4, B2));
    EXPECT_TRUE(containsMove(moves, D4, A1));
}

// Test 3: Corner bishop with limited moves
TEST_F(SlidingPieceTest, CornerBishopLimitedMoves) {
    // Place white bishop on a1 (corner)
    board.setFromFEN("8/8/8/8/8/8/8/B7 w - - 0 1");
    moves.clear();
    generateBishopMoves(board, moves, WHITE);
    
    // Bishop on a1 should have only 7 possible moves (northeast diagonal only)
    EXPECT_EQ(moves.size(), 7);
    EXPECT_EQ(countMovesFromSquare(moves, A1), 7);
    
    // Check northeast diagonal moves
    EXPECT_TRUE(containsMove(moves, A1, B2));
    EXPECT_TRUE(containsMove(moves, A1, C3));
    EXPECT_TRUE(containsMove(moves, A1, D4));
    EXPECT_TRUE(containsMove(moves, A1, E5));
    EXPECT_TRUE(containsMove(moves, A1, F6));
    EXPECT_TRUE(containsMove(moves, A1, G7));
    EXPECT_TRUE(containsMove(moves, A1, H8));
}

// Test 4: Bishop blocked by own pieces
TEST_F(SlidingPieceTest, BishopBlockedByOwnPieces) {
    // Place white bishop on d4 with white pieces blocking some diagonals
    // Bishop diagonals from d4 go through: c3-b2-a1, e3-f2-g1, c5-b6-a7, e5-f6-g7-h8
    // Place pieces to block further movement but allow adjacent moves
    board.setFromFEN("8/8/1P3P2/8/3B4/8/1P3P2/8 w - - 0 1");
    moves.clear();
    generateBishopMoves(board, moves, WHITE);
    
    // Bishop should have limited moves due to own pieces blocking diagonals
    EXPECT_EQ(moves.size(), 4);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 4);
    
    // Should be able to move to squares not blocked by own pieces
    EXPECT_TRUE(containsMove(moves, D4, C5));
    EXPECT_TRUE(containsMove(moves, D4, E5));
    EXPECT_TRUE(containsMove(moves, D4, C3));
    EXPECT_TRUE(containsMove(moves, D4, E3));
    
    // Should NOT be able to move to squares occupied by own pieces or beyond them
    EXPECT_FALSE(containsMove(moves, D4, B2)); // Blocked by pawn on B2
    EXPECT_FALSE(containsMove(moves, D4, F2)); // Blocked by pawn on F2
    EXPECT_FALSE(containsMove(moves, D4, B6)); // Blocked by pawn on B6
    EXPECT_FALSE(containsMove(moves, D4, F6)); // Blocked by pawn on F6
}

// Test 5: Bishop captures enemy pieces
TEST_F(SlidingPieceTest, BishopCapturesEnemyPieces) {
    // Place white bishop on d4 with black pieces on some diagonal squares
    // Bishop can capture pieces on the diagonals: b2, f2, b6, f6
    board.setFromFEN("8/8/1p3p2/8/3B4/8/1p3p2/8 w - - 0 1");
    moves.clear();
    generateBishopMoves(board, moves, WHITE);
    
    // Bishop should be able to capture enemy pieces and move to empty squares
    EXPECT_EQ(moves.size(), 8);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 8);
    EXPECT_EQ(countCaptureMovesFromSquare(moves, D4), 4);
    
    // Should be able to capture enemy pieces on diagonals
    EXPECT_TRUE(containsMove(moves, D4, B2));
    EXPECT_TRUE(containsMove(moves, D4, F2));
    EXPECT_TRUE(containsMove(moves, D4, B6));
    EXPECT_TRUE(containsMove(moves, D4, F6));
    
    // Should also be able to move to adjacent empty squares
    EXPECT_TRUE(containsMove(moves, D4, C5));
    EXPECT_TRUE(containsMove(moves, D4, E5));
    EXPECT_TRUE(containsMove(moves, D4, C3));
    EXPECT_TRUE(containsMove(moves, D4, E3));
}

// Test 6: Multiple bishops on board
TEST_F(SlidingPieceTest, MultipleBishops) {
    // Place multiple white bishops
    board.setFromFEN("8/8/8/8/3B4/8/8/B6B w - - 0 1");
    moves.clear();
    generateBishopMoves(board, moves, WHITE);
    
    // Bishop on d4: 12 moves (A1 blocked), Bishop on a1: 2 moves (D4 blocked), Bishop on h1: 7 moves  
    EXPECT_EQ(moves.size(), 21);
    
    EXPECT_EQ(countMovesFromSquare(moves, D4), 12);
    EXPECT_EQ(countMovesFromSquare(moves, A1), 2);
    EXPECT_EQ(countMovesFromSquare(moves, H1), 7);
}

// ============================================================================
// ROOK MOVE GENERATION TESTS  
// ============================================================================

// Test 7: Starting position - no rook moves available
TEST_F(SlidingPieceTest, StartingPositionRookMovesWhite) {
    moves.clear();
    generateRookMoves(board, moves, WHITE);
    
    // No rook moves available in starting position (blocked by pawns/pieces)
    EXPECT_EQ(moves.size(), 0);
}

TEST_F(SlidingPieceTest, StartingPositionRookMovesBlack) {
    moves.clear();
    generateRookMoves(board, moves, BLACK);
    
    // No rook moves available in starting position (blocked by pawns/pieces)
    EXPECT_EQ(moves.size(), 0);
}

// Test 8: Center rook with maximum mobility
TEST_F(SlidingPieceTest, CenterRookMaximumMoves) {
    // Place white rook on d4 (center square)
    board.setFromFEN("8/8/8/8/3R4/8/8/8 w - - 0 1");
    moves.clear();
    generateRookMoves(board, moves, WHITE);
    
    // Rook on d4 should have 14 possible moves (7 horizontal + 7 vertical)
    EXPECT_EQ(moves.size(), 14);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 14);
    
    // Check horizontal moves (rank 4)
    EXPECT_TRUE(containsMove(moves, D4, A4));
    EXPECT_TRUE(containsMove(moves, D4, B4));
    EXPECT_TRUE(containsMove(moves, D4, C4));
    EXPECT_TRUE(containsMove(moves, D4, E4));
    EXPECT_TRUE(containsMove(moves, D4, F4));
    EXPECT_TRUE(containsMove(moves, D4, G4));
    EXPECT_TRUE(containsMove(moves, D4, H4));
    
    // Check vertical moves (file d)
    EXPECT_TRUE(containsMove(moves, D4, D1));
    EXPECT_TRUE(containsMove(moves, D4, D2));
    EXPECT_TRUE(containsMove(moves, D4, D3));
    EXPECT_TRUE(containsMove(moves, D4, D5));
    EXPECT_TRUE(containsMove(moves, D4, D6));
    EXPECT_TRUE(containsMove(moves, D4, D7));
    EXPECT_TRUE(containsMove(moves, D4, D8));
}

// Test 9: Corner rook mobility
TEST_F(SlidingPieceTest, CornerRookMoves) {
    // Place white rook on a1 (corner)
    board.setFromFEN("8/8/8/8/8/8/8/R7 w - - 0 1");
    moves.clear();
    generateRookMoves(board, moves, WHITE);
    
    // Rook on a1 should have 14 possible moves (7 horizontal + 7 vertical)
    EXPECT_EQ(moves.size(), 14);
    EXPECT_EQ(countMovesFromSquare(moves, A1), 14);
    
    // Check horizontal moves (rank 1)
    EXPECT_TRUE(containsMove(moves, A1, B1));
    EXPECT_TRUE(containsMove(moves, A1, C1));
    EXPECT_TRUE(containsMove(moves, A1, D1));
    EXPECT_TRUE(containsMove(moves, A1, E1));
    EXPECT_TRUE(containsMove(moves, A1, F1));
    EXPECT_TRUE(containsMove(moves, A1, G1));
    EXPECT_TRUE(containsMove(moves, A1, H1));
    
    // Check vertical moves (file a)
    EXPECT_TRUE(containsMove(moves, A1, A2));
    EXPECT_TRUE(containsMove(moves, A1, A3));
    EXPECT_TRUE(containsMove(moves, A1, A4));
    EXPECT_TRUE(containsMove(moves, A1, A5));
    EXPECT_TRUE(containsMove(moves, A1, A6));
    EXPECT_TRUE(containsMove(moves, A1, A7));
    EXPECT_TRUE(containsMove(moves, A1, A8));
}

// Test 10: Rook blocked by own pieces
TEST_F(SlidingPieceTest, RookBlockedByOwnPieces) {
    // Place white rook on d4 with white pieces blocking some directions
    board.setFromFEN("8/8/8/3P4/1P1R1P2/8/3P4/8 w - - 0 1");
    moves.clear();
    generateRookMoves(board, moves, WHITE);
    
    // Rook should have limited moves due to own pieces blocking
    EXPECT_EQ(moves.size(), 3);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 3);
    
    // Should be able to move to empty squares not blocked by own pieces
    EXPECT_TRUE(containsMove(moves, D4, C4));
    EXPECT_TRUE(containsMove(moves, D4, E4));
    EXPECT_TRUE(containsMove(moves, D4, D3));
    
    // Should NOT be able to move to squares occupied by own pieces
    EXPECT_FALSE(containsMove(moves, D4, B4));
    EXPECT_FALSE(containsMove(moves, D4, F4));
    EXPECT_FALSE(containsMove(moves, D4, D2));
    EXPECT_FALSE(containsMove(moves, D4, D5));
}

// Test 11: Rook captures enemy pieces
TEST_F(SlidingPieceTest, RookCapturesEnemyPieces) {
    // Place white rook on d4 with black pieces on some orthogonal squares
    board.setFromFEN("8/8/8/3p4/1p1R1p2/8/3p4/8 w - - 0 1");
    moves.clear();
    generateRookMoves(board, moves, WHITE);
    
    // Rook should be able to capture enemy pieces and move to empty squares
    EXPECT_EQ(moves.size(), 7);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 7);
    EXPECT_EQ(countCaptureMovesFromSquare(moves, D4), 4);
    
    // Should be able to capture enemy pieces
    EXPECT_TRUE(containsMove(moves, D4, B4));
    EXPECT_TRUE(containsMove(moves, D4, F4));
    EXPECT_TRUE(containsMove(moves, D4, D2));
    EXPECT_TRUE(containsMove(moves, D4, D5));
    
    // Should also be able to move to adjacent empty squares
    EXPECT_TRUE(containsMove(moves, D4, C4));
    EXPECT_TRUE(containsMove(moves, D4, E4));
    EXPECT_TRUE(containsMove(moves, D4, D3));
}

// ============================================================================
// QUEEN MOVE GENERATION TESTS
// ============================================================================

// Test 12: Starting position - no queen moves available
TEST_F(SlidingPieceTest, StartingPositionQueenMovesWhite) {
    moves.clear();
    generateQueenMoves(board, moves, WHITE);
    
    // No queen moves available in starting position (blocked by pawns/pieces)
    EXPECT_EQ(moves.size(), 0);
}

TEST_F(SlidingPieceTest, StartingPositionQueenMovesBlack) {
    moves.clear();
    generateQueenMoves(board, moves, BLACK);
    
    // No queen moves available in starting position (blocked by pawns/pieces)
    EXPECT_EQ(moves.size(), 0);
}

// Test 13: Center queen with maximum mobility
TEST_F(SlidingPieceTest, CenterQueenMaximumMoves) {
    // Place white queen on d4 (center square)
    board.setFromFEN("8/8/8/8/3Q4/8/8/8 w - - 0 1");
    moves.clear();
    generateQueenMoves(board, moves, WHITE);
    
    // Queen on d4 should have 27 possible moves (14 rook + 13 bishop)
    EXPECT_EQ(moves.size(), 27);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 27);
    
    // Check some key horizontal moves
    EXPECT_TRUE(containsMove(moves, D4, A4));
    EXPECT_TRUE(containsMove(moves, D4, H4));
    
    // Check some key vertical moves  
    EXPECT_TRUE(containsMove(moves, D4, D1));
    EXPECT_TRUE(containsMove(moves, D4, D8));
    
    // Check some key diagonal moves
    EXPECT_TRUE(containsMove(moves, D4, A1));
    EXPECT_TRUE(containsMove(moves, D4, G7));
    EXPECT_TRUE(containsMove(moves, D4, A7));
    EXPECT_TRUE(containsMove(moves, D4, H8));
}

// Test 14: Queen blocked by mixed pieces
TEST_F(SlidingPieceTest, QueenBlockedByMixedPieces) {
    // Place white queen on d4 with mix of own and enemy pieces around
    board.setFromFEN("8/8/2pPp3/3Q4/1PqRp3/8/2pPp3/8 w - - 0 1");
    moves.clear();
    generateQueenMoves(board, moves, WHITE);
    
    // Queen should be able to capture enemy pieces but not move to own pieces
    EXPECT_GT(moves.size(), 0);  // Should have some moves available
    
    // Should be able to capture enemy pieces
    bool hasCaptures = countCaptureMovesFromSquare(moves, D5) > 0;
    EXPECT_TRUE(hasCaptures);
}

// Test 15: Queen in complex tactical position
TEST_F(SlidingPieceTest, QueenTacticalPosition) {
    // Use a more complex middle-game position
    board.setFromFEN("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    moves.clear();
    generateQueenMoves(board, moves, WHITE);
    
    // White queen on d1 should have some moves available
    EXPECT_GT(moves.size(), 0);
    
    // All moves should be valid (no capturing own pieces)
    for (size_t i = 0; i < moves.size(); ++i) {
        Square to = moves[i].to();
        Piece targetPiece = board.getPiece(to);
        
        if (targetPiece != NO_PIECE) {
            // If capturing a piece, it must be enemy piece
            EXPECT_NE(colorOf(targetPiece), WHITE);
            EXPECT_TRUE(moves[i].isCapture());
        }
    }
}

// ============================================================================
// EDGE CASE AND VALIDATION TESTS
// ============================================================================

// Test 16: Empty board edge case
TEST_F(SlidingPieceTest, EmptyBoardEdgeCase) {
    // Empty board
    board.setFromFEN("8/8/8/8/8/8/8/8 w - - 0 1");
    
    moves.clear();
    generateBishopMoves(board, moves, WHITE);
    EXPECT_EQ(moves.size(), 0);
    
    moves.clear();
    generateRookMoves(board, moves, WHITE);
    EXPECT_EQ(moves.size(), 0);
    
    moves.clear();
    generateQueenMoves(board, moves, WHITE);
    EXPECT_EQ(moves.size(), 0);
}

// Test 17: Move type validation
TEST_F(SlidingPieceTest, MoveTypesAreCorrect) {
    // Place pieces for various move scenarios
    board.setFromFEN("8/8/2p1p3/8/3Q4/8/8/8 w - - 0 1");
    moves.clear();
    generateQueenMoves(board, moves, WHITE);
    
    // All sliding piece moves should be NORMAL type
    for (size_t i = 0; i < moves.size(); ++i) {
        EXPECT_EQ(moves[i].type(), MoveGen::MoveType::NORMAL);
        EXPECT_FALSE(moves[i].isPromotion());
        EXPECT_FALSE(moves[i].isCastling());
        EXPECT_FALSE(moves[i].isEnPassant());
        EXPECT_FALSE(moves[i].isDoublePawnPush());
    }
}

// Test 18: Performance validation
TEST_F(SlidingPieceTest, PerformanceValidation) {
    // Complex position to test performance
    board.setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 4 4");
    
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        moves.clear();
        generateBishopMoves(board, moves, WHITE);
        generateRookMoves(board, moves, WHITE);  
        generateQueenMoves(board, moves, WHITE);
        generateBishopMoves(board, moves, BLACK);
        generateRookMoves(board, moves, BLACK);
        generateQueenMoves(board, moves, BLACK);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    long long avgTimeNs = duration / iterations;
    
    // Sliding piece move generation should be reasonably fast (< 50 microseconds)
    EXPECT_LT(avgTimeNs, 50000); // 50 microseconds
}