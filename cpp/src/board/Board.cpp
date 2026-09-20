#include "Board.h"
#include "MoveGen.h"
#include <sstream>
#include <iostream>
#include <random>
#include <algorithm>
#include <cassert>
#include <mutex>

namespace opera {

// Static zobrist initialization
uint64_t Board::zobristPieces[64][12];
uint64_t Board::zobristSideToMove;
uint64_t Board::zobristCastling[16];
uint64_t Board::zobristEnPassant[64];
bool Board::zobristInitialized = false;

void Board::initializeZobrist() {
    static std::once_flag initialized;
    std::call_once(initialized, [] {
    
    std::mt19937_64 rng(0x1234567890ABCDEFULL); // Fixed seed for reproducibility
    
    // Initialize piece zobrist keys
    for (int sq = 0; sq < 64; ++sq) {
        for (int piece = 0; piece < 12; ++piece) {
            zobristPieces[sq][piece] = rng();
        }
    }
    
    // Initialize other zobrist keys
    zobristSideToMove = rng();
    
    for (int i = 0; i < 16; ++i) {
        zobristCastling[i] = rng();
    }
    
    for (int i = 0; i < 64; ++i) {
        zobristEnPassant[i] = rng();
    }
    
    zobristInitialized = true;
    });
}

// Constructors
Board::Board() {
    initializeZobrist();
    setFromFEN(STARTING_FEN);
}

Board::Board(const std::string& fen) {
    initializeZobrist();
    setFromFEN(fen);
}

Board::Board(const Board& other) {
    initializeZobrist();
    *this = other;
}

Board& Board::operator=(const Board& other) {
    if (this != &other) {
        std::copy(other.pieces, other.pieces + 12, pieces);
        std::copy(other.occupied, other.occupied + 3, occupied);
        castling = other.castling;
        enPassant = other.enPassant;
        halfmoveClock = other.halfmoveClock;
        fullmoveNumber = other.fullmoveNumber;
        sideToMove = other.sideToMove;
        zobristKey = other.zobristKey;
        history = other.history;
    }
    return *this;
}

// FEN operations
void Board::setFromFEN(const std::string& fen) {
    clear();
    
    // Fast string parsing without streams - find space positions
    const char* str = fen.c_str();
    const char* end = str + fen.length();
    
    // Find the 6 FEN components by scanning for spaces
    const char* parts[6];
    int partLengths[6];
    int partCount = 0;
    
    const char* start = str;
    for (const char* p = str; p <= end && partCount < 6; ++p) {
        if (*p == ' ' || p == end) {
            if (p > start) {
                parts[partCount] = start;
                partLengths[partCount] = static_cast<int>(p - start);
                partCount++;
            }
            start = p + 1;
        }
    }
    
    if (partCount != 6) {
        throw std::invalid_argument("Invalid FEN string");
    }
    
    // Parse components directly from char arrays (much faster)
    parsePiecePlacementOptimized(parts[0], partLengths[0]);
    parseGameStateOptimized(parts[1], partLengths[1], parts[2], partLengths[2], 
                           parts[3], partLengths[3], parts[4], partLengths[4], 
                           parts[5], partLengths[5]);
    
    updateOccupancyAndZobrist();  // Combined operation
}

std::string Board::toFEN() const {
    std::ostringstream oss;
    
    // Piece placement
    oss << generatePiecePlacement() << " ";
    
    // Side to move
    oss << (sideToMove == WHITE ? 'w' : 'b') << " ";
    
    // Castling rights
    oss << generateCastlingString() << " ";
    
    // En passant square
    if (enPassant == NO_SQUARE) {
        oss << "- ";
    } else {
        oss << static_cast<char>('a' + fileOf(enPassant));
        oss << static_cast<char>('1' + rankOf(enPassant)) << " ";
    }
    
    // Halfmove and fullmove counters
    oss << halfmoveClock << " " << fullmoveNumber;
    
    return oss.str();
}

// Piece management
void Board::setPiece(Square sq, Piece piece) {
    if (piece != NO_PIECE) {
        setBit(pieces[piece], sq);
        zobristKey ^= zobristPieces[sq][piece];
    }
}

void Board::removePiece(Square sq) {
    const Piece piece = getPiece(sq);
    if (piece != NO_PIECE) {
        clearBit(pieces[piece], sq);
        zobristKey ^= zobristPieces[sq][piece];
    }
}

void Board::movePiece(Square from, Square to, Piece piece) {
    clearBit(pieces[piece], from);
    setBit(pieces[piece], to);
}

void Board::updateOccupancy() {
    occupied[WHITE] = EMPTY_BB;
    occupied[BLACK] = EMPTY_BB;
    
    for (int piece = WHITE_PAWN; piece <= WHITE_KING; ++piece) {
        occupied[WHITE] |= pieces[piece];
    }
    
    for (int piece = BLACK_PAWN; piece <= BLACK_KING; ++piece) {
        occupied[BLACK] |= pieces[piece];
    }
    
    occupied[2] = occupied[WHITE] | occupied[BLACK];
}

// Combined occupancy and zobrist update for FEN parsing optimization
void Board::updateOccupancyAndZobrist() {
    occupied[WHITE] = EMPTY_BB;
    occupied[BLACK] = EMPTY_BB;
    zobristKey = 0;
    
    // Single loop to update both occupancy and zobrist
    for (int piece = WHITE_PAWN; piece <= WHITE_KING; ++piece) {
        Bitboard pieceBB = pieces[piece];
        occupied[WHITE] |= pieceBB;
        
        // Update zobrist for each piece of this type
        while (pieceBB) {
            Square sq = static_cast<Square>(__builtin_ctzll(pieceBB));
            pieceBB &= pieceBB - 1; // Clear the least significant bit
            zobristKey ^= zobristPieces[sq][piece];
        }
    }
    
    for (int piece = BLACK_PAWN; piece <= BLACK_KING; ++piece) {
        Bitboard pieceBB = pieces[piece];
        occupied[BLACK] |= pieceBB;
        
        // Update zobrist for each piece of this type
        while (pieceBB) {
            Square sq = static_cast<Square>(__builtin_ctzll(pieceBB));
            pieceBB &= pieceBB - 1; // Clear the least significant bit
            zobristKey ^= zobristPieces[sq][piece];
        }
    }
    
    occupied[2] = occupied[WHITE] | occupied[BLACK];
    
    // Add remaining zobrist components
    if (sideToMove == BLACK) {
        zobristKey ^= zobristSideToMove;
    }
    zobristKey ^= zobristCastling[castling];
    if (hasLegalEnPassant()) {
        zobristKey ^= zobristEnPassant[fileOf(enPassant)];
    }
}

// Zobrist key management
uint64_t Board::computeZobristKey() const {
    uint64_t key = 0;
    
    // Add piece contributions
    for (Square sq = A1; sq <= H8; ++sq) {
        Piece piece = getPiece(sq);
        if (piece != NO_PIECE) {
            key ^= zobristPieces[sq][piece];
        }
    }
    
    // Add side to move
    if (sideToMove == BLACK) {
        key ^= zobristSideToMove;
    }
    
    // Add castling rights
    key ^= zobristCastling[castling];
    
    // Add en passant file
    if (hasLegalEnPassant()) {
        key ^= zobristEnPassant[fileOf(enPassant)];
    }
    
    return key;
}

void Board::togglePiece(Square sq, Piece piece) {
    zobristKey ^= zobristPieces[sq][piece];
}

void Board::toggleSideToMove() {
    zobristKey ^= zobristSideToMove;
}

void Board::toggleCastlingRight(int right) {
    zobristKey ^= zobristCastling[castling];
    castling ^= right;
    zobristKey ^= zobristCastling[castling];
}

void Board::toggleEnPassantFile(File file) {
    if (enPassant != NO_SQUARE) {
        zobristKey ^= zobristEnPassant[fileOf(enPassant)];
    }
    if (file != -1) {
        zobristKey ^= zobristEnPassant[file];
    }
}

// Optimized FEN parsing helpers
void Board::parsePiecePlacementOptimized(const char* placement, int length) {
    int rank = 7; // Start from rank 8 (index 7)
    int file = 0;
    
    // Fast piece lookup with explicit initialization to avoid warnings
    static Piece PIECE_LOOKUP[128];
    static bool initialized = false;
    if (!initialized) {
        // Initialize all to NO_PIECE
        std::fill_n(PIECE_LOOKUP, 128, NO_PIECE);
        // Set valid pieces
        PIECE_LOOKUP['P'] = WHITE_PAWN; PIECE_LOOKUP['N'] = WHITE_KNIGHT; PIECE_LOOKUP['B'] = WHITE_BISHOP;
        PIECE_LOOKUP['R'] = WHITE_ROOK; PIECE_LOOKUP['Q'] = WHITE_QUEEN; PIECE_LOOKUP['K'] = WHITE_KING;
        PIECE_LOOKUP['p'] = BLACK_PAWN; PIECE_LOOKUP['n'] = BLACK_KNIGHT; PIECE_LOOKUP['b'] = BLACK_BISHOP;
        PIECE_LOOKUP['r'] = BLACK_ROOK; PIECE_LOOKUP['q'] = BLACK_QUEEN; PIECE_LOOKUP['k'] = BLACK_KING;
        initialized = true;
    }
    
    for (int i = 0; i < length; ++i) {
        char c = placement[i];
        if (c == '/') {
            rank--;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0'; // Skip empty squares
        } else {
            Piece piece = PIECE_LOOKUP[static_cast<unsigned char>(c)];
            if (piece == NO_PIECE) {
                throw std::invalid_argument("Invalid piece character in FEN");
            }
            
            // Bounds check only once
            if (rank < 0 || rank > 7 || file >= 8) {
                throw std::invalid_argument("Invalid square in FEN");
            }
            
            setPiece(makeSquare(file, rank), piece);
            file++;
        }
    }
}

// Keep original for compatibility
void Board::parsePiecePlacement(const std::string& placement) {
    parsePiecePlacementOptimized(placement.c_str(), static_cast<int>(placement.length()));
}

// Fast integer parsing without exceptions
inline int fastParseInt(const char* str, int length) {
    int result = 0;
    for (int i = 0; i < length; ++i) {
        if (str[i] < '0' || str[i] > '9') return -1; // Invalid
        result = result * 10 + (str[i] - '0');
    }
    return result;
}

void Board::parseGameStateOptimized(const char* side, int sideLen,
                                   const char* castlingStr, int castlingLen,
                                   const char* enPassantStr, int enPassantLen,
                                   const char* halfmoveStr, int halfmoveLen,
                                   const char* fullmoveStr, int fullmoveLen) {
    // Parse side to move - single character comparison
    if (sideLen == 1) {
        if (side[0] == 'w') {
            sideToMove = WHITE;
        } else if (side[0] == 'b') {
            sideToMove = BLACK;
        } else {
            throw std::invalid_argument("Invalid side to move in FEN");
        }
    } else {
        throw std::invalid_argument("Invalid side to move in FEN");
    }
    
    // Parse castling rights - optimized switch
    castling = NO_CASTLING;
    if (castlingLen != 1 || castlingStr[0] != '-') {
        for (int i = 0; i < castlingLen; ++i) {
            switch (castlingStr[i]) {
                case 'K': castling |= WHITE_KING_SIDE; break;
                case 'Q': castling |= WHITE_QUEEN_SIDE; break;
                case 'k': castling |= BLACK_KING_SIDE; break;
                case 'q': castling |= BLACK_QUEEN_SIDE; break;
                default: throw std::invalid_argument("Invalid castling rights in FEN");
            }
        }
    }
    
    // Parse en passant square - avoid string comparison
    if (enPassantLen == 1 && enPassantStr[0] == '-') {
        enPassant = NO_SQUARE;
    } else if (enPassantLen == 2 && 
               enPassantStr[0] >= 'a' && enPassantStr[0] <= 'h' &&
               enPassantStr[1] >= '1' && enPassantStr[1] <= '8') {
        enPassant = makeSquare(enPassantStr[0] - 'a', enPassantStr[1] - '1');
    } else {
        throw std::invalid_argument("Invalid en passant square in FEN");
    }
    
    // Parse halfmove clock - fast integer parsing
    halfmoveClock = fastParseInt(halfmoveStr, halfmoveLen);
    if (halfmoveClock < 0) {
        throw std::invalid_argument("Invalid halfmove clock in FEN");
    }
    
    // Parse fullmove number - fast integer parsing
    fullmoveNumber = fastParseInt(fullmoveStr, fullmoveLen);
    if (fullmoveNumber < 0) {
        throw std::invalid_argument("Invalid fullmove number in FEN");
    }
}

// Keep original for compatibility
void Board::parseGameState(const std::string& side, const std::string& castlingStr,
                          const std::string& enPassantStr, const std::string& halfmoveStr,
                          const std::string& fullmoveStr) {
    parseGameStateOptimized(side.c_str(), static_cast<int>(side.length()),
                           castlingStr.c_str(), static_cast<int>(castlingStr.length()),
                           enPassantStr.c_str(), static_cast<int>(enPassantStr.length()),
                           halfmoveStr.c_str(), static_cast<int>(halfmoveStr.length()),
                           fullmoveStr.c_str(), static_cast<int>(fullmoveStr.length()));
}

// FEN generation helpers
std::string Board::generatePiecePlacement() const {
    std::ostringstream oss;
    
    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        
        for (int file = 0; file < 8; ++file) {
            Square sq = makeSquare(file, rank);
            Piece piece = getPiece(sq);
            
            if (piece == NO_PIECE) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    oss << emptyCount;
                    emptyCount = 0;
                }
                
                char pieceChar = '?';
                Color color = colorOf(piece);
                PieceType type = typeOf(piece);
                
                switch (type) {
                    case PAWN: pieceChar = 'p'; break;
                    case KNIGHT: pieceChar = 'n'; break;
                    case BISHOP: pieceChar = 'b'; break;
                    case ROOK: pieceChar = 'r'; break;
                    case QUEEN: pieceChar = 'q'; break;
                    case KING: pieceChar = 'k'; break;
                    default: break;
                }
                
                if (color == WHITE) {
                    pieceChar = std::toupper(pieceChar);
                }
                
                oss << pieceChar;
            }
        }
        
        if (emptyCount > 0) {
            oss << emptyCount;
        }
        
        if (rank > 0) {
            oss << '/';
        }
    }
    
    return oss.str();
}

