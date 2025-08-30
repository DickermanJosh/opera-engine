#include <gtest/gtest.h>
#include "Board.h"
#include "Types.h"

using namespace opera;

class BoardTest : public ::testing::Test {
protected:
    void SetUp() override {
        board = std::make_unique<Board>();
    }
    
    std::unique_ptr<Board> board;
};

// Test basic board initialization
TEST_F(BoardTest, DefaultConstructor) {
    EXPECT_EQ(board->getSideToMove(), WHITE);
    EXPECT_EQ(board->getCastlingRights(), ALL_CASTLING);
    EXPECT_EQ(board->getEnPassantSquare(), NO_SQUARE);
    EXPECT_EQ(board->getHalfmoveClock(), 0);
    EXPECT_EQ(board->getFullmoveNumber(), 1);
    EXPECT_NE(board->getZobristKey(), 0ULL);
}

// Test FEN parsing and generation
TEST_F(BoardTest, StartingPositionFEN) {
    Board startBoard(STARTING_FEN);
    
    // Verify starting position setup
    EXPECT_EQ(startBoard.getSideToMove(), WHITE);
    EXPECT_EQ(startBoard.getCastlingRights(), ALL_CASTLING);
    EXPECT_EQ(startBoard.getEnPassantSquare(), NO_SQUARE);
    EXPECT_EQ(startBoard.getHalfmoveClock(), 0);
    EXPECT_EQ(startBoard.getFullmoveNumber(), 1);
    
    // Verify piece placement
    EXPECT_EQ(startBoard.getPiece(A1), WHITE_ROOK);
    EXPECT_EQ(startBoard.getPiece(B1), WHITE_KNIGHT);
    EXPECT_EQ(startBoard.getPiece(C1), WHITE_BISHOP);
    EXPECT_EQ(startBoard.getPiece(D1), WHITE_QUEEN);
    EXPECT_EQ(startBoard.getPiece(E1), WHITE_KING);
    EXPECT_EQ(startBoard.getPiece(F1), WHITE_BISHOP);
    EXPECT_EQ(startBoard.getPiece(G1), WHITE_KNIGHT);
    EXPECT_EQ(startBoard.getPiece(H1), WHITE_ROOK);
    
    for (File f = 0; f < 8; f++) {
        EXPECT_EQ(startBoard.getPiece(makeSquare(f, 1)), WHITE_PAWN);
        EXPECT_EQ(startBoard.getPiece(makeSquare(f, 6)), BLACK_PAWN);
    }
    
    EXPECT_EQ(startBoard.getPiece(A8), BLACK_ROOK);
    EXPECT_EQ(startBoard.getPiece(B8), BLACK_KNIGHT);
    EXPECT_EQ(startBoard.getPiece(C8), BLACK_BISHOP);
    EXPECT_EQ(startBoard.getPiece(D8), BLACK_QUEEN);
    EXPECT_EQ(startBoard.getPiece(E8), BLACK_KING);
    EXPECT_EQ(startBoard.getPiece(F8), BLACK_BISHOP);
    EXPECT_EQ(startBoard.getPiece(G8), BLACK_KNIGHT);
    EXPECT_EQ(startBoard.getPiece(H8), BLACK_ROOK);
}

TEST_F(BoardTest, FENRoundTrip) {
    std::string originalFEN = STARTING_FEN;
    Board testBoard(originalFEN);
    std::string generatedFEN = testBoard.toFEN();
    
    EXPECT_EQ(originalFEN, generatedFEN);
}

TEST_F(BoardTest, CustomFENPosition) {
    std::string customFEN = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";
    Board testBoard(customFEN);
    
    EXPECT_EQ(testBoard.getSideToMove(), WHITE);
    EXPECT_EQ(testBoard.getCastlingRights(), ALL_CASTLING);
    EXPECT_EQ(testBoard.getEnPassantSquare(), NO_SQUARE);
    EXPECT_EQ(testBoard.getHalfmoveClock(), 4);
    EXPECT_EQ(testBoard.getFullmoveNumber(), 4);
    
    // Verify specific pieces
    EXPECT_EQ(testBoard.getPiece(C6), BLACK_KNIGHT);
    EXPECT_EQ(testBoard.getPiece(C4), WHITE_BISHOP);
    EXPECT_EQ(testBoard.getPiece(F3), WHITE_KNIGHT);
    EXPECT_EQ(testBoard.getPiece(E5), BLACK_PAWN);
    EXPECT_EQ(testBoard.getPiece(E4), WHITE_PAWN);
}

