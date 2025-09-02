#include <gtest/gtest.h>
#include "MoveGen.h"

using namespace opera;

class MoveGenTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup common test data
    }
    
    void TearDown() override {
        // Cleanup after tests
    }
};

// ===== POSITIVE TESTS =====

TEST_F(MoveGenTest, DefaultConstructor) {
    MoveGen move;
    EXPECT_EQ(move.from(), MoveGen::NULL_SQUARE_VALUE);
    EXPECT_EQ(move.to(), MoveGen::NULL_SQUARE_VALUE);
    EXPECT_EQ(move.type(), MoveGen::MoveType::NORMAL);
    EXPECT_EQ(move.promotionPiece(), NO_PIECE);
    EXPECT_EQ(move.capturedPiece(), NO_PIECE);
    EXPECT_FALSE(move.isCapture());
    EXPECT_FALSE(move.isPromotion());
    EXPECT_FALSE(move.isCastling());
    EXPECT_FALSE(move.isEnPassant());
}

TEST_F(MoveGenTest, NormalMoveConstruction) {
    MoveGen move(E2, E4);
    EXPECT_EQ(move.from(), E2);
    EXPECT_EQ(move.to(), E4);
    EXPECT_EQ(move.type(), MoveGen::MoveType::NORMAL);
    EXPECT_EQ(move.promotionPiece(), NO_PIECE);
    EXPECT_EQ(move.capturedPiece(), NO_PIECE);
    EXPECT_FALSE(move.isCapture());
    EXPECT_FALSE(move.isPromotion());
    EXPECT_FALSE(move.isCastling());
    EXPECT_FALSE(move.isEnPassant());
}

TEST_F(MoveGenTest, CaptureConstruction) {
    MoveGen move(E4, D5, MoveGen::MoveType::NORMAL, NO_PIECE, BLACK_PAWN);
    EXPECT_EQ(move.from(), E4);
    EXPECT_EQ(move.to(), D5);
    EXPECT_EQ(move.type(), MoveGen::MoveType::NORMAL);
    EXPECT_EQ(move.promotionPiece(), NO_PIECE);
    EXPECT_EQ(move.capturedPiece(), BLACK_PAWN);
    EXPECT_TRUE(move.isCapture());
    EXPECT_FALSE(move.isPromotion());
    EXPECT_FALSE(move.isCastling());
    EXPECT_FALSE(move.isEnPassant());
}

TEST_F(MoveGenTest, PromotionConstruction) {
    MoveGen move(A7, A8, MoveGen::MoveType::PROMOTION, WHITE_QUEEN);
    EXPECT_EQ(move.from(), A7);
    EXPECT_EQ(move.to(), A8);
    EXPECT_EQ(move.type(), MoveGen::MoveType::PROMOTION);
    EXPECT_EQ(move.promotionPiece(), WHITE_QUEEN);
    EXPECT_EQ(move.capturedPiece(), NO_PIECE);
    EXPECT_FALSE(move.isCapture());
    EXPECT_TRUE(move.isPromotion());
    EXPECT_FALSE(move.isCastling());
    EXPECT_FALSE(move.isEnPassant());
}

TEST_F(MoveGenTest, CapturingPromotionConstruction) {
    MoveGen move(B7, A8, MoveGen::MoveType::PROMOTION, WHITE_KNIGHT, BLACK_ROOK);
    EXPECT_EQ(move.from(), B7);
    EXPECT_EQ(move.to(), A8);
    EXPECT_EQ(move.type(), MoveGen::MoveType::PROMOTION);
    EXPECT_EQ(move.promotionPiece(), WHITE_KNIGHT);
    EXPECT_EQ(move.capturedPiece(), BLACK_ROOK);
    EXPECT_TRUE(move.isCapture());
    EXPECT_TRUE(move.isPromotion());
    EXPECT_FALSE(move.isCastling());
    EXPECT_FALSE(move.isEnPassant());
}