std::string Board::generateCastlingString() const {
    std::ostringstream oss;
    
    if (castling & WHITE_KING_SIDE) oss << 'K';
    if (castling & WHITE_QUEEN_SIDE) oss << 'Q';
    if (castling & BLACK_KING_SIDE) oss << 'k';
    if (castling & BLACK_QUEEN_SIDE) oss << 'q';
    
    return oss.str().empty() ? "-" : oss.str();
}

// Attack pattern generation (simplified implementation)
Bitboard Board::getPawnAttacks(Square sq, Color color) const {
    Bitboard attacks = EMPTY_BB;
    int direction = (color == WHITE) ? NORTH : SOUTH;
    
    Square leftAttack = sq + direction + WEST;
    Square rightAttack = sq + direction + EAST;
    
    if (leftAttack >= A1 && leftAttack <= H8 && fileOf(leftAttack) != 7) {
        setBit(attacks, leftAttack);
    }
    
    if (rightAttack >= A1 && rightAttack <= H8 && fileOf(rightAttack) != 0) {
        setBit(attacks, rightAttack);
    }
    
    return attacks;
}

namespace {
const std::array<Bitboard, 64> knight_attacks = [] {
    std::array<Bitboard, 64> table{};
    for (int from = 0; from < 64; ++from)
        for (int to = 0; to < 64; ++to) {
            int df = abs(fileOf(from) - fileOf(to)), dr = abs(rankOf(from) - rankOf(to));
            if (df * dr == 2) table[from] |= 1ULL << to;
        }
    return table;
}();
const std::array<Bitboard, 64> king_attacks = [] {
    std::array<Bitboard, 64> table{};
    for (int from = 0; from < 64; ++from)
        for (int to = 0; to < 64; ++to)
            if (from != to && abs(fileOf(from) - fileOf(to)) <= 1 && abs(rankOf(from) - rankOf(to)) <= 1)
                table[from] |= 1ULL << to;
    return table;
}();
}
Bitboard Board::getKnightAttacks(Square sq) const {
    return isValidSquare(sq) ? knight_attacks[sq] : 0;
}
Bitboard Board::getKingAttacks(Square sq) const {
    return isValidSquare(sq) ? king_attacks[sq] : 0;
}

