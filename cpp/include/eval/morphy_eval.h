/**
 * @file morphy_eval.h
 * @brief Morphy-style chess evaluator with aggressive, sacrificial playing style
 *
 * Implements Paul Morphy's characteristic playing style through bias multipliers:
 * - Rapid development emphasis (1.2x weight in opening)
 * - Aggressive king attacks (1.5x king safety weight)
 * - Initiative and tempo prioritization (1.1x mobility weight)
 * - Material sacrifice compensation (up to 100cp for initiative)
 * - Uncastled king exploitation (50cp penalty)
 *
 * The evaluator extends HandcraftedEvaluator with configurable bias multipliers,
 * allowing runtime adjustment of Morphy style intensity (0.0 = normal, 1.0 = standard, 2.0 = extreme).
 */

#ifndef OPERA_MORPHY_EVAL_H
#define OPERA_MORPHY_EVAL_H

#include "eval/handcrafted_eval.h"
#include "Board.h"
#include "Types.h"
#include <map>
#include <string>

namespace opera {
namespace eval {

/**
 * @brief Morphy-style evaluator with aggressive playing characteristics
 *
 * Extends HandcraftedEvaluator by applying bias multipliers to:
 * - Development bonuses (1.2x in opening)
 * - King safety evaluation (1.5x for attacks)
 * - Mobility and initiative (1.1x for active pieces)
 * - Material sacrifice compensation (up to 100cp)
 *
 * The bias can be configured via UCI options (MorphyBias: 0.0-2.0)
 */
class MorphyEvaluator : public HandcraftedEvaluator {
public:
    /**
     * @brief Construct Morphy evaluator with specified bias multiplier
     *
     * @param morphy_bias Bias multiplier (0.0 = normal, 1.0 = standard Morphy, 2.0 = extreme)
     */
    explicit MorphyEvaluator(double morphy_bias = 1.0);

    /**
     * @brief Evaluate position with Morphy-style biases
     *
     * Applies the base HandcraftedEvaluator evaluation, then adjusts scores based on:
     * - Development advantage (biased in opening)
     * - King safety attacks (aggressive penalties for exposed kings)
     * - Piece activity and initiative (tempo bonuses)
     * - Material sacrifice compensation (initiative-based)
     *
     * @param board The board position to evaluate
     * @param side_to_move The side to evaluate from
     * @return Evaluation score in centipawns (positive = white advantage)
     */
    int evaluate(const Board& board, Color side_to_move) override;

    /**
     * @brief Configure evaluator options via UCI-style parameters
     *
     * Supported options:
     * - MorphyBias: 0.0-2.0 (default 1.0)
     *
     * @param options Map of option names to values
     */
    void configure_options(const std::map<std::string, std::string>& options) override;

    /**
     * @brief Get current Morphy bias multiplier
     *
     * @return Current bias value (0.0-2.0)
     */
    double get_morphy_bias() const { return morphy_bias_; }

private:
    /**
     * @brief Calculate material sacrifice compensation based on initiative
     *
     * Evaluates if a material deficit is compensated by:
     * - Piece activity and mobility
     * - King attack potential
     * - Development advantage
     * - Tempo and initiative
     *
     * @param board The board position
     * @param color The side to evaluate
     * @param material_deficit Material disadvantage in centipawns (negative)
     * @return Compensation bonus (0-100cp)
     */
    int calculate_sacrifice_compensation(const Board& board, Color color, int material_deficit) const;

    /**
     * @brief Detect if enemy king is uncastled in opening/middlegame
     *
     * @param board The board position
     * @param enemy_color Enemy color
     * @param phase Game phase (0-256)
     * @return true if enemy king hasn't castled and phase > 128
     */
    bool is_uncastled_in_opening(const Board& board, Color enemy_color, int phase) const;

    /**
     * @brief Calculate initiative bonus based on piece activity
     *
     * Measures:
     * - Centralized pieces
     * - Piece mobility
     * - Control of key squares
     * - Development advantage
     *
     * @param board The board position
     * @param color The side to evaluate
     * @return Initiative score (0-100cp)
     */
    int calculate_initiative(const Board& board, Color color) const;

    // Morphy-specific bias multipliers (scaled by morphy_bias_)
    static constexpr double DEVELOPMENT_BIAS = 1.2;    ///< Development weight multiplier
    static constexpr double KING_SAFETY_BIAS = 1.5;    ///< King attack weight multiplier
    static constexpr double MOBILITY_BIAS = 1.1;       ///< Mobility/initiative multiplier
    static constexpr int SACRIFICE_COMPENSATION = 100; ///< Max compensation for sacrifices (cp)
    static constexpr int UNCASTLED_PENALTY = 50;       ///< Penalty for uncastled enemy king (cp)

    double morphy_bias_;  ///< Overall bias multiplier (0.0-2.0)
};

} // namespace eval
} // namespace opera

#endif // OPERA_MORPHY_EVAL_H
