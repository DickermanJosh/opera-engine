#pragma once

#include "Types.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace opera {

class Board {
private:
    // Bitboard representation - 12 bitboards for each piece type/color combination
    Bitboard pieces[12];
    
    // Occupancy bitboards for fast lookups
    Bitboard occupied[3]; // [WHITE_PIECES, BLACK_PIECES, ALL_PIECES]
    
    // Game state
    CastlingRights castling;
    Square enPassant;
    int halfmoveClock;
    int fullmoveNumber;
    Color sideToMove;
    
    // Zobrist hashing for transposition tables
    uint64_t zobristKey;
    
    // Move history for undo operations
    std::vector<BoardState> history;
    
    // Static zobrist keys (initialized once)
    static uint64_t zobristPieces[64][12];
    static uint64_t zobristSideToMove;
    static uint64_t zobristCastling[16];
    static uint64_t zobristEnPassant[64];
    static bool zobristInitialized;
    
    // Initialize zobrist keys
    static void initializeZobrist();
    
    // Helper methods for bitboard operations
    void setPiece(Square sq, Piece piece);
    void removePiece(Square sq);
    void movePiece(Square from, Square to, Piece piece);
    
    // Update occupancy bitboards
    void updateOccupancy();
    
    // Zobrist key management
    void updateZobristKey();
    uint64_t computeZobristKey() const;
    
    // FEN parsing helpers
    void parsePiecePlacement(const std::string& placement);
    void parseGameState(const std::string& sideToMove, const std::string& castling, 
                       const std::string& enPassant, const std::string& halfmove, 
                       const std::string& fullmove);
    
    // FEN generation helpers
    std::string generatePiecePlacement() const;
    std::string generateCastlingString() const;
    
public:
    // Constructors
    Board();
    Board(const std::string& fen);
    Board(const Board& other);
    Board& operator=(const Board& other);
    
    // Destructor
    ~Board() = default;
    
    // FEN operations
    void setFromFEN(const std::string& fen);
    std::string toFEN() const;
    
    // Basic board queries
    Piece getPiece(Square sq) const;
    bool isEmpty(Square sq) const;
    bool isOccupied(Square sq) const;
    
    // Bitboard queries
    Bitboard getPieceBitboard(Piece piece) const;
    Bitboard getPieceBitboard(Color color, PieceType pieceType) const;
    Bitboard getColorBitboard(Color color) const;
    Bitboard getOccupiedBitboard() const;
    Bitboard getEmptyBitboard() const;
    
    // Game state accessors
    Color getSideToMove() const { return sideToMove; }
    CastlingRights getCastlingRights() const { return castling; }
    Square getEnPassantSquare() const { return enPassant; }
    int getHalfmoveClock() const { return halfmoveClock; }
    int getFullmoveNumber() const { return fullmoveNumber; }
    uint64_t getZobristKey() const { return zobristKey; }
    
    // King position queries
    Square getKingSquare(Color color) const;
    
    // Attack/defend queries
    bool isSquareAttacked(Square sq, Color attackingColor) const;
    bool isInCheck(Color color) const;
    
    // Piece counting
    int getPieceCount(Color color, PieceType pieceType) const;
    int getTotalPieceCount(Color color) const;
    
    // Move operations
    void makeMove(const Move& move);
    void unmakeMove(const Move& move);
    
    // Move validation
    bool isLegalMove(const Move& move) const;
    bool isPseudoLegalMove(const Move& move) const;
    
    // Castling legality
    bool canCastle(Color color, bool kingside) const;
    bool canCastleKingside(Color color) const;
    bool canCastleQueenside(Color color) const;
    
    // Special move detection
    bool isEnPassantCapture(const Move& move) const;
    bool isCastlingMove(const Move& move) const;
    bool isPromotionMove(const Move& move) const;
    