Bitboard Board::generateSlidingAttacks(Square sq, const int* directions, int numDirs, Bitboard occupied) const {
    Bitboard attacks = EMPTY_BB;
    
    for (int i = 0; i < numDirs; ++i) {
        int direction = directions[i];
        Square target = sq + direction;
        
        while (target >= A1 && target <= H8) {
            // Check for board wrapping
            int fileDistance = abs(fileOf(target) - fileOf(target - direction));
            int rankDistance = abs(rankOf(target) - rankOf(target - direction));
            
            if (fileDistance > 1 || rankDistance > 1) break; // Wrapped around board
            
            setBit(attacks, target);
            
            if (testBit(occupied, target)) break; // Blocked by piece
            
            target += direction;
        }
    }
    
    return attacks;
}

Bitboard Board::getBishopAttacks(Square sq, Bitboard occupied) const {
    static const int bishopDirections[] = {-9, -7, 7, 9};
    return generateSlidingAttacks(sq, bishopDirections, 4, occupied);
}

Bitboard Board::getRookAttacks(Square sq, Bitboard occupied) const {
    static const int rookDirections[] = {-8, -1, 1, 8};
    return generateSlidingAttacks(sq, rookDirections, 4, occupied);
}

Bitboard Board::getQueenAttacks(Square sq, Bitboard occupied) const {
    return getBishopAttacks(sq, occupied) | getRookAttacks(sq, occupied);
}

