#include <gtest/gtest.h>
#include "Board.h"
#include "MoveGen.h"

using namespace opera;

class KnightMoveTest : public ::testing::Test {
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
};

// Test 1: Starting position knight moves for WHITE
TEST_F(KnightMoveTest, StartingPositionWhiteKnights) {
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    // White has 2 knights on b1 and g1, each has 2 possible moves
    EXPECT_EQ(moves.size(), 4);
    
    // Knight on b1 can go to a3 and c3
    EXPECT_TRUE(containsMove(moves, B1, A3));
    EXPECT_TRUE(containsMove(moves, B1, C3));
    EXPECT_EQ(countMovesFromSquare(moves, B1), 2);
    
    // Knight on g1 can go to f3 and h3
    EXPECT_TRUE(containsMove(moves, G1, F3));
    EXPECT_TRUE(containsMove(moves, G1, H3));
    EXPECT_EQ(countMovesFromSquare(moves, G1), 2);
}

// Test 2: Starting position knight moves for BLACK
TEST_F(KnightMoveTest, StartingPositionBlackKnights) {
    moves.clear();
    generateKnightMoves(board, moves, BLACK);
    
    // Black has 2 knights on b8 and g8, each has 2 possible moves
    EXPECT_EQ(moves.size(), 4);
    
    // Knight on b8 can go to a6 and c6
    EXPECT_TRUE(containsMove(moves, B8, A6));
    EXPECT_TRUE(containsMove(moves, B8, C6));
    EXPECT_EQ(countMovesFromSquare(moves, B8), 2);
    
    // Knight on g8 can go to f6 and h6
    EXPECT_TRUE(containsMove(moves, G8, F6));
    EXPECT_TRUE(containsMove(moves, G8, H6));
    EXPECT_EQ(countMovesFromSquare(moves, G8), 2);
}