TEST_F(MoveGenTest, CastlingConstruction) {
    MoveGen kingside(E1, G1, MoveGen::MoveType::CASTLING);
    EXPECT_EQ(kingside.from(), E1);
    EXPECT_EQ(kingside.to(), G1);
    EXPECT_EQ(kingside.type(), MoveGen::MoveType::CASTLING);
    EXPECT_EQ(kingside.promotionPiece(), NO_PIECE);
    EXPECT_EQ(kingside.capturedPiece(), NO_PIECE);
    EXPECT_FALSE(kingside.isCapture());
    EXPECT_FALSE(kingside.isPromotion());
    EXPECT_TRUE(kingside.isCastling());
    EXPECT_FALSE(kingside.isEnPassant());
    
    MoveGen queenside(E8, C8, MoveGen::MoveType::CASTLING);
    EXPECT_EQ(queenside.from(), E8);
    EXPECT_EQ(queenside.to(), C8);
    EXPECT_EQ(queenside.type(), MoveGen::MoveType::CASTLING);
    EXPECT_TRUE(queenside.isCastling());
}

TEST_F(MoveGenTest, EnPassantConstruction) {
    MoveGen enPassant(E5, D6, MoveGen::MoveType::EN_PASSANT, NO_PIECE, BLACK_PAWN);
    EXPECT_EQ(enPassant.from(), E5);
    EXPECT_EQ(enPassant.to(), D6);
    EXPECT_EQ(enPassant.type(), MoveGen::MoveType::EN_PASSANT);
    EXPECT_EQ(enPassant.promotionPiece(), NO_PIECE);
    EXPECT_EQ(enPassant.capturedPiece(), BLACK_PAWN);
    EXPECT_TRUE(enPassant.isCapture());
    EXPECT_FALSE(enPassant.isPromotion());
    EXPECT_FALSE(enPassant.isCastling());
    EXPECT_TRUE(enPassant.isEnPassant());
}

TEST_F(MoveGenTest, DoublePawnPushConstruction) {
    MoveGen doublePush(D2, D4, MoveGen::MoveType::DOUBLE_PAWN_PUSH);
    EXPECT_EQ(doublePush.from(), D2);
    EXPECT_EQ(doublePush.to(), D4);
    EXPECT_EQ(doublePush.type(), MoveGen::MoveType::DOUBLE_PAWN_PUSH);
    EXPECT_EQ(doublePush.promotionPiece(), NO_PIECE);
    EXPECT_EQ(doublePush.capturedPiece(), NO_PIECE);
    EXPECT_FALSE(doublePush.isCapture());
    EXPECT_FALSE(doublePush.isPromotion());
    EXPECT_FALSE(doublePush.isCastling());
    EXPECT_FALSE(doublePush.isEnPassant());
}

TEST_F(MoveGenTest, AllPromotionPieces) {
    // Test all four promotion pieces
    MoveGen queenPromo(H7, H8, MoveGen::MoveType::PROMOTION, WHITE_QUEEN);
    MoveGen rookPromo(H7, H8, MoveGen::MoveType::PROMOTION, WHITE_ROOK);
    MoveGen bishopPromo(H7, H8, MoveGen::MoveType::PROMOTION, WHITE_BISHOP);
    MoveGen knightPromo(H7, H8, MoveGen::MoveType::PROMOTION, WHITE_KNIGHT);
    
    EXPECT_EQ(queenPromo.promotionPiece(), WHITE_QUEEN);
    EXPECT_EQ(rookPromo.promotionPiece(), WHITE_ROOK);
    EXPECT_EQ(bishopPromo.promotionPiece(), WHITE_BISHOP);
    EXPECT_EQ(knightPromo.promotionPiece(), WHITE_KNIGHT);
    
    EXPECT_TRUE(queenPromo.isPromotion());
    EXPECT_TRUE(rookPromo.isPromotion());
    EXPECT_TRUE(bishopPromo.isPromotion());
    EXPECT_TRUE(knightPromo.isPromotion());
}

TEST_F(MoveGenTest, MoveEquality) {
    MoveGen move1(E2, E4);
    MoveGen move2(E2, E4);
    MoveGen move3(E2, E3);
    MoveGen move4(D2, E4);
    
    EXPECT_EQ(move1, move2);
    EXPECT_NE(move1, move3);
    EXPECT_NE(move1, move4);
    
    // Test with special moves
    MoveGen castle1(E1, G1, MoveGen::MoveType::CASTLING);
    MoveGen castle2(E1, G1, MoveGen::MoveType::CASTLING);
    MoveGen normalMove(E1, G1);
    
    EXPECT_EQ(castle1, castle2);
    EXPECT_NE(castle1, normalMove);  // Same squares but different type
}