// Attack queries
bool Board::isSquareAttacked(Square sq, Color attackingColor) const {
    // Check pawn attacks
    Bitboard pawnAttackers = getPawnAttacks(sq, ~attackingColor) & getPieceBitboard(attackingColor, PAWN);
    if (pawnAttackers) return true;
    
    // Check knight attacks
    Bitboard knightAttacks = getKnightAttacks(sq);
    if (knightAttacks & getPieceBitboard(attackingColor, KNIGHT)) return true;
    
    // Check king attacks
    Bitboard kingAttacks = getKingAttacks(sq);
    if (kingAttacks & getPieceBitboard(attackingColor, KING)) return true;
    
    // Check sliding piece attacks
    Bitboard occupiedBB = getOccupiedBitboard();
    
    Bitboard bishopAttacks = getBishopAttacks(sq, occupiedBB);
    if (bishopAttacks & (getPieceBitboard(attackingColor, BISHOP) | getPieceBitboard(attackingColor, QUEEN))) {
        return true;
    }
    
    Bitboard rookAttacks = getRookAttacks(sq, occupiedBB);
    if (rookAttacks & (getPieceBitboard(attackingColor, ROOK) | getPieceBitboard(attackingColor, QUEEN))) {
        return true;
    }
    
    return false;
}

// Piece counting
int Board::getPieceCount(Color color, PieceType pieceType) const {
    return popcount(getPieceBitboard(color, pieceType));
}

int Board::getTotalPieceCount(Color color) const {
    return popcount(getColorBitboard(color));
}

// Move operations
// Legacy methods removed - using MoveGen only

// Castling helpers
void Board::executeCastling(const MoveGen& move) {
    Square from = move.from();
    Square to = move.to();
    Color color = sideToMove;
    
    // Move king
    removePiece(from);
    setPiece(to, makePiece(color, KING));
    
    // Move rook
    if (to == G1 || to == G8) { // Kingside
        Square rookFrom = (color == WHITE) ? H1 : H8;
        Square rookTo = (color == WHITE) ? F1 : F8;
        
        removePiece(rookFrom);
        setPiece(rookTo, makePiece(color, ROOK));
    } else { // Queenside
        Square rookFrom = (color == WHITE) ? A1 : A8;
        Square rookTo = (color == WHITE) ? D1 : D8;
        
        removePiece(rookFrom);
        setPiece(rookTo, makePiece(color, ROOK));
    }
}