// Test bitboard functionality
TEST_F(BoardTest, BitboardQueries) {
    Board testBoard(STARTING_FEN);
    
    // Test piece type bitboards
    Bitboard whitePawns = testBoard.getPieceBitboard(WHITE_PAWN);
    EXPECT_EQ(popcount(whitePawns), 8);
    EXPECT_EQ(whitePawns, RANK_2);
    
    Bitboard blackPawns = testBoard.getPieceBitboard(BLACK_PAWN);
    EXPECT_EQ(popcount(blackPawns), 8);
    EXPECT_EQ(blackPawns, RANK_7);
    
    // Test color bitboards
    Bitboard whitePieces = testBoard.getColorBitboard(WHITE);
    EXPECT_EQ(popcount(whitePieces), 16);
    EXPECT_EQ(whitePieces, RANK_1 | RANK_2);
    
    Bitboard blackPieces = testBoard.getColorBitboard(BLACK);
    EXPECT_EQ(popcount(blackPieces), 16);
    EXPECT_EQ(blackPieces, RANK_7 | RANK_8);
    
    // Test occupancy
    Bitboard occupied = testBoard.getOccupiedBitboard();
    EXPECT_EQ(popcount(occupied), 32);
    EXPECT_EQ(occupied, RANK_1 | RANK_2 | RANK_7 | RANK_8);
}

// Test move making and unmaking
TEST_F(BoardTest, MakeUnmakeMove) {
    Board testBoard(STARTING_FEN);
    uint64_t originalKey = testBoard.getZobristKey();
    
    // Make a move: e2-e4
    Move move(E2, E4, NORMAL);
    testBoard.makeMove(move);
    
    // Verify move was made
    EXPECT_EQ(testBoard.getPiece(E2), NO_PIECE);
    EXPECT_EQ(testBoard.getPiece(E4), WHITE_PAWN);
    EXPECT_EQ(testBoard.getSideToMove(), BLACK);
    EXPECT_EQ(testBoard.getHalfmoveClock(), 0);
    EXPECT_EQ(testBoard.getFullmoveNumber(), 1);
    EXPECT_NE(testBoard.getZobristKey(), originalKey);
    
    // Unmake the move
    testBoard.unmakeMove(move);
    
    // Verify position is restored
    EXPECT_EQ(testBoard.getPiece(E2), WHITE_PAWN);
    EXPECT_EQ(testBoard.getPiece(E4), NO_PIECE);
    EXPECT_EQ(testBoard.getSideToMove(), WHITE);
    EXPECT_EQ(testBoard.getHalfmoveClock(), 0);
    EXPECT_EQ(testBoard.getFullmoveNumber(), 1);
    EXPECT_EQ(testBoard.getZobristKey(), originalKey);
}

TEST_F(BoardTest, CaptureMove) {
    std::string fen = "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2";
    Board testBoard(fen);
    uint64_t originalKey = testBoard.getZobristKey();
    
    // Capture move: exd5
    Move captureMove(E4, E5, NORMAL);
    testBoard.makeMove(captureMove);
    
    EXPECT_EQ(testBoard.getPiece(E4), NO_PIECE);
    EXPECT_EQ(testBoard.getPiece(E5), WHITE_PAWN);
    EXPECT_EQ(testBoard.getSideToMove(), BLACK);
    EXPECT_EQ(testBoard.getHalfmoveClock(), 0); // Reset on capture
    
    // Unmake capture
    testBoard.unmakeMove(captureMove);
    
    EXPECT_EQ(testBoard.getPiece(E4), WHITE_PAWN);
    EXPECT_EQ(testBoard.getPiece(E5), BLACK_PAWN);
    EXPECT_EQ(testBoard.getSideToMove(), WHITE);
    EXPECT_EQ(testBoard.getZobristKey(), originalKey);
}

