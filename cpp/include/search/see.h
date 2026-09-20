#pragma once
#include "MoveGen.h"

namespace opera {
class Board;

// Material-only exchange on one square. Either side may decline a recapture;
// this is an ordering/pruning estimate, not a substitute for tactical search.
class StaticExchangeEvaluator {
public:
    explicit StaticExchangeEvaluator(const Board& board) : board(board) {}
    int evaluate(const MoveGen& move);
    int quick_evaluate(const MoveGen& move) { return evaluate(move); }
    bool is_good_capture(const MoveGen& move, int threshold = 0) {
        return evaluate(move) >= threshold;
    }
private:
    const Board& board;
};
} // namespace opera