void Board::undoCastling(const MoveGen& move) {
    Square from = move.from();
    Square to = move.to();
    Color color = sideToMove; // Side that's about to move (restored)
    
    // Restore king
    removePiece(to);
    setPiece(from, makePiece(color, KING));
    
    // Restore rook
    if (to == G1 || to == G8) { // Kingside
        Square rookFrom = (color == WHITE) ? H1 : H8;
        Square rookTo = (color == WHITE) ? F1 : F8;
        
        removePiece(rookTo);
        setPiece(rookFrom, makePiece(color, ROOK));
    } else { // Queenside
        Square rookFrom = (color == WHITE) ? A1 : A8;
        Square rookTo = (color == WHITE) ? D1 : D8;
        
        removePiece(rookTo);
        setPiece(rookFrom, makePiece(color, ROOK));
    }
}

void Board::executeEnPassant(const MoveGen& move) {
    Square from = move.from();
    Square to = move.to();
    Color color = sideToMove;
    
    // Move pawn
    removePiece(from);
    setPiece(to, makePiece(color, PAWN));
    
    // Remove captured pawn
    Square capturedSquare = to + (color == WHITE ? SOUTH : NORTH);
    removePiece(capturedSquare);
}

void Board::undoEnPassant(const MoveGen& move, const BoardState& /* state */) {
    Square from = move.from();
    Square to = move.to();
    Color color = sideToMove; // Side that's about to move (restored)
    
    // Restore moving pawn
    removePiece(to);
    setPiece(from, makePiece(color, PAWN));
    
    // Restore captured pawn
    Square capturedSquare = to + (color == WHITE ? SOUTH : NORTH);
    setPiece(capturedSquare, makePiece(~color, PAWN));
}

void Board::executePromotion(const MoveGen& move) {
    Square from = move.from();
    Square to = move.to();
    Color color = sideToMove;
    Piece capturedPiece = getPiece(to);
    
    // Remove pawn and captured piece
    removePiece(from);
    if (capturedPiece != NO_PIECE) {
        removePiece(to);
    }
    
    // Place promoted piece
    Piece promotedPiece = move.promotionPiece();
    setPiece(to, promotedPiece);
}

void Board::undoPromotion(const MoveGen& move, const BoardState& state) {
    Square from = move.from();
    Square to = move.to();
    Color color = sideToMove; // Side that's about to move (restored)
    
    // Remove promoted piece
    removePiece(to);
    
    // Restore pawn
    setPiece(from, makePiece(color, PAWN));
    
    // Restore captured piece
    if (state.capturedPiece != NO_PIECE) {
        setPiece(to, state.capturedPiece);
    }
}

void Board::updateCastlingRights(const MoveGen& move) {
    Square from = move.from();
    Square to = move.to();
    
    // Remove castling rights if king or rook moves
    if (from == E1 || to == E1) castling &= ~(WHITE_KING_SIDE | WHITE_QUEEN_SIDE);
    if (from == E8 || to == E8) castling &= ~(BLACK_KING_SIDE | BLACK_QUEEN_SIDE);
    if (from == A1 || to == A1) castling &= ~WHITE_QUEEN_SIDE;
    if (from == H1 || to == H1) castling &= ~WHITE_KING_SIDE;
    if (from == A8 || to == A8) castling &= ~BLACK_QUEEN_SIDE;
    if (from == H8 || to == H8) castling &= ~BLACK_KING_SIDE;
}

// Utility methods
void Board::clear() {
    std::fill(pieces, pieces + 12, EMPTY_BB);
    std::fill(occupied, occupied + 3, EMPTY_BB);
    castling = NO_CASTLING;
    enPassant = NO_SQUARE;
    halfmoveClock = 0;
    fullmoveNumber = 1;
    sideToMove = WHITE;
    zobristKey = 0;
    history.clear();
}

bool Board::hasNonPawnMaterial(Color color) const {
    return getPieceCount(color, KNIGHT) > 0 ||
           getPieceCount(color, BISHOP) > 0 ||
           getPieceCount(color, ROOK) > 0 ||
           getPieceCount(color, QUEEN) > 0;
}

bool Board::isEndgame() const {
    return getPhase() < 8; // Less than 8 phase points = endgame
}

int Board::getPhase() const {
    return getPieceCount(WHITE, KNIGHT) + getPieceCount(BLACK, KNIGHT) +
           getPieceCount(WHITE, BISHOP) + getPieceCount(BLACK, BISHOP) +
           2 * (getPieceCount(WHITE, ROOK) + getPieceCount(BLACK, ROOK)) +
           4 * (getPieceCount(WHITE, QUEEN) + getPieceCount(BLACK, QUEEN));
}