    // Position evaluation helpers
    bool hasNonPawnMaterial(Color color) const;
    bool isEndgame() const;
    int getPhase() const; // 0 = endgame, 24 = opening
    
    // Position properties
    bool isDrawByMaterial() const;
    bool isDrawByFiftyMoveRule() const;
    bool hasRepeated() const;
    
    // Utility methods
    void clear();
    Board copy() const;
    bool isValid() const;
    
    // Debug and display
    std::string toString() const;
    void print() const;
    
    // Hash table size for repetition detection
    static constexpr size_t MAX_GAME_LENGTH = 1024;
    
private:
    // Attack pattern generation (will be implemented with magic bitboards later)
    Bitboard getPawnAttacks(Square sq, Color color) const;
    Bitboard getKnightAttacks(Square sq) const;
    Bitboard getKingAttacks(Square sq) const;
    Bitboard getBishopAttacks(Square sq, Bitboard occupied) const;
    Bitboard getRookAttacks(Square sq, Bitboard occupied) const;
    Bitboard getQueenAttacks(Square sq, Bitboard occupied) const;
    
    // Sliding piece attack generation (simplified for initial implementation)
    Bitboard generateSlidingAttacks(Square sq, const int* directions, int numDirs, Bitboard occupied) const;
    
    // Castling helper methods
    void updateCastlingRights(const Move& move);
    void restoreCastlingRights(const BoardState& state);
    
    // Move execution helpers
    void executeCastling(const Move& move);
    void undoCastling(const Move& move);
    void executeEnPassant(const Move& move);
    void undoEnPassant(const Move& move, const BoardState& state);
    void executePromotion(const Move& move);
    void undoPromotion(const Move& move, const BoardState& state);
    
    // Zobrist key updates
    void togglePiece(Square sq, Piece piece);
    void toggleSideToMove();
    void toggleCastlingRight(int right);
    void toggleEnPassantFile(File file);
};

// Inline implementations for performance-critical methods
inline Piece Board::getPiece(Square sq) const {
    if (!isValidSquare(sq)) return NO_PIECE;
    
    for (int piece = WHITE_PAWN; piece <= BLACK_KING; ++piece) {
        if (testBit(pieces[piece], sq)) {
            return static_cast<Piece>(piece);
        }
    }
    return NO_PIECE;
}

inline bool Board::isEmpty(Square sq) const {
    return !testBit(occupied[2], sq);
}

inline bool Board::isOccupied(Square sq) const {
    return testBit(occupied[2], sq);
}

inline Bitboard Board::getPieceBitboard(Piece piece) const {
    return (piece <= BLACK_KING) ? pieces[piece] : EMPTY_BB;
}

inline Bitboard Board::getPieceBitboard(Color color, PieceType pieceType) const {
    return getPieceBitboard(makePiece(color, pieceType));
}

inline Bitboard Board::getColorBitboard(Color color) const {
    return (color <= BLACK) ? occupied[color] : EMPTY_BB;
}

inline Bitboard Board::getOccupiedBitboard() const {
    return occupied[2];
}

inline Bitboard Board::getEmptyBitboard() const {
    return ~occupied[2];
}

inline Square Board::getKingSquare(Color color) const {
    Bitboard kingBB = getPieceBitboard(makePiece(color, KING));
    return kingBB ? static_cast<Square>(__builtin_ctzll(kingBB)) : NO_SQUARE;
}

inline bool Board::isInCheck(Color color) const {
    Square kingSquare = getKingSquare(color);
    return kingSquare != NO_SQUARE && isSquareAttacked(kingSquare, ~color);
}

inline bool Board::canCastleKingside(Color color) const {
    return castling & (color == WHITE ? WHITE_KING_SIDE : BLACK_KING_SIDE);
}

inline bool Board::canCastleQueenside(Color color) const {
    return castling & (color == WHITE ? WHITE_QUEEN_SIDE : BLACK_QUEEN_SIDE);
}

} // namespace opera