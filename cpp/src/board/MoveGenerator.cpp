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

} // namespace opera