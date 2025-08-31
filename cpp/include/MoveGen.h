#pragma once

#include "Types.h"
#include <array>
#include <vector>
#include <string>
#include <cstdint>

namespace opera {

// Enhanced Move class that extends the basic Types.h Move
class MoveGen {
public:
    enum class MoveType : uint8_t {
        NORMAL = 0,
        CASTLING = 1,
        EN_PASSANT = 2,
        PROMOTION = 3,
        DOUBLE_PAWN_PUSH = 4
    };

private:
    uint32_t data;  // Packed representation
    
    // Bit layout (32 bits total):
    // Bits 0-5:   From square (6 bits, 0-63)
    // Bits 6-11:  To square (6 bits, 0-63)  
    // Bits 12-14: Move type (3 bits)
    // Bits 15-18: Promotion piece (4 bits, 0-12 for pieces)
    // Bits 19-22: Captured piece (4 bits, 0-12 for pieces)  
    // Bits 23-31: Reserved/flags (9 bits)
    
    static constexpr uint32_t FROM_MASK       = 0x0000003F;  // Bits 0-5 (6 bits)
    static constexpr uint32_t TO_MASK         = 0x00000FC0;  // Bits 6-11 (6 bits)
    static constexpr uint32_t TYPE_MASK       = 0x00007000;  // Bits 12-14 (3 bits)
    static constexpr uint32_t PROMOTION_MASK  = 0x00078000;  // Bits 15-18 (4 bits)
    static constexpr uint32_t CAPTURED_MASK   = 0x00780000;  // Bits 19-22 (4 bits)
    
    static constexpr int FROM_SHIFT       = 0;
    static constexpr int TO_SHIFT         = 6;
    static constexpr int TYPE_SHIFT       = 12;
    static constexpr int PROMOTION_SHIFT  = 15;
    static constexpr int CAPTURED_SHIFT   = 19;

public:
    // Use a special null value that fits in 6 bits for squares
    static constexpr Square NULL_SQUARE_VALUE = 63;  // Repurpose H8 as null for bit packing
    
    // Constructors
    MoveGen() : data(0) {
        setFrom(NULL_SQUARE_VALUE);
        setTo(NULL_SQUARE_VALUE);
        setType(MoveType::NORMAL);
        setPromotionPiece(NO_PIECE);
        setCapturedPiece(NO_PIECE);
    }
    
    MoveGen(Square from, Square to, MoveType type = MoveType::NORMAL, 
         Piece promotionPiece = NO_PIECE, Piece capturedPiece = NO_PIECE)
        : data(0) {
        setFrom(from);
        setTo(to);
        setType(type);
        setPromotionPiece(promotionPiece);
        setCapturedPiece(capturedPiece);
    }
    
    // Static factory method for creating from raw data
    static MoveGen fromRawData(uint32_t rawData) {
        MoveGen move;
        move.data = rawData;
        return move;
    }
    
    // Getters
    Square from() const {
        return static_cast<Square>((data & FROM_MASK) >> FROM_SHIFT);
    }
    
    Square to() const {
        return static_cast<Square>((data & TO_MASK) >> TO_SHIFT);
    }
    
    MoveType type() const {
        return static_cast<MoveType>((data & TYPE_MASK) >> TYPE_SHIFT);
    }
    
    Piece promotionPiece() const {
        return static_cast<Piece>((data & PROMOTION_MASK) >> PROMOTION_SHIFT);
    }
    
    Piece capturedPiece() const {
        return static_cast<Piece>((data & CAPTURED_MASK) >> CAPTURED_SHIFT);
    }
    
    uint32_t rawData() const {
        return data;
    }
    
    // Setters
    void setFrom(Square square) {
        data = (data & ~FROM_MASK) | (static_cast<uint32_t>(square) << FROM_SHIFT);
    }
    
    void setTo(Square square) {
        data = (data & ~TO_MASK) | (static_cast<uint32_t>(square) << TO_SHIFT);
    }
    
    void setType(MoveType moveType) {
        data = (data & ~TYPE_MASK) | (static_cast<uint32_t>(moveType) << TYPE_SHIFT);
    }
    
    void setPromotionPiece(Piece piece) {
        data = (data & ~PROMOTION_MASK) | (static_cast<uint32_t>(piece) << PROMOTION_SHIFT);
    }
    
    void setCapturedPiece(Piece piece) {
        data = (data & ~CAPTURED_MASK) | (static_cast<uint32_t>(piece) << CAPTURED_SHIFT);
    }
    
    // Convenience methods
    bool isCapture() const {
        return capturedPiece() != NO_PIECE;
    }
    
    bool isPromotion() const {
        return type() == MoveType::PROMOTION;
    }
    
    bool isCastling() const {
        return type() == MoveType::CASTLING;
    }
    
    bool isEnPassant() const {
        return type() == MoveType::EN_PASSANT;
    }
    