TEST_F(MoveGenTest, BitPackingConsistency) {
    // Test that bit packing and unpacking preserves all data
    std::vector<MoveGen> testMoves = {
        MoveGen(A1, H8),  // Corner to corner
        MoveGen(H8, A1),  // Reverse
        MoveGen(E4, D5, MoveGen::MoveType::NORMAL, NO_PIECE, BLACK_QUEEN),  // Capture
        MoveGen(A7, A8, MoveGen::MoveType::PROMOTION, WHITE_QUEEN),  // Promotion
        MoveGen(E1, G1, MoveGen::MoveType::CASTLING),  // Castling
        MoveGen(E5, F6, MoveGen::MoveType::EN_PASSANT, NO_PIECE, BLACK_PAWN),  // En passant
        MoveGen(C2, C4, MoveGen::MoveType::DOUBLE_PAWN_PUSH)  // Double push
    };
    
    for (const MoveGen& originalMove : testMoves) {
        // Create a new move from the raw data to test bit packing
        MoveGen reconstructedMove = MoveGen::fromRawData(originalMove.rawData());
        
        EXPECT_EQ(originalMove, reconstructedMove);
        EXPECT_EQ(originalMove.from(), reconstructedMove.from());
        EXPECT_EQ(originalMove.to(), reconstructedMove.to());
        EXPECT_EQ(originalMove.type(), reconstructedMove.type());
        EXPECT_EQ(originalMove.promotionPiece(), reconstructedMove.promotionPiece());
        EXPECT_EQ(originalMove.capturedPiece(), reconstructedMove.capturedPiece());
    }
}

// ===== BOUNDARY CONDITIONS =====

TEST_F(MoveGenTest, BoundarySquares) {
    // Test all corner squares
    MoveGen a1Move(A1, B2);
    MoveGen a8Move(A8, B7);
    MoveGen h1Move(H1, G2);
    MoveGen h8Move(H8, G7);
    
    EXPECT_EQ(a1Move.from(), A1);
    EXPECT_EQ(a8Move.from(), A8);
    EXPECT_EQ(h1Move.from(), H1);
    EXPECT_EQ(h8Move.from(), H8);
    
    // Test edge squares (files and ranks)
    MoveGen fileAMove(A4, A5);  // A file
    MoveGen fileHMove(H4, H5);  // H file
    MoveGen rank1Move(D1, E1);  // 1st rank
    MoveGen rank8Move(D8, E8);  // 8th rank
    
    EXPECT_EQ(fileAMove.from(), A4);
    EXPECT_EQ(fileHMove.from(), H4);
    EXPECT_EQ(rank1Move.from(), D1);
    EXPECT_EQ(rank8Move.from(), D8);
}

// ===== NEGATIVE TESTS =====

TEST_F(MoveGenTest, InvalidSquareHandling) {
    // Test construction with invalid square values (should handle gracefully)
    EXPECT_NO_THROW(MoveGen(MoveGen::NULL_SQUARE_VALUE, E4));
    EXPECT_NO_THROW(MoveGen(E4, MoveGen::NULL_SQUARE_VALUE));
    EXPECT_NO_THROW(MoveGen(MoveGen::NULL_SQUARE_VALUE, MoveGen::NULL_SQUARE_VALUE));
    
    MoveGen invalidMove(MoveGen::NULL_SQUARE_VALUE, E4);
    EXPECT_EQ(invalidMove.from(), MoveGen::NULL_SQUARE_VALUE);
    EXPECT_EQ(invalidMove.to(), E4);
}

TEST_F(MoveGenTest, StringRepresentationBasic) {
    // Test string representation with various move types
    MoveGen normalMove(E2, E4);
    MoveGen capture(E4, D5, MoveGen::MoveType::NORMAL, NO_PIECE, BLACK_PAWN);
    MoveGen promotion(A7, A8, MoveGen::MoveType::PROMOTION, WHITE_QUEEN);
    MoveGen castling(E1, G1, MoveGen::MoveType::CASTLING);
    MoveGen enPassant(E5, D6, MoveGen::MoveType::EN_PASSANT, NO_PIECE, BLACK_PAWN);
    
    // These should not crash and should produce reasonable output
    EXPECT_NO_THROW(normalMove.toString());
    EXPECT_NO_THROW(capture.toString());
    EXPECT_NO_THROW(promotion.toString());
    EXPECT_NO_THROW(castling.toString());
    EXPECT_NO_THROW(enPassant.toString());
    
    // Basic validation of string format (assuming UCI-like format)
    std::string normalStr = normalMove.toString();
    EXPECT_EQ(normalStr.length(), 4);  // e.g., "e2e4"
    EXPECT_EQ(normalStr, "e2e4");
    
    std::string promoStr = promotion.toString();
    EXPECT_EQ(promoStr.length(), 5);   // e.g., "a7a8q"
    EXPECT_EQ(promoStr, "a7a8q");
}