// Test 3: Knight in center with all 8 moves available
TEST_F(KnightMoveTest, CenterKnightAllMoves) {
    // Place a white knight on d4 (center of board)
    board.setFromFEN("8/8/8/8/3N4/8/8/8 w - - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    // Knight on d4 should have all 8 possible moves
    EXPECT_EQ(moves.size(), 8);
    EXPECT_EQ(countMovesFromSquare(moves, D4), 8);
    
    // Check all 8 knight moves from d4
    EXPECT_TRUE(containsMove(moves, D4, B3)); // 2 left, 1 down
    EXPECT_TRUE(containsMove(moves, D4, B5)); // 2 left, 1 up
    EXPECT_TRUE(containsMove(moves, D4, C2)); // 1 left, 2 down
    EXPECT_TRUE(containsMove(moves, D4, C6)); // 1 left, 2 up
    EXPECT_TRUE(containsMove(moves, D4, E2)); // 1 right, 2 down
    EXPECT_TRUE(containsMove(moves, D4, E6)); // 1 right, 2 up
    EXPECT_TRUE(containsMove(moves, D4, F3)); // 2 right, 1 down
    EXPECT_TRUE(containsMove(moves, D4, F5)); // 2 right, 1 up
}

// Test 4: Knight in corner with limited moves
TEST_F(KnightMoveTest, CornerKnightLimitedMoves) {
    // Place a white knight on a1 (corner)
    board.setFromFEN("8/8/8/8/8/8/8/N7 w - - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    // Knight on a1 should have only 2 possible moves
    EXPECT_EQ(moves.size(), 2);
    EXPECT_EQ(countMovesFromSquare(moves, A1), 2);
    
    // Check the 2 valid knight moves from a1
    EXPECT_TRUE(containsMove(moves, A1, B3)); // 1 right, 2 up
    EXPECT_TRUE(containsMove(moves, A1, C2)); // 2 right, 1 up
}

// Test 5: Knight on edge with limited moves
TEST_F(KnightMoveTest, EdgeKnightLimitedMoves) {
    // Place a white knight on a4 (left edge)
    board.setFromFEN("8/8/8/8/N7/8/8/8 w - - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    // Knight on a4 should have 4 possible moves (edge position)
    EXPECT_EQ(moves.size(), 4);
    EXPECT_EQ(countMovesFromSquare(moves, A4), 4);
    
    // Check the 4 valid knight moves from a4
    EXPECT_TRUE(containsMove(moves, A4, B2)); // 1 right, 2 down
    EXPECT_TRUE(containsMove(moves, A4, B6)); // 1 right, 2 up
    EXPECT_TRUE(containsMove(moves, A4, C3)); // 2 right, 1 down
    EXPECT_TRUE(containsMove(moves, A4, C5)); // 2 right, 1 up
}

// Test 6: Knight captures enemy pieces
TEST_F(KnightMoveTest, KnightCaptures) {
    // Place white knight on d4 with black pieces on some knight target squares
    // Knight from d4 can reach: c2, e2, b3, f3, b5, f5, c6, e6
    board.setFromFEN("8/8/2p1p3/8/3N4/5p2/4p3/8 w - - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    // Knight should be able to move to empty squares and capture enemy pieces
    // Target squares: c2(empty), e2(pawn), b3(empty), f3(pawn), b5(empty), f5(empty), c6(pawn), e6(pawn)
    EXPECT_EQ(moves.size(), 8);
    
    // Check that captures are marked correctly
    bool foundC6Capture = false, foundE6Capture = false, foundF3Capture = false, foundE2Capture = false;
    for (size_t i = 0; i < moves.size(); ++i) {
        if (moves[i].from() == D4 && moves[i].to() == C6) {
            EXPECT_TRUE(moves[i].isCapture());
            EXPECT_EQ(moves[i].capturedPiece(), BLACK_PAWN);
            foundC6Capture = true;
        } else if (moves[i].from() == D4 && moves[i].to() == E6) {
            EXPECT_TRUE(moves[i].isCapture());
            EXPECT_EQ(moves[i].capturedPiece(), BLACK_PAWN);
            foundE6Capture = true;
        } else if (moves[i].from() == D4 && moves[i].to() == F3) {
            EXPECT_TRUE(moves[i].isCapture());
            EXPECT_EQ(moves[i].capturedPiece(), BLACK_PAWN);
            foundF3Capture = true;
        } else if (moves[i].from() == D4 && moves[i].to() == E2) {
            EXPECT_TRUE(moves[i].isCapture());
            EXPECT_EQ(moves[i].capturedPiece(), BLACK_PAWN);
            foundE2Capture = true;
        }
    }
    
    EXPECT_TRUE(foundC6Capture);
    EXPECT_TRUE(foundE6Capture);
    EXPECT_TRUE(foundF3Capture);
    EXPECT_TRUE(foundE2Capture);
}

// Test 7: Knight blocked by own pieces
TEST_F(KnightMoveTest, KnightBlockedByOwnPieces) {
    // Place white knight on d4 with white pieces on some knight target squares
    // Knight from d4 can reach: c2, e2, b3, f3, b5, f5, c6, e6
    board.setFromFEN("8/8/2P1P3/8/3N4/5P2/4P3/8 w - - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    // Knight should only be able to move to empty squares, not capture own pieces
    // Target squares: c2(empty), e2(WHITE_PAWN-blocked), b3(empty), f3(WHITE_PAWN-blocked), 
    //                 b5(empty), f5(empty), c6(WHITE_PAWN-blocked), e6(WHITE_PAWN-blocked)
    EXPECT_EQ(moves.size(), 4); // Only 4 moves available (4 squares blocked by own pieces)
    
    // Verify knight cannot move to squares occupied by own pieces
    EXPECT_FALSE(containsMove(moves, D4, C6)); // Blocked by own pawn
    EXPECT_FALSE(containsMove(moves, D4, E6)); // Blocked by own pawn
    EXPECT_FALSE(containsMove(moves, D4, F3)); // Blocked by own pawn
    EXPECT_FALSE(containsMove(moves, D4, E2)); // Blocked by own pawn
    
    // Verify knight can move to empty squares
    EXPECT_TRUE(containsMove(moves, D4, B3));
    EXPECT_TRUE(containsMove(moves, D4, B5));
    EXPECT_TRUE(containsMove(moves, D4, C2));
    EXPECT_TRUE(containsMove(moves, D4, F5));
}

// Test 8: Multiple knights on board
TEST_F(KnightMoveTest, MultipleKnights) {
    // Place multiple white knights
    board.setFromFEN("8/8/8/8/3N4/8/8/N6N w - - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    // Knight on d4: 8 moves, Knight on a1: 2 moves, Knight on h1: 2 moves
    EXPECT_EQ(moves.size(), 12);
    
    EXPECT_EQ(countMovesFromSquare(moves, D4), 8);
    EXPECT_EQ(countMovesFromSquare(moves, A1), 2);
    EXPECT_EQ(countMovesFromSquare(moves, H1), 2);
}

// Test 9: Empty board (no knights)
TEST_F(KnightMoveTest, NoKnights) {
    // Empty board
    board.setFromFEN("8/8/8/8/8/8/8/8 w - - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    EXPECT_EQ(moves.size(), 0);
    
    moves.clear();
    generateKnightMoves(board, moves, BLACK);
    EXPECT_EQ(moves.size(), 0);
}

// Test 10: Knight move types are correct
TEST_F(KnightMoveTest, MoveTypesCorrect) {
    // Place white knight on d4
    board.setFromFEN("8/8/8/8/3N4/8/8/8 w - - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    // All knight moves should be NORMAL type (knights can't promote, castle, etc.)
    for (size_t i = 0; i < moves.size(); ++i) {
        EXPECT_EQ(moves[i].type(), MoveGen::MoveType::NORMAL);
        EXPECT_FALSE(moves[i].isPromotion());
        EXPECT_FALSE(moves[i].isCastling());
        EXPECT_FALSE(moves[i].isEnPassant());
        EXPECT_FALSE(moves[i].isDoublePawnPush());
    }
}

// Test 11: Knight move pattern validation
TEST_F(KnightMoveTest, MovePatternValidation) {
    // Place white knight on e4
    board.setFromFEN("8/8/8/8/4N3/8/8/8 w - - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    EXPECT_EQ(moves.size(), 8);
    
    // Validate that all moves are exactly L-shaped (2+1 or 1+2 squares)
    for (size_t i = 0; i < moves.size(); ++i) {
        if (moves[i].from() == E4) {
            Square to = moves[i].to();
            int fileDiff = abs(fileOf(to) - fileOf(E4));
            int rankDiff = abs(rankOf(to) - rankOf(E4));
            
            // Knight moves are always 2+1 or 1+2 pattern
            EXPECT_TRUE((fileDiff == 2 && rankDiff == 1) || (fileDiff == 1 && rankDiff == 2));
        }
    }
}

// Test 12: Board boundary validation
TEST_F(KnightMoveTest, BoardBoundaryValidation) {
    // Test knights on various edge positions
    
    // Knight on h8 (top-right corner)
    board.setFromFEN("7N/8/8/8/8/8/8/8 w - - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    EXPECT_EQ(moves.size(), 2);
    EXPECT_TRUE(containsMove(moves, H8, F7)); // 2 left, 1 down
    EXPECT_TRUE(containsMove(moves, H8, G6)); // 1 left, 2 down
    
    // Verify no out-of-bounds moves
    for (size_t i = 0; i < moves.size(); ++i) {
        Square to = moves[i].to();
        EXPECT_GE(static_cast<int>(to), static_cast<int>(A1));
        EXPECT_LE(static_cast<int>(to), static_cast<int>(H8));
    }
}

// Test 13: Complex position with mixed pieces
TEST_F(KnightMoveTest, ComplexPosition) {
    // Kiwipete-like position with knights
    board.setFromFEN("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    // Should have white knights on f3 and h6
    EXPECT_GT(moves.size(), 0);
    
    // All generated moves should be valid
    for (size_t i = 0; i < moves.size(); ++i) {
        Square from = moves[i].from();
        Square to = moves[i].to();
        
        // Verify it's actually a knight move
        Piece piece = board.getPiece(from);
        EXPECT_EQ(piece, WHITE_KNIGHT);
        
        // Verify L-shaped pattern
        int fileDiff = abs(fileOf(to) - fileOf(from));
        int rankDiff = abs(rankOf(to) - rankOf(from));
        EXPECT_TRUE((fileDiff == 2 && rankDiff == 1) || (fileDiff == 1 && rankDiff == 2));
        
        // Verify destination is not occupied by own piece
        Piece targetPiece = board.getPiece(to);
        if (targetPiece != NO_PIECE) {
            EXPECT_NE(colorOf(targetPiece), WHITE);
            EXPECT_TRUE(moves[i].isCapture());
        }
    }
}

// Test 14: Performance validation
TEST_F(KnightMoveTest, PerformanceValidation) {
    // Complex position to test performance
    board.setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 4 4");
    
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        moves.clear();
        generateKnightMoves(board, moves, WHITE);
        generateKnightMoves(board, moves, BLACK);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    long long avgTimeNs = duration / iterations;
    
    // Knight move generation should be very fast (< 10 microseconds)
    EXPECT_LT(avgTimeNs, 10000); // 10 microseconds
}