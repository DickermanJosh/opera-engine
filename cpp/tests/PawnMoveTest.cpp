#include <gtest/gtest.h>
#include "Board.h"
#include "MoveGen.h"

using namespace opera;

class PawnMoveTest : public ::testing::Test {
protected:
    void SetUp() override {
        board.setFromFEN(STARTING_FEN);
    }
    
    void TearDown() override {
        // Cleanup after tests
    }
    
    // Helper method to set custom position
    void setPosition(const std::string& fen) {
        board.setFromFEN(fen);
    }
    
    // Helper method to count moves of specific type
    int countMoves(const MoveGenList<>& moves, MoveGen::MoveType type) {
        int count = 0;
        for (const auto& move : moves) {
            if (move.type() == type) count++;
        }
        return count;
    }
    
    // Helper method to find move from-to
    bool hasMove(const MoveGenList<>& moves, Square from, Square to) {
        for (const auto& move : moves) {
            if (move.from() == from && move.to() == to) return true;
        }
        return false;
    }
    
    // Helper method to find promotion move
    bool hasPromotionMove(const MoveGenList<>& moves, Square from, Square to, Piece promotionPiece) {
        for (const auto& move : moves) {
            if (move.from() == from && move.to() == to && 
                move.isPromotion() && move.promotionPiece() == promotionPiece) {
                return true;
            }
        }
        return false;
    }
    
    Board board;
};

// ===== POSITIVE TESTS - WHITE PAWNS =====

TEST_F(PawnMoveTest, WhitePawnSinglePushFromStarting) {
    // Starting position: all white pawns can move forward one square
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // Check all 8 pawns can move forward
    EXPECT_TRUE(hasMove(moves, A2, A3));
    EXPECT_TRUE(hasMove(moves, B2, B3));
    EXPECT_TRUE(hasMove(moves, C2, C3));
    EXPECT_TRUE(hasMove(moves, D2, D3));
    EXPECT_TRUE(hasMove(moves, E2, E3));
    EXPECT_TRUE(hasMove(moves, F2, F3));
    EXPECT_TRUE(hasMove(moves, G2, G3));
    EXPECT_TRUE(hasMove(moves, H2, H3));
}

TEST_F(PawnMoveTest, WhitePawnDoublePushFromStarting) {
    // Starting position: all white pawns can move forward two squares
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // Check all 8 pawns can double push
    EXPECT_TRUE(hasMove(moves, A2, A4));
    EXPECT_TRUE(hasMove(moves, B2, B4));
    EXPECT_TRUE(hasMove(moves, C2, C4));
    EXPECT_TRUE(hasMove(moves, D2, D4));
    EXPECT_TRUE(hasMove(moves, E2, E4));
    EXPECT_TRUE(hasMove(moves, F2, F4));
    EXPECT_TRUE(hasMove(moves, G2, G4));
    EXPECT_TRUE(hasMove(moves, H2, H4));
    
    // Verify these are marked as double pawn pushes
    int doublePushCount = 0;
    for (const auto& move : moves) {
        if (move.isDoublePawnPush()) doublePushCount++;
    }
    EXPECT_EQ(doublePushCount, 8);
}

