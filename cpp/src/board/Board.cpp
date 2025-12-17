#include "Board.h"
#include "MoveGen.h"
#include <sstream>
#include <iostream>
#include <random>
#include <algorithm>
#include <cassert>

namespace opera {

// Static zobrist initialization
uint64_t Board::zobristPieces[64][12];
uint64_t Board::zobristSideToMove;
uint64_t Board::zobristCastling[16];
uint64_t Board::zobristEnPassant[64];
bool Board::zobristInitialized = false;

void Board::initializeZobrist() {
    if (zobristInitialized) return;
    
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
    }
}

void Board::removePiece(Square sq) {
    for (int piece = WHITE_PAWN; piece <= BLACK_KING; ++piece) {
        clearBit(pieces[piece], sq);
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
    if (enPassant != NO_SQUARE) {
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
    if (enPassant != NO_SQUARE) {
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

Bitboard Board::getKnightAttacks(Square sq) const {
    static const int knightMoves[] = {-17, -15, -10, -6, 6, 10, 15, 17};
    Bitboard attacks = EMPTY_BB;
    
    for (int move : knightMoves) {
        Square target = sq + move;
        if (target >= A1 && target <= H8) {
            int fileDistance = abs(fileOf(target) - fileOf(sq));
            int rankDistance = abs(rankOf(target) - rankOf(sq));
            
            if ((fileDistance == 2 && rankDistance == 1) || (fileDistance == 1 && rankDistance == 2)) {
                setBit(attacks, target);
            }
        }
    }
    
    return attacks;
}

Bitboard Board::getKingAttacks(Square sq) const {
    static const int kingMoves[] = {-9, -8, -7, -1, 1, 7, 8, 9};
    Bitboard attacks = EMPTY_BB;
    
    for (int move : kingMoves) {
        Square target = sq + move;
        if (target >= A1 && target <= H8) {
            int fileDistance = abs(fileOf(target) - fileOf(sq));
            int rankDistance = abs(rankOf(target) - rankOf(sq));
            
            if (fileDistance <= 1 && rankDistance <= 1) {
                setBit(attacks, target);
            }
        }
    }
    
    return attacks;
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
    return isFiftyMoveRule() || isInsufficientMaterial() || isThreefoldRepetition() || 
           isStalemate(sideToMove);
}

bool Board::isFiftyMoveRule() const {
    return halfmoveClock >= 50; // 50 half-moves for 50-move rule
}

bool Board::isInsufficientMaterial() const {
    // Count pieces for both sides
    int whitePieces = 0, blackPieces = 0;
    int whiteBishops = 0, blackBishops = 0;
    int whiteKnights = 0, blackKnights = 0;
    bool whiteBishopOnLight = false, whiteBishopOnDark = false;
    bool blackBishopOnLight = false, blackBishopOnDark = false;
    
    for (int sq = A1; sq <= H8; ++sq) {
        Piece piece = getPiece(static_cast<Square>(sq));
        if (piece == NO_PIECE) continue;
        
        Color pieceColor = colorOf(piece);
        PieceType pieceType = typeOf(piece);
        
        if (pieceColor == WHITE) {
            whitePieces++;
            if (pieceType == BISHOP) {
                whiteBishops++;
                if ((fileOf(static_cast<Square>(sq)) + rankOf(static_cast<Square>(sq))) % 2 == 0) {
                    whiteBishopOnDark = true;
                } else {
                    whiteBishopOnLight = true;
                }
            } else if (pieceType == KNIGHT) {
                whiteKnights++;
            } else if (pieceType == PAWN || pieceType == ROOK || pieceType == QUEEN) {
                return false; // These pieces can force mate
            }
        } else {
            blackPieces++;
            if (pieceType == BISHOP) {
                blackBishops++;
                if ((fileOf(static_cast<Square>(sq)) + rankOf(static_cast<Square>(sq))) % 2 == 0) {
                    blackBishopOnDark = true;
                } else {
                    blackBishopOnLight = true;
                }
            } else if (pieceType == KNIGHT) {
                blackKnights++;
            } else if (pieceType == PAWN || pieceType == ROOK || pieceType == QUEEN) {
                return false; // These pieces can force mate
            }
        }
    }
    
    // King vs King
    if (whitePieces == 1 && blackPieces == 1) {
        return true;
    }
    
    // King and Bishop/Knight vs King
    if ((whitePieces == 2 && blackPieces == 1 && (whiteBishops == 1 || whiteKnights == 1)) ||
        (blackPieces == 2 && whitePieces == 1 && (blackBishops == 1 || blackKnights == 1))) {
        return true;
    }
    
    // King and Bishop vs King and Bishop (same color squares)
    if (whitePieces == 2 && blackPieces == 2 && whiteBishops == 1 && blackBishops == 1) {
        if ((whiteBishopOnLight && blackBishopOnLight) || 
            (whiteBishopOnDark && blackBishopOnDark)) {
            return true;
        }
    }
    
    return false;
}

bool Board::isThreefoldRepetition() const {
    // Count how many times current position has occurred
    int repetitions = 1; // Current position counts as 1
    uint64_t currentKey = zobristKey;
    
    // Check against all positions in history
    for (const auto& state : history) {
        if (state.zobristKey == currentKey) {
            repetitions++;
            if (repetitions >= 3) {
                return true;
            }
        }
    }
    
    return false;
}

bool Board::isLegalMove(const MoveGen& move, Color color) const {
    // Make the move on a temporary board
    Board tempBoard = *this;
    
    // Try to make the move using MoveGen system directly
    // Temporarily allow illegal moves for testing check
    tempBoard.sideToMove = color;
    Square from = move.from();
    Square to = move.to();
    
    // Execute move without legality check for testing
    Piece movingPiece = tempBoard.getPiece(from);
    
    tempBoard.removePiece(from);
    
    // Handle special en passant case
    if (move.isEnPassant()) {
        // En passant: captured pawn is not on destination square
        Square capturedSquare = to + (color == WHITE ? SOUTH : NORTH);
        tempBoard.removePiece(capturedSquare);
        tempBoard.setPiece(to, movingPiece);
    } else {
        // Normal move or capture
        Piece capturedPiece = tempBoard.getPiece(to);
        if (capturedPiece != NO_PIECE) {
            tempBoard.removePiece(to);
        }
        tempBoard.setPiece(to, movingPiece);
    }
    
    tempBoard.updateOccupancy();
    
    // Check if the king is in check after the move
    return !tempBoard.isInCheck(color);
}

bool Board::wouldBeInCheck(const MoveGen& move, Color color) const {
    Board tempBoard = *this;
    
    // Execute move directly for testing
    Square from = move.from();
    Square to = move.to();
    
    Piece movingPiece = tempBoard.getPiece(from);
    Piece capturedPiece = tempBoard.getPiece(to);
    
    tempBoard.removePiece(from);
    if (capturedPiece != NO_PIECE) {
        tempBoard.removePiece(to);
    }
    tempBoard.setPiece(to, movingPiece);
    tempBoard.updateOccupancy();
    
    return tempBoard.isInCheck(color);
}

bool Board::makeMove(const MoveGen& move) {
    // Check if move is legal first
    if (!isLegalMove(move, sideToMove)) {
        return false;
    }
    
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
    if (move.isDoublePawnPush()) {
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
    zobristKey = computeZobristKey();
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
    zobristKey = state.zobristKey;
    
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
        if (isLegalMove(moves[i], color)) {
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