TEST_F(BoardTest, EnPassantCapture) {
    // Position where en passant is possible
    std::string fen = "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3";
    Board testBoard(fen);
    
    // En passant capture: exf6
    Move epMove(E5, F6, EN_PASSANT);
    testBoard.makeMove(epMove);
    
    EXPECT_EQ(testBoard.getPiece(E5), NO_PIECE);
    EXPECT_EQ(testBoard.getPiece(F6), WHITE_PAWN);
    EXPECT_EQ(testBoard.getPiece(F5), NO_PIECE); // Captured pawn removed
    
    // Unmake en passant
    testBoard.unmakeMove(epMove);
    
    EXPECT_EQ(testBoard.getPiece(E5), WHITE_PAWN);
    EXPECT_EQ(testBoard.getPiece(F6), NO_PIECE);
    EXPECT_EQ(testBoard.getPiece(F5), BLACK_PAWN); // Captured pawn restored
}

TEST_F(BoardTest, Castling) {
    // Position where white can castle kingside
    std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 1";
    Board testBoard(fen);
    
    // Kingside castling
    Move castleMove(E1, G1, CASTLING);
    testBoard.makeMove(castleMove);
    
    EXPECT_EQ(testBoard.getPiece(E1), NO_PIECE);
    EXPECT_EQ(testBoard.getPiece(F1), WHITE_ROOK);
    EXPECT_EQ(testBoard.getPiece(G1), WHITE_KING);
    EXPECT_EQ(testBoard.getPiece(H1), NO_PIECE);
    EXPECT_EQ(testBoard.getCastlingRights() & WHITE_KING_SIDE, 0);
    EXPECT_EQ(testBoard.getCastlingRights() & WHITE_QUEEN_SIDE, 0);
    
    // Unmake castling
    testBoard.unmakeMove(castleMove);
    
    EXPECT_EQ(testBoard.getPiece(E1), WHITE_KING);
    EXPECT_EQ(testBoard.getPiece(F1), NO_PIECE);
    EXPECT_EQ(testBoard.getPiece(G1), NO_PIECE);
    EXPECT_EQ(testBoard.getPiece(H1), WHITE_ROOK);
}

TEST_F(BoardTest, Promotion) {
    // Position where white pawn can promote (non-capturing)
    std::string fen = "rnbqkb1r/ppppppP1/8/8/8/8/PPPPPPP1/RNBQKBNR w KQq - 0 1";
    Board testBoard(fen);
    
    // Promote to queen
    Move promoteMove(G7, G8, PROMOTION, QUEEN);
    testBoard.makeMove(promoteMove);
    
    EXPECT_EQ(testBoard.getPiece(G7), NO_PIECE);
    EXPECT_EQ(testBoard.getPiece(G8), WHITE_QUEEN);
    
    // Unmake promotion
    testBoard.unmakeMove(promoteMove);
    
    EXPECT_EQ(testBoard.getPiece(G7), WHITE_PAWN);
    EXPECT_EQ(testBoard.getPiece(G8), NO_PIECE);
}

TEST_F(BoardTest, CapturingPromotion) {
    // Position where white pawn can capture and promote
    std::string fen = "rnbqkbnr/ppppppP1/8/8/8/8/PPPPPPP1/RNBQKBNR w KQkq - 0 1";
    Board testBoard(fen);
    
    // Promote to queen by capturing knight
    Move promoteMove(G7, G8, PROMOTION, QUEEN);
    testBoard.makeMove(promoteMove);
    
    EXPECT_EQ(testBoard.getPiece(G7), NO_PIECE);
    EXPECT_EQ(testBoard.getPiece(G8), WHITE_QUEEN);
    
    // Unmake promotion - should restore both pawn and captured knight
    testBoard.unmakeMove(promoteMove);
    
    EXPECT_EQ(testBoard.getPiece(G7), WHITE_PAWN);
    EXPECT_EQ(testBoard.getPiece(G8), BLACK_KNIGHT); // Captured piece restored
}

// Test attack/defend queries
TEST_F(BoardTest, SquareAttacked) {
    Board testBoard(STARTING_FEN);
    
    // In starting position, no squares are attacked
    EXPECT_FALSE(testBoard.isSquareAttacked(E4, WHITE));
    EXPECT_FALSE(testBoard.isSquareAttacked(E4, BLACK));
    
    // After e2-e4, e4 is defended by white pawn
    testBoard.makeMove(Move(E2, E4));
    EXPECT_TRUE(testBoard.isSquareAttacked(D5, WHITE));
    EXPECT_TRUE(testBoard.isSquareAttacked(F5, WHITE));
}