std::string Board::toString() const {
    std::ostringstream oss;
    
    for (int rank = 7; rank >= 0; --rank) {
        oss << (rank + 1) << " ";
        
        for (int file = 0; file < 8; ++file) {
            Square sq = makeSquare(file, rank);
            Piece piece = getPiece(sq);
            
            if (piece == NO_PIECE) {
                oss << ". ";
            } else {
                char pieceChar = '?';
                Color color = colorOf(piece);
                PieceType type = typeOf(piece);
                
                switch (type) {
                    case PAWN: pieceChar = 'P'; break;
                    case KNIGHT: pieceChar = 'N'; break;
                    case BISHOP: pieceChar = 'B'; break;
                    case ROOK: pieceChar = 'R'; break;
                    case QUEEN: pieceChar = 'Q'; break;
                    case KING: pieceChar = 'K'; break;
                    default: break;
                }
                
                if (color == BLACK) {
                    pieceChar = std::tolower(pieceChar);
                }
                
                oss << pieceChar << " ";
            }
        }
        oss << "\n";
    }
    
    oss << "  a b c d e f g h\n";
    oss << "FEN: " << toFEN() << "\n";
    
    return oss.str();
}

void Board::print() const {
    std::cout << toString() << std::endl;
}

// ============================================================================
// CHESS RULES IMPLEMENTATION
// ============================================================================

bool Board::isCheckmate(Color color) const {
    // Must be in check to be checkmate
    if (!isInCheck(color)) {
        return false;
    }
    
    // If in check and no legal moves, it's checkmate
    return !hasLegalMovesForColor(color);
}

bool Board::isStalemate(Color color) const {
    // Must not be in check to be stalemate
    if (isInCheck(color)) {
        return false;
    }
    
    // If not in check and no legal moves, it's stalemate
    return !hasLegalMovesForColor(color);
}

bool Board::isDraw() const {
    if (isInCheck(sideToMove) && !hasLegalMovesForColor(sideToMove)) return false;
    return isFiftyMoveRule() || isInsufficientMaterial() || isThreefoldRepetition() || 
           isStalemate(sideToMove);
}

bool Board::isFiftyMoveRule() const {
    return halfmoveClock >= 100;
}

bool Board::isInsufficientMaterial() const {
    if (pieces[WHITE_PAWN] | pieces[BLACK_PAWN] | pieces[WHITE_ROOK] |
        pieces[BLACK_ROOK] | pieces[WHITE_QUEEN] | pieces[BLACK_QUEEN]) return false;
    const Bitboard bishops = pieces[WHITE_BISHOP] | pieces[BLACK_BISHOP];
    const Bitboard knights = pieces[WHITE_KNIGHT] | pieces[BLACK_KNIGHT];
    if (__builtin_popcountll(bishops | knights) <= 1) return true;
    constexpr Bitboard dark = 0xAA55AA55AA55AA55ULL;
    return !knights && (!(bishops & dark) || !(bishops & ~dark));
}

bool Board::isThreefoldRepetition() const {
    // Count how many times current position has occurred
    int repetitions = 1; // Current position counts as 1
    uint64_t currentKey = zobristKey;
    
    // Only positions with the same side to move since the last irreversible move.
    const int count = static_cast<int>(history.size());
    const int earliest = std::max(0, count - halfmoveClock);
    for (int i = count - 2; i >= earliest; i -= 2) {
        if (history[i].zobristKey == currentKey && ++repetitions >= 3) return true;
    }

    return false;
}

bool Board::canCastle(Color color, bool kingside) const {
    if (!(kingside ? canCastleKingside(color) : canCastleQueenside(color))) return false;
    const Square king = color == WHITE ? E1 : E8;
    const Square rook = kingside ? king + 3 : king - 4;
    if (getPiece(king) != makePiece(color, KING) || getPiece(rook) != makePiece(color, ROOK)) return false;
    const int step = kingside ? 1 : -1;
    for (Square square = king + step; square != rook; square += step)
        if (!isEmpty(square)) return false;
    for (int i = 0; i <= 2; ++i)
        if (isSquareAttacked(king + step * i, ~color)) return false;
    return true;
}

bool Board::hasLegalEnPassant() const {
    if (enPassant == NO_SQUARE) return false;
    Bitboard candidates = getPawnAttacks(enPassant, ~sideToMove) & getPieceBitboard(sideToMove, PAWN);
    while (candidates) {
        Square from = static_cast<Square>(__builtin_ctzll(candidates));
        candidates &= candidates - 1;
        if (isLegalMove(MoveGen(from, enPassant, MoveGen::MoveType::EN_PASSANT,
                               NO_PIECE, makePiece(~sideToMove, PAWN)), sideToMove)) return true;
    }
    return false;
}

