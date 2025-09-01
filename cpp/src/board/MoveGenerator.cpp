#include "MoveGen.h"
#include "Board.h"
#include "Types.h"

namespace opera {

void generatePawnMoves(const Board& board, MoveGenList<>& moves, Color color) {
    const Bitboard pawns = board.getPieceBitboard(color, PAWN);
    const Bitboard occupied = board.getOccupiedBitboard();
    const Bitboard enemyPieces = board.getColorBitboard(color == WHITE ? BLACK : WHITE);
    const Square enPassantSquare = board.getEnPassantSquare();
    
    const int pawnDirection = (color == WHITE) ? NORTH : SOUTH;
    const Rank startingRank = (color == WHITE) ? 1 : 6;
    const Rank promotionRank = (color == WHITE) ? 7 : 0;
    
    // Iterate through all pawns of the given color
    Bitboard pawnsCopy = pawns;
    while (pawnsCopy) {
        const Square fromSquare = static_cast<Square>(__builtin_ctzll(pawnsCopy));
        pawnsCopy &= pawnsCopy - 1; // Clear the least significant bit
        
        const File fromFile = fileOf(fromSquare);
        const Rank fromRank = rankOf(fromSquare);
        const Square oneSquareForward = fromSquare + pawnDirection;
        const Square twoSquaresForward = fromSquare + (2 * pawnDirection);
        
        // Single pawn push
        if (oneSquareForward >= A1 && oneSquareForward <= H8 && !testBit(occupied, oneSquareForward)) {
            if (rankOf(oneSquareForward) == promotionRank) {
                // Promotion moves - use correct color for promotion pieces
                const Piece queen = makePiece(color, QUEEN);
                const Piece rook = makePiece(color, ROOK);
                const Piece bishop = makePiece(color, BISHOP);
                const Piece knight = makePiece(color, KNIGHT);
                moves.add(MoveGen(fromSquare, oneSquareForward, MoveGen::MoveType::PROMOTION, queen));
                moves.add(MoveGen(fromSquare, oneSquareForward, MoveGen::MoveType::PROMOTION, rook));
                moves.add(MoveGen(fromSquare, oneSquareForward, MoveGen::MoveType::PROMOTION, bishop));
                moves.add(MoveGen(fromSquare, oneSquareForward, MoveGen::MoveType::PROMOTION, knight));
            } else {
                moves.add(MoveGen(fromSquare, oneSquareForward, MoveGen::MoveType::NORMAL));
            }
            
            // Double pawn push from starting position
            if (fromRank == startingRank && twoSquaresForward >= A1 && twoSquaresForward <= H8 && 
                !testBit(occupied, twoSquaresForward)) {
                moves.add(MoveGen(fromSquare, twoSquaresForward, MoveGen::MoveType::DOUBLE_PAWN_PUSH));
            }
        }
        
        // Pawn captures
        const int captureDirections[] = {pawnDirection + WEST, pawnDirection + EAST};
        
        for (int i = 0; i < 2; ++i) {
            const Square captureSquare = fromSquare + captureDirections[i];
            
            // Check bounds and file wrapping
            if (captureSquare >= A1 && captureSquare <= H8) {
                const File captureFile = fileOf(captureSquare);
                
                // Prevent wrapping around the board
                if (abs(captureFile - fromFile) == 1) {
                    if (testBit(enemyPieces, captureSquare)) {
                        // Regular capture
                        const Piece capturedPiece = board.getPiece(captureSquare);
                        
                        if (rankOf(captureSquare) == promotionRank) {
                            // Capture promotion - use correct color for promotion pieces
                            const Piece queen = makePiece(color, QUEEN);
                            const Piece rook = makePiece(color, ROOK);
                            const Piece bishop = makePiece(color, BISHOP);
                            const Piece knight = makePiece(color, KNIGHT);
                            moves.add(MoveGen(fromSquare, captureSquare, MoveGen::MoveType::PROMOTION, queen, capturedPiece));
                            moves.add(MoveGen(fromSquare, captureSquare, MoveGen::MoveType::PROMOTION, rook, capturedPiece));
                            moves.add(MoveGen(fromSquare, captureSquare, MoveGen::MoveType::PROMOTION, bishop, capturedPiece));
                            moves.add(MoveGen(fromSquare, captureSquare, MoveGen::MoveType::PROMOTION, knight, capturedPiece));
                        } else {
                            moves.add(MoveGen(fromSquare, captureSquare, MoveGen::MoveType::NORMAL, NO_PIECE, capturedPiece));
                        }
                    } else if (captureSquare == enPassantSquare && enPassantSquare != NO_SQUARE) {
                        // En passant capture - only valid for pawns on the correct rank
                        const Rank enPassantRank = (color == WHITE) ? 4 : 3; // 5th rank for white, 4th for black (0-indexed)
                        if (fromRank == enPassantRank) {
                            const Square capturedPawnSquare = enPassantSquare - pawnDirection;
                            const Piece capturedPawn = board.getPiece(capturedPawnSquare);
                            moves.add(MoveGen(fromSquare, captureSquare, MoveGen::MoveType::EN_PASSANT, NO_PIECE, capturedPawn));
                        }
                    }
                }
            }
        }
    }
}

void generateKnightMoves(const Board& board, MoveGenList<>& moves, Color color) {
    const Bitboard knights = board.getPieceBitboard(color, KNIGHT);
    
    // Knight move offsets: L-shaped moves (2+1 or 1+2 squares)
    const int knightOffsets[8] = {
        -17,  // 2 up, 1 left    (north-north-west)
        -15,  // 2 up, 1 right   (north-north-east)
        -10,  // 1 up, 2 left    (north-west-west)
         -6,  // 1 up, 2 right   (north-east-east)
          6,  // 1 down, 2 right (south-east-east)
         10,  // 1 down, 2 left  (south-west-west)
         15,  // 2 down, 1 left  (south-south-west)
         17   // 2 down, 1 right (south-south-east)
    };
    
    // Iterate through all knights of the given color
    Bitboard knightsCopy = knights;
    while (knightsCopy) {
        const Square fromSquare = static_cast<Square>(__builtin_ctzll(knightsCopy));
        knightsCopy &= knightsCopy - 1; // Clear the least significant bit
        
        const File fromFile = fileOf(fromSquare);
        const Rank fromRank = rankOf(fromSquare);
        
        // Try all 8 knight moves
        for (int i = 0; i < 8; ++i) {
            const int toSquareInt = static_cast<int>(fromSquare) + knightOffsets[i];
            
            // Check if destination is within board bounds
            if (toSquareInt < static_cast<int>(A1) || toSquareInt > static_cast<int>(H8)) {
                continue;
            }
            
            const Square toSquare = static_cast<Square>(toSquareInt);
            const File toFile = fileOf(toSquare);
            const Rank toRank = rankOf(toSquare);
            
            // Verify the move is actually L-shaped (prevent wrapping around board)
            const int fileDiff = abs(toFile - fromFile);
            const int rankDiff = abs(toRank - fromRank);
            
            if (!((fileDiff == 2 && rankDiff == 1) || (fileDiff == 1 && rankDiff == 2))) {
                continue; // Not a valid L-shaped move (board wrap-around)
            }
            
            // Check what piece (if any) is on the destination square
            const Piece targetPiece = board.getPiece(toSquare);
            
            // Can't move to square occupied by own piece
            if (targetPiece != NO_PIECE && colorOf(targetPiece) == color) {
                continue; // Blocked by own piece
            }
            
            // Check if it's a capture
            if (targetPiece != NO_PIECE) {
                // It's a capture (must be enemy piece since we filtered out own pieces above)
                moves.add(MoveGen(fromSquare, toSquare, MoveGen::MoveType::NORMAL, NO_PIECE, targetPiece));
            } else {
                // It's a quiet move
                moves.add(MoveGen(fromSquare, toSquare, MoveGen::MoveType::NORMAL));
            }
        }
    }
}

void generateBishopMoves(const Board& board, MoveGenList<>& moves, Color color) {
    const Bitboard bishops = board.getPieceBitboard(color, BISHOP);
    const Bitboard occupied = board.getOccupiedBitboard();
    
    // Iterate through all bishops of the given color
    Bitboard bishopsCopy = bishops;
    while (bishopsCopy) {
        const Square fromSquare = static_cast<Square>(__builtin_ctzll(bishopsCopy));
        bishopsCopy &= bishopsCopy - 1; // Clear the least significant bit
        
        // Get all squares this bishop can attack
        const Bitboard attackBitboard = board.getBishopAttacks(fromSquare, occupied);
        
        // Convert attack bitboard to moves
        Bitboard attacksCopy = attackBitboard;
        while (attacksCopy) {
            const Square toSquare = static_cast<Square>(__builtin_ctzll(attacksCopy));
            attacksCopy &= attacksCopy - 1; // Clear the least significant bit
            
            // Check what piece (if any) is on the destination square
            const Piece targetPiece = board.getPiece(toSquare);
            
            // Can't move to square occupied by own piece
            if (targetPiece != NO_PIECE && colorOf(targetPiece) == color) {
                continue; // Skip this square
            }
            
            // Check if it's a capture or quiet move
            if (targetPiece != NO_PIECE) {
                // It's a capture (enemy piece)
                moves.add(MoveGen(fromSquare, toSquare, MoveGen::MoveType::NORMAL, NO_PIECE, targetPiece));
            } else {
                // It's a quiet move
                moves.add(MoveGen(fromSquare, toSquare, MoveGen::MoveType::NORMAL));
            }
        }
    }
}

void generateRookMoves(const Board& board, MoveGenList<>& moves, Color color) {
    const Bitboard rooks = board.getPieceBitboard(color, ROOK);
    const Bitboard occupied = board.getOccupiedBitboard();
    
    // Iterate through all rooks of the given color
    Bitboard rooksCopy = rooks;
    while (rooksCopy) {
        const Square fromSquare = static_cast<Square>(__builtin_ctzll(rooksCopy));
        rooksCopy &= rooksCopy - 1; // Clear the least significant bit
        
        // Get all squares this rook can attack
        const Bitboard attackBitboard = board.getRookAttacks(fromSquare, occupied);
        
        // Convert attack bitboard to moves
        Bitboard attacksCopy = attackBitboard;
        while (attacksCopy) {
            const Square toSquare = static_cast<Square>(__builtin_ctzll(attacksCopy));
            attacksCopy &= attacksCopy - 1; // Clear the least significant bit
            
            // Check what piece (if any) is on the destination square
            const Piece targetPiece = board.getPiece(toSquare);
            
            // Can't move to square occupied by own piece
            if (targetPiece != NO_PIECE && colorOf(targetPiece) == color) {
                continue; // Skip this square
            }
            
            // Check if it's a capture or quiet move
            if (targetPiece != NO_PIECE) {
                // It's a capture (enemy piece)
                moves.add(MoveGen(fromSquare, toSquare, MoveGen::MoveType::NORMAL, NO_PIECE, targetPiece));
            } else {
                // It's a quiet move
                moves.add(MoveGen(fromSquare, toSquare, MoveGen::MoveType::NORMAL));
            }
        }
    }
}

void generateQueenMoves(const Board& board, MoveGenList<>& moves, Color color) {
    const Bitboard queens = board.getPieceBitboard(color, QUEEN);
    const Bitboard occupied = board.getOccupiedBitboard();
    
    // Iterate through all queens of the given color
    Bitboard queensCopy = queens;
    while (queensCopy) {
        const Square fromSquare = static_cast<Square>(__builtin_ctzll(queensCopy));
        queensCopy &= queensCopy - 1; // Clear the least significant bit
        
        // Get all squares this queen can attack
        const Bitboard attackBitboard = board.getQueenAttacks(fromSquare, occupied);
        
        // Convert attack bitboard to moves
        Bitboard attacksCopy = attackBitboard;
        while (attacksCopy) {
            const Square toSquare = static_cast<Square>(__builtin_ctzll(attacksCopy));
            attacksCopy &= attacksCopy - 1; // Clear the least significant bit
            
            // Check what piece (if any) is on the destination square
            const Piece targetPiece = board.getPiece(toSquare);
            
            // Can't move to square occupied by own piece
            if (targetPiece != NO_PIECE && colorOf(targetPiece) == color) {
                continue; // Skip this square
            }
            
            // Check if it's a capture or quiet move
            if (targetPiece != NO_PIECE) {
                // It's a capture (enemy piece)
                moves.add(MoveGen(fromSquare, toSquare, MoveGen::MoveType::NORMAL, NO_PIECE, targetPiece));
            } else {
                // It's a quiet move
                moves.add(MoveGen(fromSquare, toSquare, MoveGen::MoveType::NORMAL));
            }
        }
    }
}

void generateKingMoves(const Board& board, MoveGenList<>& moves, Color color) {
    const Bitboard kings = board.getPieceBitboard(color, KING);
    
    // King move offsets: all 8 adjacent squares
    const int kingOffsets[8] = {
        -9,  // Southwest (1 left, 1 down)
        -8,  // South (straight down)
        -7,  // Southeast (1 right, 1 down)
        -1,  // West (1 left)
         1,  // East (1 right)
         7,  // Northwest (1 left, 1 up)
         8,  // North (straight up)
         9   // Northeast (1 right, 1 up)
    };
    
    // Iterate through all kings of the given color (should be only one)
    Bitboard kingsCopy = kings;
    while (kingsCopy) {
        const Square fromSquare = static_cast<Square>(__builtin_ctzll(kingsCopy));
        kingsCopy &= kingsCopy - 1; // Clear the least significant bit
        
        const File fromFile = fileOf(fromSquare);
        const Rank fromRank = rankOf(fromSquare);
        
        // Try all 8 king moves
        for (int i = 0; i < 8; ++i) {
            const int toSquareInt = static_cast<int>(fromSquare) + kingOffsets[i];
            
            // Check if destination is within board bounds
            if (toSquareInt < static_cast<int>(A1) || toSquareInt > static_cast<int>(H8)) {
                continue;
            }
            
            const Square toSquare = static_cast<Square>(toSquareInt);
            const File toFile = fileOf(toSquare);
            const Rank toRank = rankOf(toSquare);
            
            // Verify the move is actually adjacent (prevent wrapping around board)
            const int fileDiff = abs(toFile - fromFile);
            const int rankDiff = abs(toRank - fromRank);
            
            if (fileDiff > 1 || rankDiff > 1) {
                continue; // Board wrap-around, not a valid king move
            }
            
            // Check what piece (if any) is on the destination square
            const Piece targetPiece = board.getPiece(toSquare);
            
            // Can't move to square occupied by own piece
            if (targetPiece != NO_PIECE && colorOf(targetPiece) == color) {
                continue; // Blocked by own piece
            }
            
            // Check if it's a capture or quiet move
            if (targetPiece != NO_PIECE) {
                // It's a capture (enemy piece)
                moves.add(MoveGen(fromSquare, toSquare, MoveGen::MoveType::NORMAL, NO_PIECE, targetPiece));
            } else {
                // It's a quiet move
                moves.add(MoveGen(fromSquare, toSquare, MoveGen::MoveType::NORMAL));
            }
        }
        
        // Check for castling moves
        generateCastlingMoves(board, moves, color, fromSquare);
    }
}

void generateCastlingMoves(const Board& board, MoveGenList<>& moves, Color color, Square kingSquare) {
    // Determine expected king and rook positions for castling
    const Rank homeRank = (color == WHITE) ? 0 : 7;
    const Square expectedKingSquare = static_cast<Square>(homeRank * 8 + 4); // e1 or e8
    
    // Only generate castling moves if king is on its starting square
    if (kingSquare != expectedKingSquare) {
        return;
    }
    
    // Check kingside castling
    if (board.canCastleKingside(color)) {
        const Square kingTargetSquare = static_cast<Square>(homeRank * 8 + 6);   // g1 or g8
        const Square f_square = static_cast<Square>(homeRank * 8 + 5);           // f1 or f8
        
        // Check if path is clear (f and g squares must be empty)
        if (board.getPiece(f_square) == NO_PIECE && 
            board.getPiece(kingTargetSquare) == NO_PIECE) {
            
            // Add kingside castling move
            moves.add(MoveGen(kingSquare, kingTargetSquare, MoveGen::MoveType::CASTLING));
        }
    }
    
    // Check queenside castling
    if (board.canCastleQueenside(color)) {
        const Square kingTargetSquare = static_cast<Square>(homeRank * 8 + 2);    // c1 or c8
        const Square d_square = static_cast<Square>(homeRank * 8 + 3);            // d1 or d8
        const Square b_square = static_cast<Square>(homeRank * 8 + 1);            // b1 or b8
        
        // Check if path is clear (b, c, and d squares must be empty)
        if (board.getPiece(b_square) == NO_PIECE && 
            board.getPiece(kingTargetSquare) == NO_PIECE && 
            board.getPiece(d_square) == NO_PIECE) {
            
            // Add queenside castling move
            moves.add(MoveGen(kingSquare, kingTargetSquare, MoveGen::MoveType::CASTLING));
        }
    }
}

} // namespace opera