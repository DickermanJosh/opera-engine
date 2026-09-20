#include <gtest/gtest.h>
#include <random>
#include "Board.h"
#include "MoveGen.h"
#include "eval/handcrafted_eval.h"
#include "eval/morphy_eval.h"
#include "search/alphabeta.h"

using namespace opera;

namespace {
class EvalProbe : public eval::HandcraftedEvaluator {
public:
    using HandcraftedEvaluator::evaluate_pst;
    using HandcraftedEvaluator::evaluate_king_safety;
    using HandcraftedEvaluator::evaluate_mobility;
};

class CoreSearchTest : public testing::Test {
protected:
    Board board;
    std::atomic<bool> stop{false};
    TranspositionTable tt{1};
    MoveOrdering ordering{board, tt};
    StaticExchangeEvaluator see{board};
    AlphaBetaSearch search{board, stop, tt, ordering, see};
};

TEST(CoreBoardTest, FiftyMovesMeansOneHundredPlies) {
    Board board("4k3/8/8/8/8/8/8/R3K3 w - - 99 1");
    EXPECT_FALSE(board.isFiftyMoveRule());
    ASSERT_TRUE(board.makeMove(MoveGen(A1, A2)));
    EXPECT_TRUE(board.isFiftyMoveRule());
}

TEST(CoreBoardTest, NullMoveClearsEnPassantAndRestoresEveryStateField) {
    Board board("r3k2r/8/8/3pP3/8/8/8/R3K2R w KQkq d6 73 40");
    const auto fen = board.toFEN();
    const auto key = board.getZobristKey();
    board.makeNullMove();
    EXPECT_EQ(board.getSideToMove(), BLACK);
    EXPECT_EQ(board.getEnPassantSquare(), NO_SQUARE);
    EXPECT_FALSE(board.isThreefoldRepetition());
    EXPECT_EQ(board.getZobristKey(), Board(board.toFEN()).getZobristKey());
    board.unmakeNullMove();
    EXPECT_EQ(board.toFEN(), fen);
    EXPECT_EQ(board.getZobristKey(), key);
}

TEST(CoreBoardTest, RepetitionIgnoresUnusableEnPassantRights) {
    for (const auto* fen : {"4k3/8/8/4p3/8/8/8/4K3 w - e6 0 1",
                           "k3r3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"}) {
        Board board(fen);
        std::string normalized = board.toFEN();
        const auto ep = normalized.find(" w - ") + 5;
        normalized.replace(ep, 2, "-");
        EXPECT_EQ(board.getZobristKey(), Board(normalized).getZobristKey());
    }
}

TEST(CoreBoardTest, InvalidMovesLeaveBoardUntouched) {
    Board board;
    const auto fen = board.toFEN();
    for (MoveGen move : {MoveGen(E2, E5), MoveGen(E2, F3), MoveGen(A1, A4),
                         MoveGen(E1, G1, MoveGen::MoveType::CASTLING), MoveGen(E7, E6)}) {
        EXPECT_FALSE(board.makeMove(move));
        EXPECT_EQ(board.toFEN(), fen);
    }
}

TEST(CoreBoardTest, HashAndUndoMatchFreshFenThroughoutLegalGames) {
    std::mt19937 rng(20260919);
    for (const auto* fen : {STARTING_FEN,
         "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
         "4k3/P7/8/3pP3/8/8/7p/4K3 w - d6 0 1"}) {
        Board board(fen);
        for (int ply = 0; ply < 120; ++ply) {
            MoveGenList<> moves;
            generateAllLegalMoves(board, moves, board.getSideToMove());
            if (moves.empty()) break;
            const auto before = board.toFEN();
            const auto key = board.getZobristKey();
            for (auto move : moves) {
                ASSERT_TRUE(board.makeMove(move));
                EXPECT_EQ(board.getZobristKey(), Board(board.toFEN()).getZobristKey());
                board.unmakeMove(move);
                EXPECT_EQ(board.toFEN(), before);
                EXPECT_EQ(board.getZobristKey(), key);
            }
            ASSERT_TRUE(board.makeMove(moves[rng() % moves.size()]));
        }
    }
}

TEST(CoreTableTest, EmptyAndLowKeysDoNotAlias) {
    TranspositionTable table(1);
    TTEntry entry;
    EXPECT_FALSE(table.probe(0, entry));
    EXPECT_FALSE(table.probe(123, entry));
    table.store(0, Move(E2, E4), 20, 3, TTEntryType::EXACT);
    EXPECT_TRUE(table.probe(0, entry));
    EXPECT_FALSE(table.probe(16384, entry)); // Same cluster, different full key.
}

TEST(CoreTableTest, PreservesSpecialMoveAndPromotionIdentity) {
    for (Move move : {Move(A7, A8, PROMOTION, KNIGHT), Move(E1, G1, CASTLING),
                      Move(E5, D6, EN_PASSANT)}) {
        TTEntry entry;
        entry.set_data(123, move, 10, 4, TTEntryType::EXACT, 0);
        EXPECT_EQ(entry.get_move(), move);
    }
}

TEST(CoreTableTest, ReplacesShallowAndOldEntriesFirst) {
    TranspositionTable table(1);
    // 16384 clusters in 1 MiB; all these keys share cluster zero.
    for (int i = 1; i <= 4; ++i)
        table.store(i * 16384ULL, Move(E2, E4), 0, i * 4, TTEntryType::EXACT);
    table.store(5 * 16384ULL, Move(D2, D4), 0, 8, TTEntryType::EXACT);
    EXPECT_FALSE(table.contains(16384));
    EXPECT_TRUE(table.contains(4 * 16384ULL));
    for (int i = 0; i < 8; ++i) table.new_search();
    table.store(4 * 16384ULL, Move(E2, E4), 0, 16, TTEntryType::EXACT);
    table.store(6 * 16384ULL, Move(D2, D4), 0, 6, TTEntryType::EXACT);
    EXPECT_TRUE(table.contains(4 * 16384ULL));
}

TEST_F(CoreSearchTest, HorizonStalemateIsDrawEvenWithWinningStandPat) {
    board.setFromFEN("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    EXPECT_EQ(search.quiescence(0, -32000, 32000), 0);
    EXPECT_EQ(search.quiescence(0, -1000, -950), 0);
}

TEST_F(CoreSearchTest, QuiescenceIncludesQuietPromotions) {
    board.setFromFEN("7k/P7/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_GE(search.search(0), 900);
}

TEST_F(CoreSearchTest, MateTakesPrecedenceOverFiftyMoveDraw) {
    board.setFromFEN("7k/6Q1/6K1/8/8/8/8/8 b - - 100 1");
    EXPECT_EQ(search.quiescence(3, -32000, 32000), -CHECKMATE_SCORE + 3);
    EXPECT_EQ(search.pvs(2, 3, -32000, 32000, true), -CHECKMATE_SCORE + 3);
}

TEST_F(CoreSearchTest, MateDistanceSurvivesTranspositionAtDifferentPly) {
    board.setFromFEN("k7/8/1K6/8/8/8/8/7R w - - 0 1");
    const int far = search.pvs(2, 5, -32000, 32000, false);
    ASSERT_GT(far, CHECKMATE_SCORE - MAX_PLY);
    const int near = search.pvs(2, 1, -32000, 32000, false);
    EXPECT_EQ(near, far + 4);
}

TEST_F(CoreSearchTest, QuietCutoffsTrainTheOrderingActuallyUsed) {
    search.search(3);
    int learned = 0;
    for (int from = 0; from < 64; ++from)
        for (int to = 0; to < 64; ++to)
            learned += ordering.get_history_score(MoveGen(from, to), WHITE) +
                       ordering.get_history_score(MoveGen(from, to), BLACK);
    EXPECT_GT(learned, 0);
}

TEST_F(CoreSearchTest, NullPruningIsUsedButNotInPawnEndings) {
    EXPECT_GE(search.pvs(4, 1, -501, -500, false), -500);
    EXPECT_GT(search.get_stats().null_move_cutoffs, 0);
    EXPECT_EQ(board.toFEN(), STARTING_FEN);
    board.setFromFEN("8/8/8/8/8/3k4/3P4/3K4 w - - 0 1");
    search.reset();
    search.pvs(4, 1, -501, -500, false);
    EXPECT_EQ(search.get_stats().null_move_cutoffs, 0);
}

TEST_F(CoreSearchTest, FindsOperaGameQueenSacrifice) {
    board.setFromFEN("4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 0 16");
    eval::MorphyEvaluator evaluator;
    search.set_evaluator(&evaluator);
    EXPECT_GE(search.search(4), CHECKMATE_SCORE - 3);
    ASSERT_FALSE(search.get_principal_variation().empty());
    EXPECT_EQ(search.get_principal_variation().front().toString(), "b3b8");
}

TEST_F(CoreSearchTest, PrefersShelteredCastlingToAnUnforcedKingWalk) {
    board.setFromFEN("r2q1rk1/ppp2ppp/2nbbn2/3pp3/3PP3/2NBBN2/PPP2PPP/R2QK2R w KQ - 0 8");
    search.root_moves = {"e1g1", "e1e2", "e1f1", "e1d2"};
    eval::HandcraftedEvaluator normal;
    eval::MorphyEvaluator morphy;
    for (eval::Evaluator* evaluator : {static_cast<eval::Evaluator*>(&normal), static_cast<eval::Evaluator*>(&morphy)}) {
        tt.clear();
        search.set_evaluator(evaluator);
        search.search(4);
        ASSERT_FALSE(search.get_principal_variation().empty());
        EXPECT_EQ(search.get_principal_variation().front().toString(), "e1g1");
    }
}

TEST(CoreEvaluationTest, CentralPawnAdvancesAndPromotionProgressHaveValue) {
    EvalProbe eval;
    Board home("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
    Board centre("4k3/8/8/8/4P3/8/8/4K3 w - - 0 1");
    Board advanced("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    Board pawn_home("4k3/8/8/8/8/8/P7/4K3 w - - 0 1");
    EXPECT_GT(eval.evaluate_pst(centre, WHITE, 256), eval.evaluate_pst(home, WHITE, 256));
    EXPECT_GT(eval.evaluate_pst(advanced, WHITE, 0), eval.evaluate_pst(pawn_home, WHITE, 0));
}

TEST(CoreEvaluationTest, PawnsBehindKingDoNotProvideShelter) {
    EvalProbe eval;
    Board sheltered("r2q1rk1/ppp2ppp/2nbbn2/8/8/2NBBN2/PPP2PPP/R2Q1RK1 w - - 0 1");
    Board exposed("r2q1rk1/ppp2ppp/2nbbn2/8/8/2NBBNK1/PPP2PPP/R2Q1R2 w - - 0 1");
    EXPECT_GT(eval.evaluate_king_safety(sheltered, WHITE, 256),
              eval.evaluate_king_safety(exposed, WHITE, 256));
    EXPECT_EQ(eval.evaluate_king_safety(exposed, WHITE, 0), 0);
}

int styleBonus(const char* fen) {
    Board board(fen);
    eval::HandcraftedEvaluator normal;
    eval::MorphyEvaluator morphy;
    return morphy.evaluate(board, board.getSideToMove()) - normal.evaluate(board, board.getSideToMove());
}

TEST(CoreDevelopmentTest, RewardsAnotherActivePieceOverAnotherKnightMove) {
    // 1.e4 e5 2.Nf3 Nc6 followed by Bc4 or Ng5. Material is identical;
    // Morphy's extra preference must strongly favour involving the bishop.
    const char* army = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3";
    const char* solo = "r1bqkbnr/pppp1ppp/2n5/4p1N1/4P3/8/PPPP1PPP/RNBQKB1R b KQkq - 3 3";
    EXPECT_GT(styleBonus(army) - styleBonus(solo), 60);
}

TEST(CoreDevelopmentTest, OpeningBishopDiagonalsMattersBeforeBishopMoves) {
    const char* open = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1";
    EXPECT_GT(styleBonus(open) - styleBonus(STARTING_FEN), 70);
}

TEST(CoreDevelopmentTest, EarlyKnightCanBeChasedByCentralPawn) {
    const char* ready_to_kick = "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2";
    const char* not_yet = "rnbqkb1r/pppppppp/5n2/8/8/4P3/PPPP1PPP/RNBQKBNR w KQkq - 1 2";
    EXPECT_GT(styleBonus(ready_to_kick) - styleBonus(not_yet), 20);
}

TEST(CoreDevelopmentTest, ContestsTheCentreRatherThanOnlyOpeningADiagonal) {
    const char* centre = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1";
    const char* diagonal_only = "rnbqkbnr/pppppppp/8/8/8/4P3/PPPP1PPP/RNBQKBNR b KQkq - 0 1";
    EXPECT_GT(styleBonus(centre) - styleBonus(diagonal_only), 15);
}

TEST(CoreDevelopmentTest, StyleIsColorSymmetricAndIndependentOfMoveNumber) {
    const char* white = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3";
    const char* black = "rnbqk2r/pppp1ppp/5n2/2b1p3/4P3/2N5/PPPP1PPP/R1BQKBNR w KQkq - 3 3";
    EXPECT_EQ(styleBonus(white), -styleBonus(black));
    EXPECT_EQ(styleBonus(white), styleBonus("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 40"));
}

TEST(CoreDevelopmentTest, BiasZeroStillUsesExactNormalEvaluation) {
    Board board("rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2");
    eval::HandcraftedEvaluator normal;
    eval::MorphyEvaluator disabled(0);
    EXPECT_EQ(disabled.evaluate(board, WHITE), normal.evaluate(board, WHITE));
}

TEST_F(CoreSearchTest, DevelopmentDoesNotOverrideWinningAFreeQueen) {
    board.setFromFEN("rnb1kbnr/ppp2ppp/8/3pp3/3q4/4P3/PPPP1PPP/RNBQKBNR w KQkq - 0 4");
    eval::MorphyEvaluator evaluator;
    search.set_evaluator(&evaluator);
    search.search(4);
    ASSERT_FALSE(search.get_principal_variation().empty());
    EXPECT_EQ(search.get_principal_variation().front().toString(), "e3d4");
}

TEST_F(CoreSearchTest, OpensLinesForBishopsInsteadOfDevelopingOnlyKnights) {
    // After 1.e4 Nc6 2.d4, don't bring out the other knight and invite d5/e5
    // while both bishops are still locked behind their starting pawns.
    board.setFromFEN("r1bqkbnr/pppppppp/2n5/8/3PP3/8/PPP2PPP/RNBQKBNR b KQkq - 0 2");
    eval::MorphyEvaluator evaluator;
    search.set_evaluator(&evaluator);
    search.search(5);
    ASSERT_FALSE(search.get_principal_variation().empty());
    const auto move = search.get_principal_variation().front().toString();
    EXPECT_TRUE(move == "e7e5" || move == "e7e6" || move == "d7d5" || move == "d7d6") << move;
}

TEST(CoreExchangeTest, SideMayDeclineALosingRecapture) {
    Board board("6k1/8/3q4/3p4/4P3/8/8/3R2K1 w - - 0 1");
    StaticExchangeEvaluator see(board);
    // exd5 wins a pawn: Qxd5? Rxd5 would lose the queen, so Black declines.
    EXPECT_EQ(see.evaluate(MoveGen(E4, D5, MoveGen::MoveType::NORMAL, NO_PIECE, BLACK_PAWN)), 100);
}

TEST(CoreExchangeTest, RevealsSlidingAttackersAndIgnoresPinnedDefender) {
    Board board("3r2k1/3r4/8/3p4/4P3/8/8/3R2K1 w - - 0 1");
    StaticExchangeEvaluator see(board);
    // exd5 Rxd5 Rxd5 Rxd5: the second black rook appears through the first.
    EXPECT_EQ(see.evaluate(MoveGen(E4, D5, MoveGen::MoveType::NORMAL, NO_PIECE, BLACK_PAWN)), 0);
    board.setFromFEN("4k3/4n3/8/3p4/4P3/8/8/4R1K1 w - - 0 1");
    EXPECT_EQ(see.evaluate(MoveGen(E4, D5, MoveGen::MoveType::NORMAL, NO_PIECE, BLACK_PAWN)), 100);
}
} // namespace