TEST_F(BoardTest, InCheck) {
    // Position where white king is in check
    std::string fen = "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3";
    Board testBoard(fen);
    
    EXPECT_TRUE(testBoard.isInCheck(WHITE));
    EXPECT_FALSE(testBoard.isInCheck(BLACK));
}

// Test king safety
TEST_F(BoardTest, FindKing) {
    Board testBoard(STARTING_FEN);
    
    EXPECT_EQ(testBoard.getKingSquare(WHITE), E1);
    EXPECT_EQ(testBoard.getKingSquare(BLACK), E8);
}

// Test position evaluation helpers
TEST_F(BoardTest, MaterialCount) {
    Board testBoard(STARTING_FEN);
    
    EXPECT_EQ(testBoard.getPieceCount(WHITE, PAWN), 8);
    EXPECT_EQ(testBoard.getPieceCount(WHITE, ROOK), 2);
    EXPECT_EQ(testBoard.getPieceCount(WHITE, KNIGHT), 2);
    EXPECT_EQ(testBoard.getPieceCount(WHITE, BISHOP), 2);
    EXPECT_EQ(testBoard.getPieceCount(WHITE, QUEEN), 1);
    EXPECT_EQ(testBoard.getPieceCount(WHITE, KING), 1);
    
    EXPECT_EQ(testBoard.getPieceCount(BLACK, PAWN), 8);
    EXPECT_EQ(testBoard.getPieceCount(BLACK, ROOK), 2);
    EXPECT_EQ(testBoard.getPieceCount(BLACK, KNIGHT), 2);
    EXPECT_EQ(testBoard.getPieceCount(BLACK, BISHOP), 2);
    EXPECT_EQ(testBoard.getPieceCount(BLACK, QUEEN), 1);
    EXPECT_EQ(testBoard.getPieceCount(BLACK, KING), 1);
}

// Test copy constructor and assignment
TEST_F(BoardTest, CopyConstructor) {
    Board original(STARTING_FEN);
    Board copy(original);
    
    EXPECT_EQ(original.getSideToMove(), copy.getSideToMove());
    EXPECT_EQ(original.getCastlingRights(), copy.getCastlingRights());
    EXPECT_EQ(original.getZobristKey(), copy.getZobristKey());
    
    // Verify all pieces are copied correctly
    for (Square sq = A1; sq <= H8; ++sq) {
        EXPECT_EQ(original.getPiece(sq), copy.getPiece(sq));
    }
}

TEST_F(BoardTest, AssignmentOperator) {
    Board original(STARTING_FEN);
    Board assigned;
    
    assigned = original;
    
    EXPECT_EQ(original.getSideToMove(), assigned.getSideToMove());
    EXPECT_EQ(original.getCastlingRights(), assigned.getCastlingRights());
    EXPECT_EQ(original.getZobristKey(), assigned.getZobristKey());
    
    // Verify all pieces are assigned correctly
    for (Square sq = A1; sq <= H8; ++sq) {
        EXPECT_EQ(original.getPiece(sq), assigned.getPiece(sq));
    }
}

// Test error handling
TEST_F(BoardTest, InvalidFEN) {
    EXPECT_THROW(Board("invalid fen string"), std::invalid_argument);
    EXPECT_THROW(Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq"), std::invalid_argument);
}

TEST_F(BoardTest, InvalidMove) {
    Board testBoard(STARTING_FEN);
    
    // Invalid moves should not crash but may not be properly handled
    // This depends on the move validation strategy
    Move invalidMove(A1, A1); // Same square
    EXPECT_FALSE(invalidMove.isValid());
}

// Test zobrist key consistency
TEST_F(BoardTest, ZobristKeyConsistency) {
    Board board1(STARTING_FEN);
    Board board2(STARTING_FEN);
    
    EXPECT_EQ(board1.getZobristKey(), board2.getZobristKey());
    
    // After same sequence of moves, keys should match
    Move move1(E2, E4);
    Move move2(E7, E5);
    
    board1.makeMove(move1);
    board1.makeMove(move2);
    
    board2.makeMove(move1);
    board2.makeMove(move2);
    
    EXPECT_EQ(board1.getZobristKey(), board2.getZobristKey());
}