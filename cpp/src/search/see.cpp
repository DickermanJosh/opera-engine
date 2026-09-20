#include "search/see.h"
#include "Board.h"
#include <algorithm>

namespace opera {
int StaticExchangeEvaluator::evaluate(const MoveGen& move) {
    if (!move.isCapture() && !move.isPromotion() && !move.isEnPassant()) return 0;
    const Square from = move.from(), target = move.to();
    const Piece moving = board.getPiece(from);
    if (moving == NO_PIECE || !isValidSquare(target)) return 0;
    const Color us = colorOf(moving);
    if (move.isPromotion() && (move.promotionPiece() == NO_PIECE ||
        colorOf(move.promotionPiece()) != us || typeOf(move.promotionPiece()) < KNIGHT ||
        typeOf(move.promotionPiece()) > QUEEN)) return 0;
    constexpr int values[] = {100, 320, 330, 500, 900, 20000, 0};
    const Square captured = move.isEnPassant() ? target + (us == WHITE ? SOUTH : NORTH) : target;
    const Piece victim = board.getPiece(captured);
    if (typeOf(victim) == KING) return 0;

    Bitboard pieces[12];
    for (int p = 0; p < 12; ++p) pieces[p] = board.getPieceBitboard(static_cast<Piece>(p));
    Bitboard occupied = board.getOccupiedBitboard();
    const Bitboard to_bit = 1ULL << target;
    pieces[moving] &= ~(1ULL << from);
    occupied &= ~(1ULL << from);
    if (victim != NO_PIECE) {
        pieces[victim] &= ~(1ULL << captured);
        occupied &= ~(1ULL << captured);
    }
    Piece on_target = move.isPromotion() ? move.promotionPiece() : moving;
    pieces[on_target] |= to_bit;
    occupied |= to_bit;
    int gain[32];
    gain[0] = values[typeOf(victim)] + values[typeOf(on_target)] - values[typeOf(moving)];

    auto attackers = [&](Square square, Color color, Bitboard occupancy) {
        return (board.getPawnAttacks(square, ~color) & pieces[makePiece(color, PAWN)]) |
               (board.getKnightAttacks(square) & pieces[makePiece(color, KNIGHT)]) |
               (board.getKingAttacks(square) & pieces[makePiece(color, KING)]) |
               (board.getBishopAttacks(square, occupancy) &
                (pieces[makePiece(color, BISHOP)] | pieces[makePiece(color, QUEEN)])) |
               (board.getRookAttacks(square, occupancy) &
                (pieces[makePiece(color, ROOK)] | pieces[makePiece(color, QUEEN)]));
    };

    Color side = ~us;
    int depth = 0;
    while (depth < 31 && typeOf(on_target) != KING) {
        const Bitboard candidates = attackers(target, side, occupied);
        bool found = false;
        Piece next = NO_PIECE;
        int promotion_gain = 0;
        for (int pt = PAWN; pt <= KING && !found; ++pt) {
            const Piece piece = makePiece(side, static_cast<PieceType>(pt));
            Bitboard sources = candidates & pieces[piece];
            while (sources) {
                const Square square = static_cast<Square>(__builtin_ctzll(sources));
                sources &= sources - 1;
                const Bitboard bit = 1ULL << square;
                const bool promotes = pt == PAWN && rankOf(target) == (side == WHITE ? 7 : 0);
                next = promotes ? makePiece(side, QUEEN) : piece;
                pieces[on_target] &= ~to_bit;
                pieces[piece] &= ~bit;
                pieces[next] |= to_bit;
                const Bitboard king = pieces[makePiece(side, KING)];
                const bool legal = !king || !attackers(static_cast<Square>(__builtin_ctzll(king)), ~side, occupied & ~bit);
                if (legal) {
                    occupied &= ~bit;
                    promotion_gain = promotes ? 800 : 0;
                    found = true;
                    break;
                }
                pieces[next] &= ~to_bit;
                pieces[piece] |= bit;
                pieces[on_target] |= to_bit;
            }
        }
        if (!found) break;
        ++depth;
        gain[depth] = values[typeOf(on_target)] + promotion_gain - gain[depth - 1];
        on_target = next;
        side = ~side;
    }
    while (depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
        --depth;
    }
    return gain[0];
}
} // namespace opera