// ===== MOVELIST TESTS =====

TEST_F(MoveGenTest, MoveListBasic) {
    MoveGenList<> moveList;
    EXPECT_EQ(moveList.size(), 0);
    EXPECT_TRUE(moveList.empty());
    EXPECT_EQ(moveList.capacity(), 256);  // Default capacity
    
    MoveGen move(E2, E4);
    moveList.add(move);
    
    EXPECT_EQ(moveList.size(), 1);
    EXPECT_FALSE(moveList.empty());
    EXPECT_EQ(moveList[0], move);
}

TEST_F(MoveGenTest, MoveListMultipleMoves) {
    MoveGenList<> moveList;
    std::vector<MoveGen> testMoves = {
        MoveGen(E2, E4),
        MoveGen(D2, D4),
        MoveGen(G1, F3),
        MoveGen(B1, C3),
        MoveGen(F1, C4)
    };
    
    for (const MoveGen& move : testMoves) {
        moveList.add(move);
    }
    
    EXPECT_EQ(moveList.size(), testMoves.size());
    EXPECT_FALSE(moveList.empty());
    
    for (size_t i = 0; i < testMoves.size(); ++i) {
        EXPECT_EQ(moveList[i], testMoves[i]);
    }
}

TEST_F(MoveGenTest, MoveListIteratorSupport) {
    MoveGenList<> moveList;
    std::vector<MoveGen> testMoves = {
        MoveGen(E2, E4),
        MoveGen(D2, D4),
        MoveGen(G1, F3)
    };
    
    for (const MoveGen& move : testMoves) {
        moveList.add(move);
    }
    
    // Test iterator
    auto it = moveList.begin();
    for (size_t i = 0; i < testMoves.size(); ++i, ++it) {
        EXPECT_NE(it, moveList.end());
        EXPECT_EQ(*it, testMoves[i]);
    }
    EXPECT_EQ(it, moveList.end());
    
    // Test range-based for loop
    size_t index = 0;
    for (const MoveGen& move : moveList) {
        EXPECT_LT(index, testMoves.size());
        EXPECT_EQ(move, testMoves[index]);
        ++index;
    }
    EXPECT_EQ(index, testMoves.size());
}

TEST_F(MoveGenTest, MoveListClear) {
    MoveGenList<> moveList;
    
    moveList.add(MoveGen(E2, E4));
    moveList.add(MoveGen(D2, D4));
    
    EXPECT_EQ(moveList.size(), 2);
    EXPECT_FALSE(moveList.empty());
    
    moveList.clear();
    
    EXPECT_EQ(moveList.size(), 0);
    EXPECT_TRUE(moveList.empty());
    EXPECT_EQ(moveList.begin(), moveList.end());
}

TEST_F(MoveGenTest, MoveListCapacity) {
    MoveGenList<16> smallList;
    MoveGen testMove(E2, E4);
    
    // Fill to capacity
    for (size_t i = 0; i < 16; ++i) {
        smallList.add(testMove);
    }
    
    EXPECT_EQ(smallList.size(), 16);
    EXPECT_TRUE(smallList.full());
    EXPECT_FALSE(smallList.empty());
    
    // Attempt to add beyond capacity (should handle gracefully)
    EXPECT_NO_THROW(smallList.add(testMove));
    
    // Size should not exceed capacity
    EXPECT_LE(smallList.size(), smallList.capacity());
    EXPECT_TRUE(smallList.full());
}

TEST_F(MoveGenTest, MoveListEmplaceBack) {
    MoveGenList<> moveList;
    
    moveList.emplace_back(E2, E4);
    moveList.emplace_back(D2, D4, MoveGen::MoveType::DOUBLE_PAWN_PUSH);
    moveList.emplace_back(A7, A8, MoveGen::MoveType::PROMOTION, WHITE_QUEEN);
    
    EXPECT_EQ(moveList.size(), 3);
    
    EXPECT_EQ(moveList[0], MoveGen(E2, E4));
    EXPECT_EQ(moveList[1].type(), MoveGen::MoveType::DOUBLE_PAWN_PUSH);
    EXPECT_TRUE(moveList[2].isPromotion());
    EXPECT_EQ(moveList[2].promotionPiece(), WHITE_QUEEN);
}