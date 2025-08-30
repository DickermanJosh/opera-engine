#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace opera {

// Basic chess types
using Bitboard = uint64_t;
using Square = int;
using File = int;
using Rank = int;

// Color enumeration
enum Color : int {
    WHITE = 0,
    BLACK = 1,
    NO_COLOR = 2
};

// Piece type enumeration
enum PieceType : int {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    NO_PIECE_TYPE = 6
};

// Piece enumeration (color + piece type combined)
enum Piece : int {
    WHITE_PAWN = 0,
    WHITE_KNIGHT = 1,
    WHITE_BISHOP = 2,
    WHITE_ROOK = 3,
    WHITE_QUEEN = 4,
    WHITE_KING = 5,
    BLACK_PAWN = 6,
    BLACK_KNIGHT = 7,
    BLACK_BISHOP = 8,
    BLACK_ROOK = 9,
    BLACK_QUEEN = 10,
    BLACK_KING = 11,
    NO_PIECE = 12
};

// Square enumeration (A1 = 0, H8 = 63)
enum SquareEnum : Square {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NO_SQUARE = 64
};

// Castling rights
enum CastlingRight : int {
    WHITE_KING_SIDE = 1,
    WHITE_QUEEN_SIDE = 2,
    BLACK_KING_SIDE = 4,
    BLACK_QUEEN_SIDE = 8,
    ALL_CASTLING = 15,
    NO_CASTLING = 0
};

using CastlingRights = int;

// Move type enumeration
enum MoveType : int {
    NORMAL = 0,
    PROMOTION = 1,
    EN_PASSANT = 2,
    CASTLING = 3
};

// Move structure (packed into 32 bits for efficiency)
struct Move {
    // Packed move representation
    uint32_t data;
    
    // Constructors
    Move() : data(0) {}
    Move(Square from, Square to, MoveType type = NORMAL, PieceType promotion = NO_PIECE_TYPE)
        : data(0) {
        setFrom(from);
        setTo(to);
        setMoveType(type);
        setPromotionType(promotion);
    }
    
    // Accessors
    Square from() const { return static_cast<Square>(data & 0x3F); }
    Square to() const { return static_cast<Square>((data >> 6) & 0x3F); }
    MoveType moveType() const { return static_cast<MoveType>((data >> 12) & 0x3); }
    PieceType promotionType() const { return static_cast<PieceType>((data >> 14) & 0x7); }
    
    // Mutators
    void setFrom(Square sq) { data = (data & ~0x3F) | (sq & 0x3F); }
    void setTo(Square sq) { data = (data & ~0xFC0) | ((sq & 0x3F) << 6); }
    void setMoveType(MoveType type) { data = (data & ~0x3000) | ((type & 0x3) << 12); }
    void setPromotionType(PieceType type) { data = (data & ~0x1C000) | ((type & 0x7) << 14); }
    
    // Utility methods
    bool isPromotion() const { return moveType() == PROMOTION; }
    bool isEnPassant() const { return moveType() == EN_PASSANT; }
    bool isCastling() const { return moveType() == CASTLING; }
    bool isNormal() const { return moveType() == NORMAL; }
    bool isValid() const { return from() != to() && from() < 64 && to() < 64; }
    
    // Comparison operators
    bool operator==(const Move& other) const { return data == other.data; }
    bool operator!=(const Move& other) const { return data != other.data; }
    
    // String representation
    std::string toString() const;
    static Move fromString(const std::string& moveStr);
};

// Move list for efficient move storage
using MoveList = std::vector<Move>;

// Board state for undo operations
struct BoardState {
    CastlingRights castling;
    Square enPassant;
    int halfmoveClock;
    int fullmoveNumber;
    Color sideToMove;
    uint64_t zobristKey;
    Piece capturedPiece;
    
    BoardState() : castling(NO_CASTLING), enPassant(NO_SQUARE), 
                   halfmoveClock(0), fullmoveNumber(1), 
                   sideToMove(WHITE), zobristKey(0), capturedPiece(NO_PIECE) {}
};

// Utility functions
inline Color operator~(Color c) { return Color(c ^ BLACK); }
inline Piece makePiece(Color c, PieceType pt) { return Piece(c * 6 + pt); }
inline Color colorOf(Piece p) { return Color(p / 6); }
inline PieceType typeOf(Piece p) { return PieceType(p % 6); }

// Square utility functions
inline File fileOf(Square s) { return s & 7; }
inline Rank rankOf(Square s) { return s >> 3; }
inline Square makeSquare(File f, Rank r) { return Square((r << 3) + f); }
inline bool isValidSquare(Square s) { return s >= A1 && s <= H8; }

// Bitboard utility functions
inline Bitboard squareToBitboard(Square s) { return 1ULL << s; }
inline bool testBit(Bitboard bb, Square s) { return bb & squareToBitboard(s); }
inline void setBit(Bitboard& bb, Square s) { bb |= squareToBitboard(s); }
inline void clearBit(Bitboard& bb, Square s) { bb &= ~squareToBitboard(s); }
inline int popcount(Bitboard bb) { return __builtin_popcountll(bb); }

// Direction vectors for piece movement
enum Direction : int {
    NORTH = 8,
    SOUTH = -8,
    EAST = 1,
    WEST = -1,
    NORTH_EAST = 9,
    NORTH_WEST = 7,
    SOUTH_EAST = -7,
    SOUTH_WEST = -9
};

// Constants
constexpr Bitboard EMPTY_BB = 0ULL;
constexpr Bitboard FULL_BB = ~0ULL;
constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_B = FILE_A << 1;
constexpr Bitboard FILE_C = FILE_A << 2;
constexpr Bitboard FILE_D = FILE_A << 3;
constexpr Bitboard FILE_E = FILE_A << 4;
constexpr Bitboard FILE_F = FILE_A << 5;
constexpr Bitboard FILE_G = FILE_A << 6;
constexpr Bitboard FILE_H = FILE_A << 7;

constexpr Bitboard RANK_1 = 0xFFULL;
constexpr Bitboard RANK_2 = RANK_1 << 8;
constexpr Bitboard RANK_3 = RANK_1 << 16;
constexpr Bitboard RANK_4 = RANK_1 << 24;
constexpr Bitboard RANK_5 = RANK_1 << 32;
constexpr Bitboard RANK_6 = RANK_1 << 40;
constexpr Bitboard RANK_7 = RANK_1 << 48;
constexpr Bitboard RANK_8 = RANK_1 << 56;

// Starting position FEN
constexpr char STARTING_FEN[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

} // namespace opera