bool Board::isLegalMove(const MoveGen& move, Color color) const {
    const Square from = move.from(), to = move.to();
    if (!isValidSquare(from) || !isValidSquare(to) || from == to) return false;
    const Piece piece = getPiece(from), victim = getPiece(to);
    if (piece == NO_PIECE || colorOf(piece) != color ||
        (victim != NO_PIECE && (colorOf(victim) == color || typeOf(victim) == KING))) return false;

    const PieceType type = typeOf(piece);
    const Bitboard target = 1ULL << to;
    Square captured = victim != NO_PIECE ? to : NO_SQUARE;
    if (move.isCastling()) {
        const Square home = color == WHITE ? E1 : E8;
        if (type != KING || from != home || (to != home + 2 && to != home - 2) ||
            !canCastle(color, to > from)) return false;
    } else if (type == PAWN) {
        const int forward = color == WHITE ? NORTH : SOUTH;
        const bool last_rank = rankOf(to) == (color == WHITE ? 7 : 0);
        if (last_rank != move.isPromotion()) return false;
        if (move.isPromotion() && (colorOf(move.promotionPiece()) != color ||
            typeOf(move.promotionPiece()) < KNIGHT || typeOf(move.promotionPiece()) > QUEEN)) return false;
        if (move.isEnPassant()) {
            if (to != enPassant || victim != NO_PIECE) return false;
            captured = to - forward;
            if (getPiece(captured) != makePiece(~color, PAWN)) return false;
        }
        if (captured != NO_SQUARE) {
            if (!(getPawnAttacks(from, color) & target)) return false;
        } else if (to != from + forward) {
            if (to != from + 2 * forward || rankOf(from) != (color == WHITE ? 1 : 6) ||
                !isEmpty(from + forward)) return false;
        }
    } else {
        if (move.isPromotion() || move.isEnPassant() || move.isDoublePawnPush()) return false;
        Bitboard attacks = type == KNIGHT ? getKnightAttacks(from) :
                           type == BISHOP ? getBishopAttacks(from, occupied[2]) :
                           type == ROOK ? getRookAttacks(from, occupied[2]) :
                           type == QUEEN ? getQueenAttacks(from, occupied[2]) : getKingAttacks(from);
        if (!(attacks & target)) return false;
    }

    return isLegalGeneratedMove(move, color);
}

bool Board::isLegalGeneratedMove(const MoveGen& move, Color color) const {
    const Square from = move.from(), to = move.to();
    const bool king_move = testBit(getPieceBitboard(color, KING), from);
    if (testBit(getPieceBitboard(~color, KING), to)) return false;
    const Square captured = move.isEnPassant() ? to + (color == WHITE ? SOUTH : NORTH) : to;
    const Bitboard target = 1ULL << to;
    // Test the resulting king attacks with local occupancy; no Board/history copy.
    Bitboard remaining = captured == NO_SQUARE ? ~0ULL : ~(1ULL << captured);
    Bitboard occupancy = (occupied[2] & ~(1ULL << from) & remaining) | target;
    if (move.isCastling()) {
        const Square rook_from = to > from ? from + 3 : from - 4;
        const Square rook_to = to > from ? from + 1 : from - 1;
        occupancy = (occupancy & ~(1ULL << rook_from)) | (1ULL << rook_to);
    }
    const Square king = king_move ? to : getKingSquare(color);
    if (king == NO_SQUARE) return false;
    const Color enemy = ~color;
    return !(remaining & (
        (getPawnAttacks(king, color) & getPieceBitboard(enemy, PAWN)) |
        (getKnightAttacks(king) & getPieceBitboard(enemy, KNIGHT)) |
        (getKingAttacks(king) & getPieceBitboard(enemy, KING)) |
        (getBishopAttacks(king, occupancy) & (getPieceBitboard(enemy, BISHOP) | getPieceBitboard(enemy, QUEEN))) |
        (getRookAttacks(king, occupancy) & (getPieceBitboard(enemy, ROOK) | getPieceBitboard(enemy, QUEEN)))));
}

bool Board::wouldBeInCheck(const MoveGen& move, Color color) const {
    return !isLegalMove(move, color);
}

void Board::makeNullMove() {
    BoardState state;
    state.castling = castling;
    state.enPassant = enPassant;
    state.halfmoveClock = halfmoveClock;
    state.fullmoveNumber = fullmoveNumber;
    state.sideToMove = sideToMove;
    state.zobristKey = zobristKey;
    history.push_back(state);
    if (hasLegalEnPassant()) zobristKey ^= zobristEnPassant[fileOf(enPassant)];
    enPassant = NO_SQUARE;
    sideToMove = ~sideToMove;
    zobristKey ^= zobristSideToMove;
    halfmoveClock = 0;
}

void Board::unmakeNullMove() {
    const BoardState state = history.back();
    history.pop_back();
    castling = state.castling;
    enPassant = state.enPassant;
    halfmoveClock = state.halfmoveClock;
    fullmoveNumber = state.fullmoveNumber;
    sideToMove = state.sideToMove;
    zobristKey = state.zobristKey;
}

bool Board::makeMove(const MoveGen& move) {
    return isLegalMove(move, sideToMove) && doMove(move);
}

bool Board::makeGeneratedMove(const MoveGen& move) {
    return isLegalGeneratedMove(move, sideToMove) && doMove(move);
}