    bool isDoublePawnPush() const {
        return type() == MoveType::DOUBLE_PAWN_PUSH;
    }
    
    bool isQuiet() const {
        return !isCapture() && !isPromotion() && !isCastling() && !isEnPassant();
    }
    
    // Comparison operators
    bool operator==(const MoveGen& other) const {
        return data == other.data;
    }
    
    bool operator!=(const MoveGen& other) const {
        return data != other.data;
    }
    
    // Less-than operator for sorting/ordering
    bool operator<(const MoveGen& other) const {
        return data < other.data;
    }
    
    // String representation (UCI format)
    std::string toString() const {
        std::string result;
        
        // Convert from square
        Square fromSq = from();
        if (fromSq == NULL_SQUARE_VALUE) {
            result += "xx";
        } else {
            result += static_cast<char>('a' + (static_cast<int>(fromSq) % 8));
            result += static_cast<char>('1' + (static_cast<int>(fromSq) / 8));
        }
        
        // Convert to square
        Square toSq = to();
        if (toSq == NULL_SQUARE_VALUE) {
            result += "xx";
        } else {
            result += static_cast<char>('a' + (static_cast<int>(toSq) % 8));
            result += static_cast<char>('1' + (static_cast<int>(toSq) / 8));
        }
        
        // Add promotion piece if applicable
        if (isPromotion() && promotionPiece() != NO_PIECE) {
            Piece promo = promotionPiece();
            
            switch (promo) {
                case WHITE_QUEEN:
                case BLACK_QUEEN:  result += 'q'; break;
                case WHITE_ROOK:
                case BLACK_ROOK:   result += 'r'; break;
                case WHITE_BISHOP:
                case BLACK_BISHOP: result += 'b'; break;
                case WHITE_KNIGHT:
                case BLACK_KNIGHT: result += 'n'; break;
                default: result += '?'; break;
            }
        }
        
        return result;
    }
    
    // Hash function for use in hash tables
    uint32_t hash() const {
        return data;
    }
    
    // Null move check
    bool isNull() const {
        return from() == NULL_SQUARE_VALUE && to() == NULL_SQUARE_VALUE;
    }
    
    // Make this move a null move
    void setNull() {
        data = 0;
    }
};

// Null move constant
inline MoveGen NULL_MOVE_GEN() { return MoveGen(); }

// Template-based MoveList for efficient stack allocation
template<size_t MAX_MOVES = 256>
class MoveGenList {
private:
    std::array<MoveGen, MAX_MOVES> moves;
    size_t count;

public:
    // Iterator types
    using iterator = typename std::array<MoveGen, MAX_MOVES>::iterator;
    using const_iterator = typename std::array<MoveGen, MAX_MOVES>::const_iterator;
    using value_type = MoveGen;
    using size_type = size_t;

    // Constructor
    MoveGenList() : count(0) {}
    
    // Add a move to the list
    void add(const MoveGen& move) {
        if (count < MAX_MOVES) {
            moves[count] = move;
            ++count;
        }
    }
    
    // Add a move using push_back (STL compatibility)
    void push_back(const MoveGen& move) {
        add(move);
    }
    
    // Construct and add a move in place
    template<typename... Args>
    void emplace_back(Args&&... args) {
        if (count < MAX_MOVES) {
            moves[count] = MoveGen(std::forward<Args>(args)...);
            ++count;
        }
    }
    
    // Clear all moves
    void clear() {
        count = 0;
    }
    
    // Get the number of moves
    size_t size() const {
        return count;
    }
    
    // Check if the list is empty
    bool empty() const {
        return count == 0;
    }
    
    // Check if the list is full
    bool full() const {
        return count >= MAX_MOVES;
    }
    
    // Get the maximum capacity
    constexpr size_t capacity() const {
        return MAX_MOVES;
    }
    
    // Array access operators
    MoveGen& operator[](size_t index) {
        return moves[index];
    }
    
    const MoveGen& operator[](size_t index) const {
        return moves[index];
    }
    
    // Iterator support
    iterator begin() {
        return moves.begin();
    }
    
    const_iterator begin() const {
        return moves.begin();
    }
    
    iterator end() {
        return moves.begin() + count;
    }
    
    const_iterator end() const {
        return moves.begin() + count;
    }
    
    // Comparison operators
    bool operator==(const MoveGenList& other) const {
        if (count != other.count) return false;
        return std::equal(begin(), end(), other.begin());
    }
};

// Convenience type aliases
using MoveGenList64 = MoveGenList<64>;
using MoveGenList128 = MoveGenList<128>;
using MoveGenList256 = MoveGenList<256>;

// Forward declarations for move generation functions
class Board;

// Pawn move generation functions
void generatePawnMoves(const Board& board, MoveGenList<>& moves, Color color);

// Knight move generation functions
void generateKnightMoves(const Board& board, MoveGenList<>& moves, Color color);

} // namespace opera