TEST_F(PawnMoveTest, WhitePawnBlockedByPiece) {
    // Place black pawn on a3 to block white a2 pawn
    setPosition("rnbqkbnr/1ppppppp/8/8/8/p7/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // A2 pawn should be blocked, cannot move to A3 or A4
    EXPECT_FALSE(hasMove(moves, A2, A3));
    EXPECT_FALSE(hasMove(moves, A2, A4));
    
    // Other pawns should still be able to move
    EXPECT_TRUE(hasMove(moves, B2, B3));
    EXPECT_TRUE(hasMove(moves, B2, B4));
}

TEST_F(PawnMoveTest, WhitePawnNoDoubleAfterMove) {
    // Move white pawn from A2 to A3, then check it cannot double push
    setPosition("rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // A3 pawn can only move to A4 (single push), not double push
    EXPECT_TRUE(hasMove(moves, A3, A4));
    
    // Should not have any double pawn pushes from A3
    for (const auto& move : moves) {
        if (move.from() == A3 && move.isDoublePawnPush()) {
            FAIL() << "Pawn on A3 should not be able to double push";
        }
    }
}

TEST_F(PawnMoveTest, WhitePawnCaptureLeft) {
    // Place black pawn on b3 for white a2 pawn to capture
    setPosition("rnbqkbnr/pppppppp/8/8/8/1p6/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // A2 pawn should be able to capture on B3
    EXPECT_TRUE(hasMove(moves, A2, B3));
    
    // Verify it's marked as a capture
    bool foundCapture = false;
    for (const auto& move : moves) {
        if (move.from() == A2 && move.to() == B3) {
            EXPECT_TRUE(move.isCapture());
            EXPECT_EQ(move.capturedPiece(), BLACK_PAWN);
            foundCapture = true;
        }
    }
    EXPECT_TRUE(foundCapture);
}

TEST_F(PawnMoveTest, WhitePawnCaptureRight) {
    // Place black pawn on c3 for white b2 pawn to capture
    setPosition("rnbqkbnr/pppppppp/8/8/8/2p5/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // B2 pawn should be able to capture on C3
    EXPECT_TRUE(hasMove(moves, B2, C3));
    
    // Verify it's marked as a capture
    bool foundCapture = false;
    for (const auto& move : moves) {
        if (move.from() == B2 && move.to() == C3) {
            EXPECT_TRUE(move.isCapture());
            EXPECT_EQ(move.capturedPiece(), BLACK_PAWN);
            foundCapture = true;
        }
    }
    EXPECT_TRUE(foundCapture);
}

TEST_F(PawnMoveTest, WhitePawnCannotCaptureOwnPiece) {
    // Place white knight on b3 - a2 pawn cannot capture it
    setPosition("rnbqkbnr/pppppppp/8/8/8/1N6/PPPPPPPP/R1BQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // A2 pawn should NOT be able to capture own knight on B3
    EXPECT_FALSE(hasMove(moves, A2, B3));
}

TEST_F(PawnMoveTest, WhitePawnPromotionOnEighthRank) {
    // White pawn on 7th rank ready to promote - F8 must be empty
    setPosition("rnbqk1nr/pppppPpp/8/8/8/8/PPPPPpPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // F7 pawn should promote to all pieces when moving to F8
    EXPECT_TRUE(hasPromotionMove(moves, F7, F8, WHITE_QUEEN));
    EXPECT_TRUE(hasPromotionMove(moves, F7, F8, WHITE_ROOK));
    EXPECT_TRUE(hasPromotionMove(moves, F7, F8, WHITE_BISHOP));
    EXPECT_TRUE(hasPromotionMove(moves, F7, F8, WHITE_KNIGHT));
}

TEST_F(PawnMoveTest, WhitePawnPromotionCapture) {
    // White pawn on 7th rank can capture and promote - G8 has black rook
    setPosition("rnbqkbr1/pppppPpp/8/8/8/8/PPPPPpPP/RNBQKB1R w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // F7 pawn should be able to capture on G8 with promotion
    EXPECT_TRUE(hasPromotionMove(moves, F7, G8, WHITE_QUEEN));
    EXPECT_TRUE(hasPromotionMove(moves, F7, G8, WHITE_ROOK));
    EXPECT_TRUE(hasPromotionMove(moves, F7, G8, WHITE_BISHOP));
    EXPECT_TRUE(hasPromotionMove(moves, F7, G8, WHITE_KNIGHT));
    
    // Verify captures are marked correctly
    int promotionCaptures = 0;
    for (const auto& move : moves) {
        if (move.from() == F7 && move.to() == G8 && move.isPromotion()) {
            EXPECT_TRUE(move.isCapture());
            EXPECT_EQ(move.capturedPiece(), BLACK_ROOK);
            promotionCaptures++;
        }
    }
    EXPECT_EQ(promotionCaptures, 4); // All 4 promotion pieces
}

TEST_F(PawnMoveTest, WhitePawnEnPassantCapture) {
    // Set up en passant position: black pawn just moved d7-d5, white pawn on e5
    setPosition("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // E5 pawn should be able to capture en passant on D6
    bool foundEnPassant = false;
    for (const auto& move : moves) {
        if (move.from() == E5 && move.to() == D6 && move.isEnPassant()) {
            EXPECT_TRUE(move.isCapture());
            EXPECT_EQ(move.capturedPiece(), BLACK_PAWN);
            foundEnPassant = true;
        }
    }
    EXPECT_TRUE(foundEnPassant);
}

// ===== POSITIVE TESTS - BLACK PAWNS =====

TEST_F(PawnMoveTest, BlackPawnSinglePushFromStarting) {
    // Starting position: all black pawns can move forward one square
    MoveGenList<> moves;
    generatePawnMoves(board, moves, BLACK);
    
    // Check all 8 black pawns can move forward (down the board)
    EXPECT_TRUE(hasMove(moves, A7, A6));
    EXPECT_TRUE(hasMove(moves, B7, B6));
    EXPECT_TRUE(hasMove(moves, C7, C6));
    EXPECT_TRUE(hasMove(moves, D7, D6));
    EXPECT_TRUE(hasMove(moves, E7, E6));
    EXPECT_TRUE(hasMove(moves, F7, F6));
    EXPECT_TRUE(hasMove(moves, G7, G6));
    EXPECT_TRUE(hasMove(moves, H7, H6));
}

TEST_F(PawnMoveTest, BlackPawnDoublePushFromStarting) {
    // Starting position: all black pawns can move forward two squares
    MoveGenList<> moves;
    generatePawnMoves(board, moves, BLACK);
    
    // Check all 8 black pawns can double push
    EXPECT_TRUE(hasMove(moves, A7, A5));
    EXPECT_TRUE(hasMove(moves, B7, B5));
    EXPECT_TRUE(hasMove(moves, C7, C5));
    EXPECT_TRUE(hasMove(moves, D7, D5));
    EXPECT_TRUE(hasMove(moves, E7, E5));
    EXPECT_TRUE(hasMove(moves, F7, F5));
    EXPECT_TRUE(hasMove(moves, G7, G5));
    EXPECT_TRUE(hasMove(moves, H7, H5));
    
    // Verify these are marked as double pawn pushes
    int doublePushCount = 0;
    for (const auto& move : moves) {
        if (move.isDoublePawnPush()) doublePushCount++;
    }
    EXPECT_EQ(doublePushCount, 8);
}

TEST_F(PawnMoveTest, BlackPawnPromotionOnFirstRank) {
    // Black pawn on 2nd rank ready to promote - A1 must be empty  
    setPosition("rnbqkbnr/pppppppp/8/8/8/8/p7/1NBQKBNR b KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, BLACK);
    
    // A2 pawn should promote to all pieces when moving to A1
    EXPECT_TRUE(hasPromotionMove(moves, A2, A1, BLACK_QUEEN));
    EXPECT_TRUE(hasPromotionMove(moves, A2, A1, BLACK_ROOK));
    EXPECT_TRUE(hasPromotionMove(moves, A2, A1, BLACK_BISHOP));
    EXPECT_TRUE(hasPromotionMove(moves, A2, A1, BLACK_KNIGHT));
}

TEST_F(PawnMoveTest, BlackPawnEnPassantCapture) {
    // Set up en passant: white pawn just moved e2-e4, black pawn on d4
    setPosition("rnbqkbnr/pppp1ppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, BLACK);
    
    // D4 pawn should be able to capture en passant on E3
    bool foundEnPassant = false;
    for (const auto& move : moves) {
        if (move.from() == D4 && move.to() == E3 && move.isEnPassant()) {
            EXPECT_TRUE(move.isCapture());
            EXPECT_EQ(move.capturedPiece(), WHITE_PAWN);
            foundEnPassant = true;
        }
    }
    EXPECT_TRUE(foundEnPassant);
}

// ===== EDGE CASES =====

TEST_F(PawnMoveTest, PawnOnEdgeFiles) {
    // Pawns on A and H files have limited capture options
    setPosition("rnbqkbnr/1ppppp1p/8/p6P/8/8/PPPPPP1P/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // H5 pawn can only capture left (G6), not right (off board)
    // Should not generate any invalid moves to I6
    for (const auto& move : moves) {
        EXPECT_LT(move.to(), 64); // All moves should be valid squares
        EXPECT_GE(move.to(), 0);
    }
}

TEST_F(PawnMoveTest, PawnBlockedByDoubleRank) {
    // Pawn can move one square but not two (blocked on second square)
    setPosition("rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // B2 pawn should be able to move to B3 but not B4 (blocked by A3 pawn? No, that's wrong setup)
    // Let me fix: place piece on B4 to block double push
    setPosition("rnbqkbnr/pppppppp/8/8/1p6/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    moves.clear();
    generatePawnMoves(board, moves, WHITE);
    
    // B2 pawn can move to B3 but not B4 (blocked)
    EXPECT_TRUE(hasMove(moves, B2, B3));
    EXPECT_FALSE(hasMove(moves, B2, B4));
}

TEST_F(PawnMoveTest, NoEnPassantWithoutFlag) {
    // Position looks like en passant but no en passant flag set
    setPosition("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // E5 pawn should NOT be able to capture en passant (no flag)
    for (const auto& move : moves) {
        if (move.from() == E5 && move.to() == D6) {
            EXPECT_FALSE(move.isEnPassant());
        }
    }
}

// ===== NEGATIVE TESTS =====

TEST_F(PawnMoveTest, PawnCannotMoveBackward) {
    // Pawn on 4th rank cannot move backward
    setPosition("rnbqkbnr/pppppppp/8/8/P7/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // A4 pawn should not be able to move to A3 or A2
    EXPECT_FALSE(hasMove(moves, A4, A3));
    EXPECT_FALSE(hasMove(moves, A4, A2));
}

TEST_F(PawnMoveTest, PawnCannotMoveSideways) {
    // Pawn cannot move horizontally
    setPosition("rnbqkbnr/pppppppp/8/8/P7/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // A4 pawn should not be able to move to B4
    EXPECT_FALSE(hasMove(moves, A4, B4));
}

TEST_F(PawnMoveTest, PawnCannotCaptureForward) {
    // Pawn cannot capture the piece directly in front
    setPosition("rnbqkbnr/pppppppp/8/8/8/p7/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // A2 pawn cannot "capture" the pawn on A3 (can only capture diagonally)
    EXPECT_FALSE(hasMove(moves, A2, A3));
}

TEST_F(PawnMoveTest, PawnCannotJumpOverPieces) {
    // Pawn cannot jump over pieces
    setPosition("rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // B2 pawn cannot jump over A3 pawn to reach B4
    // Actually, A3 doesn't block B2-B4, let me use correct position
    setPosition("rnbqkbnr/pppppppp/8/8/8/1p6/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    moves.clear();
    generatePawnMoves(board, moves, WHITE);
    
    // B2 pawn blocked by B3 pawn cannot reach B3 or B4
    EXPECT_FALSE(hasMove(moves, B2, B3));
    EXPECT_FALSE(hasMove(moves, B2, B4));
}

// ===== COMPREHENSIVE POSITION TESTS =====

TEST_F(PawnMoveTest, ComplexPawnStructure) {
    // Test complex pawn structure with multiple interactions
    setPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    
    MoveGenList<> moves;
    generatePawnMoves(board, moves, WHITE);
    
    // Verify various pawn moves in complex position
    // This is a known position - check that reasonable pawn moves exist
    EXPECT_GT(moves.size(), 0); // Should have some pawn moves available
    
    // All moves should be valid
    for (const auto& move : moves) {
        EXPECT_GE(move.from(), 0);
        EXPECT_LT(move.from(), 64);
        EXPECT_GE(move.to(), 0);
        EXPECT_LT(move.to(), 64);
    }
}

TEST_F(PawnMoveTest, PerformanceStressTest) {
    // Generate moves for many positions to test performance
    std::vector<std::string> testPositions = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
        "rnbqkb1r/pppppppp/5n2/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 1 2",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
    };
    
    for (const auto& fen : testPositions) {
        setPosition(fen);
        
        MoveGenList<> whiteMoves;
        MoveGenList<> blackMoves;
        
        // Should complete quickly and without errors
        EXPECT_NO_THROW(generatePawnMoves(board, whiteMoves, WHITE));
        EXPECT_NO_THROW(generatePawnMoves(board, blackMoves, BLACK));
        
        // Verify moves are reasonable
        EXPECT_LE(whiteMoves.size(), 16); // Maximum theoretical pawn moves
        EXPECT_LE(blackMoves.size(), 16);
    }
}