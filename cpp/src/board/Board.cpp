#include "Board.h"
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
    
    std::istringstream iss(fen);
    std::string placement, side, castlingStr, enPassantStr, halfmoveStr, fullmoveStr;
    
    iss >> placement >> side >> castlingStr >> enPassantStr >> halfmoveStr >> fullmoveStr;
    
    if (iss.fail() || placement.empty() || side.empty() || castlingStr.empty() || 
        enPassantStr.empty() || halfmoveStr.empty() || fullmoveStr.empty()) {
        throw std::invalid_argument("Invalid FEN string");
    }
    
    parsePiecePlacement(placement);
    parseGameState(side, castlingStr, enPassantStr, halfmoveStr, fullmoveStr);
    
    updateOccupancy();
    zobristKey = computeZobristKey();
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

// FEN parsing helpers
void Board::parsePiecePlacement(const std::string& placement) {
    int rank = 7; // Start from rank 8 (index 7)
    int file = 0;
    
    for (char c : placement) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (std::isdigit(c)) {
            file += c - '0'; // Skip empty squares
        } else {
            // Parse piece
            Piece piece = NO_PIECE;
            switch (std::tolower(c)) {
                case 'p': piece = makePiece(std::isupper(c) ? WHITE : BLACK, PAWN); break;
                case 'n': piece = makePiece(std::isupper(c) ? WHITE : BLACK, KNIGHT); break;
                case 'b': piece = makePiece(std::isupper(c) ? WHITE : BLACK, BISHOP); break;
                case 'r': piece = makePiece(std::isupper(c) ? WHITE : BLACK, ROOK); break;
                case 'q': piece = makePiece(std::isupper(c) ? WHITE : BLACK, QUEEN); break;
                case 'k': piece = makePiece(std::isupper(c) ? WHITE : BLACK, KING); break;
                default: throw std::invalid_argument("Invalid piece character in FEN");
            }
            
            if (rank < 0 || rank > 7 || file < 0 || file > 7) {
                throw std::invalid_argument("Invalid square in FEN");
            }
            
            setPiece(makeSquare(file, rank), piece);
            file++;
        }
    }
}

void Board::parseGameState(const std::string& side, const std::string& castlingStr,
                          const std::string& enPassantStr, const std::string& halfmoveStr,
                          const std::string& fullmoveStr) {
    // Parse side to move
    if (side == "w") {
        sideToMove = WHITE;
    } else if (side == "b") {
        sideToMove = BLACK;
    } else {
        throw std::invalid_argument("Invalid side to move in FEN");
    }
    
    // Parse castling rights
    castling = NO_CASTLING;
    if (castlingStr != "-") {
        for (char c : castlingStr) {
            switch (c) {
                case 'K': castling |= WHITE_KING_SIDE; break;
                case 'Q': castling |= WHITE_QUEEN_SIDE; break;
                case 'k': castling |= BLACK_KING_SIDE; break;
                case 'q': castling |= BLACK_QUEEN_SIDE; break;
                default: throw std::invalid_argument("Invalid castling rights in FEN");
            }
        }
    }
    
    // Parse en passant square
    if (enPassantStr == "-") {
        enPassant = NO_SQUARE;
    } else {
        if (enPassantStr.length() != 2 || enPassantStr[0] < 'a' || enPassantStr[0] > 'h' ||
            enPassantStr[1] < '1' || enPassantStr[1] > '8') {
            throw std::invalid_argument("Invalid en passant square in FEN");
        }
        enPassant = makeSquare(enPassantStr[0] - 'a', enPassantStr[1] - '1');
    }
    
    // Parse halfmove clock
    try {
        halfmoveClock = std::stoi(halfmoveStr);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid halfmove clock in FEN");
    }
    
    // Parse fullmove number
    try {
        fullmoveNumber = std::stoi(fullmoveStr);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid fullmove number in FEN");
    }
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
void Board::makeMove(const Move& move) {
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
        removePiece(to); // Remove captured piece
        setPiece(to, movingPiece);
    }
    
    // Update castling rights
    updateCastlingRights(move);
    
    // Update en passant square
    enPassant = NO_SQUARE;
    
    // Check for double pawn push
    if (typeOf(movingPiece) == PAWN && abs(to - from) == 16) {
        enPassant = (from + to) / 2; // En passant target square
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
}

void Board::unmakeMove(const Move& move) {
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

// Castling helpers
void Board::executeCastling(const Move& move) {
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

void Board::undoCastling(const Move& move) {
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

void Board::executeEnPassant(const Move& move) {
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

void Board::undoEnPassant(const Move& move, const BoardState& /* state */) {
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

void Board::executePromotion(const Move& move) {
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
    Piece promotedPiece = makePiece(color, move.promotionType());
    setPiece(to, promotedPiece);
}

void Board::undoPromotion(const Move& move, const BoardState& state) {
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

void Board::updateCastlingRights(const Move& move) {
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

} // namespace opera