bool Board::doMove(const MoveGen& move) {
    // Save current state for undo
    BoardState state;
    state.castling = castling;
    state.enPassant = enPassant;
    state.halfmoveClock = halfmoveClock;
    state.fullmoveNumber = fullmoveNumber;
    state.sideToMove = sideToMove;
    state.zobristKey = zobristKey;
    state.capturedPiece = getPiece(move.to());
    
    history.push_back(state);
    if (hasLegalEnPassant()) zobristKey ^= zobristEnPassant[fileOf(enPassant)];
    zobristKey ^= zobristCastling[castling];
    
    Square from = move.from();
    Square to = move.to();
    Piece movingPiece = getPiece(from);
    Piece capturedPiece = getPiece(to);
    
    // Handle special moves
    if (move.isCastling()) {
        executeCastling(move);
    } else if (move.isEnPassant()) {
        executeEnPassant(move);
    } else if (move.isPromotion()) {
        executePromotion(move);
    } else {
        // Normal move
        removePiece(from);
        if (capturedPiece != NO_PIECE) {
            removePiece(to); // Remove captured piece
        }
        setPiece(to, movingPiece);
    }
    
    // Update castling rights
    updateCastlingRights(move);
    
    // Update en passant square
    enPassant = NO_SQUARE;
    
    // Check for double pawn push
    if (typeOf(movingPiece) == PAWN && abs(to - from) == 16) {
        enPassant = static_cast<Square>((from + to) / 2); // En passant target square
    }
    
    // Update move counters
    halfmoveClock++;
    if (typeOf(movingPiece) == PAWN || capturedPiece != NO_PIECE) {
        halfmoveClock = 0;
    }
    
    if (sideToMove == BLACK) {
        fullmoveNumber++;
    }
    
    // Switch sides
    sideToMove = ~sideToMove;
    
    updateOccupancy();
    zobristKey ^= zobristSideToMove ^ zobristCastling[castling];
    if (hasLegalEnPassant()) zobristKey ^= zobristEnPassant[fileOf(enPassant)];
    return true;
}

void Board::unmakeMove(const MoveGen& move) {
    if (history.empty()) return;
    
    BoardState state = history.back();
    history.pop_back();
    
    // Restore game state
    castling = state.castling;
    enPassant = state.enPassant;
    halfmoveClock = state.halfmoveClock;
    fullmoveNumber = state.fullmoveNumber;
    sideToMove = state.sideToMove;
    
    Square from = move.from();
    Square to = move.to();
    
    // Handle special moves
    if (move.isCastling()) {
        undoCastling(move);
    } else if (move.isEnPassant()) {
        undoEnPassant(move, state);
    } else if (move.isPromotion()) {
        undoPromotion(move, state);
    } else {
        // Normal move
        Piece movingPiece = getPiece(to);
        removePiece(to);
        setPiece(from, movingPiece);
        
        // Restore captured piece
        if (state.capturedPiece != NO_PIECE) {
            setPiece(to, state.capturedPiece);
        }
    }
    
    updateOccupancy();
    zobristKey = state.zobristKey;
}

// Helper methods
bool Board::hasLegalMovesForColor(Color color) const {
    MoveGenList<> moves;
    
    // Generate all pseudo-legal moves
    generatePawnMoves(*this, moves, color);
    generateKnightMoves(*this, moves, color);
    generateBishopMoves(*this, moves, color);
    generateRookMoves(*this, moves, color);
    generateQueenMoves(*this, moves, color);
    generateKingMoves(*this, moves, color);
    
    // Check if any move is legal
    for (size_t i = 0; i < moves.size(); ++i) {
        if (isLegalGeneratedMove(moves[i], color)) {
            return true;
        }
    }
    
    return false;
}

// Temporary compatibility methods for existing tests (deprecated)
bool Board::makeMove(const Move& move) {
    // Convert legacy Move to MoveGen
    MoveGen::MoveType moveType = MoveGen::MoveType::NORMAL;
    if (move.isCastling()) moveType = MoveGen::MoveType::CASTLING;
    else if (move.isEnPassant()) moveType = MoveGen::MoveType::EN_PASSANT;
    else if (move.isPromotion()) moveType = MoveGen::MoveType::PROMOTION;
    
    Piece promotionPiece = NO_PIECE;
    if (move.isPromotion()) {
        promotionPiece = makePiece(sideToMove, move.promotionType());
    }
    
    MoveGen moveGen(move.from(), move.to(), moveType, promotionPiece);
    return makeMove(moveGen);
}

void Board::unmakeMove(const Move& move) {
    // Convert legacy Move to MoveGen  
    MoveGen::MoveType moveType = MoveGen::MoveType::NORMAL;
    if (move.isCastling()) moveType = MoveGen::MoveType::CASTLING;
    else if (move.isEnPassant()) moveType = MoveGen::MoveType::EN_PASSANT;
    else if (move.isPromotion()) moveType = MoveGen::MoveType::PROMOTION;
    
    Piece promotionPiece = NO_PIECE;
    if (move.isPromotion()) {
        promotionPiece = makePiece(~sideToMove, move.promotionType()); // Opposite side after undo
    }
    
    MoveGen moveGen(move.from(), move.to(), moveType, promotionPiece);
    unmakeMove(moveGen);
}

} // namespace opera
