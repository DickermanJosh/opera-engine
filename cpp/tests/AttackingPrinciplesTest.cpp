#include <gtest/gtest.h>
#include <random>
#include "Board.h"
#include "eval/handcrafted_eval.h"
#include "eval/morphy_eval.h"
#include "search/alphabeta.h"
#include "search/search_engine.h"

using namespace opera;

namespace {
class AttackProbe : public eval::HandcraftedEvaluator {
public:
    using HandcraftedEvaluator::analyze_attacks;
    using HandcraftedEvaluator::evaluate_king_safety;
    using HandcraftedEvaluator::evaluate_mobility;
    using HandcraftedEvaluator::evaluate_threats;
};

TEST(AttackingPrinciplesTest, PinnedKnightCannotSupplyMovesOrMaterialThreats) {
    AttackProbe probe;
    Board pinned("6k1/4r3/8/8/5q2/8/4N3/4K3 w - - 0 1");
    Board free("6k1/5r2/8/8/5q2/8/4N3/4K3 w - - 0 1");
    const auto restricted = probe.analyze_attacks(pinned);
    const auto available = probe.analyze_attacks(free);
    EXPECT_EQ(restricted.piece[E2], 0ULL);
    EXPECT_GT(probe.evaluate_mobility(free, WHITE), probe.evaluate_mobility(pinned, WHITE));
    EXPECT_GT(probe.evaluate_threats(free, WHITE, available), probe.evaluate_threats(pinned, WHITE, restricted));
}

TEST(AttackingPrinciplesTest, PinnedRookCanStillCaptureThePinner) {
    AttackProbe probe;
    Board board("4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1");
    const auto attacks = probe.analyze_attacks(board);
    EXPECT_NE(attacks.piece[E2] & (1ULL << E8), 0ULL);
    EXPECT_NE(attacks.piece[E2] & (1ULL << E3), 0ULL);
    EXPECT_EQ(attacks.piece[E2] & (1ULL << D2), 0ULL);
}

TEST(AttackingPrinciplesTest, DiagonalPinRetainsMovesAlongTheDiagonal) {
    AttackProbe probe;
    Board board("6k1/8/7b/8/8/8/3B4/2K5 w - - 0 1");
    const auto attacks = probe.analyze_attacks(board);
    EXPECT_NE(attacks.piece[D2] & (1ULL << H6), 0ULL);
    EXPECT_NE(attacks.piece[D2] & (1ULL << E3), 0ULL);
    EXPECT_EQ(attacks.piece[D2] & (1ULL << C3), 0ULL);
}

TEST(AttackingPrinciplesTest, TwoBlockersDoNotCreateAnAbsolutePin) {
    AttackProbe probe;
    Board board("4r1k1/8/8/8/8/4B3/4N3/4K3 w - - 0 1");
    const auto attacks = probe.analyze_attacks(board);
    EXPECT_EQ(attacks.pinned[WHITE], 0ULL);
    EXPECT_NE(attacks.piece[E2] & (1ULL << F4), 0ULL);
}

TEST(AttackingPrinciplesTest, PinnedPieceStillDeniesKingFlightUnderChessRules) {
    AttackProbe probe;
    Board board("4r3/8/8/8/6k1/8/4N3/4K3 b - - 0 1");
    const auto attacks = probe.analyze_attacks(board);
    EXPECT_EQ(attacks.piece[E2], 0ULL);
    EXPECT_NE(attacks.king_danger[WHITE] & (1ULL << G3), 0ULL);
    EXPECT_FALSE(board.makeMove(MoveGen(G4, G3)));
}

TEST(AttackingPrinciplesTest, KingCannotEscapeBehindItsVacatedSquareOnCheckingRay) {
    AttackProbe probe;
    Board board("6k1/8/8/8/8/8/8/r5K1 w - - 0 1");
    const auto attacks = probe.analyze_attacks(board);
    EXPECT_NE(attacks.king_danger[BLACK] & (1ULL << H1), 0ULL);
    EXPECT_FALSE(board.makeMove(MoveGen(G1, H1)));
}

TEST(AttackingPrinciplesTest, IntrudingAttackOutweighsTheOtherKingsCentralLocation) {
    AttackProbe probe;
    // Real user game: material ahead, but White's king is trapped by the
    // invading rooks and knight. The old term considered Black less safe.
    Board board("7Q/q3kpp1/P1p5/6p1/4PPP1/1PN1r2n/1PPr4/R4R1K w - - 2 30");
    EXPECT_LT(probe.evaluate_king_safety(board, WHITE, 192),
              probe.evaluate_king_safety(board, BLACK, 192));
}

TEST(AttackingPrinciplesTest, CoordinatedDangerSurvivesAThinMaterialPhase) {
    AttackProbe probe;
    // Sparse rook/bishop coordination against a cornered king. Moving the
    // bishop off its useful diagonal reduces danger despite equal material.
    Board net("7k/8/5KB1/8/8/8/8/R7 b - - 0 1");
    Board loose("7k/8/5K2/8/8/8/1B6/R7 b - - 0 1");
    EXPECT_LT(probe.evaluate_king_safety(net, BLACK, 32),
              probe.evaluate_king_safety(loose, BLACK, 32));
}

TEST(AttackingPrinciplesTest, PotentialChecksWithPlentyOfFlightsFadeInTheEndgame) {
    AttackProbe probe;
    // Rook/bishop checking access, but the central king has six unattacked
    // flights. Potential pressure should not become a phase-independent net.
    Board board("8/8/8/4k3/8/8/6B1/R5K1 b - - 0 1");
    EXPECT_NEAR(probe.evaluate_king_safety(board, BLACK, 128),
                probe.evaluate_king_safety(board, BLACK, 256) / 2, 1);
}

TEST(AttackingPrinciplesTest, ColorReflectionPreservesAllAttackTerms) {
    const std::pair<const char*, const char*> positions[] = {
        {"3rk2r/4qppn/p1p3p1/nP6/4P1P1/1BN2Q1P/PPP2P2/R4RK1 w k - 3 19",
         "r4rk1/ppp2p2/1bn2q1p/4p1p1/Np6/P1P3P1/4QPPN/3RK2R b K - 3 19"},
        {"7Q/q3kpp1/P1p5/6p1/4PPP1/1PN1r2n/1PPr4/R4R1K w - - 2 30",
         "r4r1k/1ppR4/1pn1R2N/4ppp1/6P1/p1P5/Q3KPP1/7q b - - 2 30"},
        {"r1b2b1r/ppkn4/2p2q2/3BppQp/6p1/1PN5/PBPP1PPP/R3R1K1 w - - 2 20",
         "r3r1k1/pbpp1ppp/1pn5/6P1/3bPPqP/2P2Q2/PPKN4/R1B2B1R b - - 2 20"},
    };
    eval::HandcraftedEvaluator normal;
    eval::MorphyEvaluator morphy;
    for (const auto& pair : positions) {
        Board white(pair.first), black(pair.second);
        EXPECT_EQ(normal.evaluate(white, WHITE), -normal.evaluate(black, BLACK));
        EXPECT_EQ(morphy.evaluate(white, WHITE), -morphy.evaluate(black, BLACK));
    }
}

TEST(AttackingPrinciplesTest, CheckPredictionMatchesPlayingLegalMovesAndPreservesState) {
    std::mt19937 random(20260921);
    for (const char* fen : {STARTING_FEN,
         "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
         "4k3/P7/8/3pP3/8/8/7p/4K3 w - d6 0 1",
         "5k2/8/8/8/8/8/8/4K2R w K - 0 1",
         "7k/8/8/3pP3/8/8/8/K2R4 w - d6 0 1"}) {
        Board board(fen);
        for (int ply = 0; ply < 60; ++ply) {
            MoveGenList<> moves;
            generateAllLegalMoves(board, moves, board.getSideToMove());
            if (moves.empty()) break;
            const auto original = board.toFEN();
            const auto key = board.getZobristKey();
            for (const auto& move : moves) {
                const bool predicted = board.givesCheck(move);
                EXPECT_EQ(board.toFEN(), original);
                EXPECT_EQ(board.getZobristKey(), key);
                ASSERT_TRUE(board.makeGeneratedMove(move));
                EXPECT_EQ(predicted, board.isInCheck(board.getSideToMove())) << original;
                board.unmakeMove(move);
            }
            ASSERT_TRUE(board.makeGeneratedMove(moves[random() % moves.size()]));
        }
    }
}

TEST(AttackingPrinciplesTest, QuiescenceFindsQuietMateAndRestoresBoard) {
    Board board("7k/5K2/6Q1/8/8/8/8/8 w - - 0 1");
    const auto original = board.toFEN();
    const auto key = board.getZobristKey();
    std::atomic<bool> stop{false};
    TranspositionTable tt(1);
    MoveOrdering ordering(board, tt);
    StaticExchangeEvaluator see(board);
    AlphaBetaSearch search(board, stop, tt, ordering, see);
    EXPECT_LT(search.quiescence(0, -INFINITY_SCORE, INFINITY_SCORE, 0), CHECKMATE_SCORE - MAX_PLY);
    EXPECT_EQ(search.search(0), CHECKMATE_SCORE - 1);
    EXPECT_EQ(board.toFEN(), original);
    EXPECT_EQ(board.getZobristKey(), key);
    ASSERT_FALSE(search.get_principal_variation().empty());
    EXPECT_EQ(board.getPiece(search.get_principal_variation().front().to()), NO_PIECE);
}

TEST(AttackingPrinciplesTest, QuietPreparationFindsTheShorterMate) {
    // Earlier user game: a3! enables mate after every one of Black's 16 legal
    // replies. Checking immediately with Qd5+ takes longer. Also reflect colors.
    for (const auto& position : {
        std::pair<const char*, const char*>{
            "N1b2bnr/1p4pp/p2p4/2k1Np2/2P5/8/PP3PPP/R2QKB1R w KQ - 2 15", "a2a3"},
        {"r2qkb1r/pp3ppp/8/2p5/2K1nP2/P2P4/1P4PP/n1B2BNR b kq - 2 15", "a7a6"}}) {
        Board board(position.first);
        std::atomic<bool> stop{false};
        SearchEngine engine(board, stop);
        engine.set_use_morphy_style(true);
        SearchLimits limits;
        limits.max_depth = 3;
        const auto result = engine.search(limits);
        EXPECT_EQ(result.best_move.toString(), position.second);
        EXPECT_EQ(result.score, CHECKMATE_SCORE - 3);
    }
}
} // namespace
