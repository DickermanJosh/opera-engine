# Perft Bug Analysis & Fixes

**Date**: 2025-11-29
**Issue**: 3 perft tests failing with small node count discrepancies
**Root Cause**: Square value inconsistencies and potential move generation edge cases

---

## Bug #1: Square Null Value Mismatch 🔴 CRITICAL

### Problem

**Discovered**: "h5xx" move appearing in perft output for Test 6

**Root Cause**: Inconsistent null square values across codebase:

```cpp
// In Types.h
enum Square {
    A1 = 0, B1, C1, /* ... */ H8 = 63,
    NO_SQUARE = 64  // ← Board uses this
};

// In MoveGen.h
class MoveGen {
    static constexpr Square NULL_SQUARE_VALUE = 63;  // ← H8 repurposed as null!
    //                                                      This conflicts with actual H8!
};
```

**Impact**:
- When `enPassantSquare == NO_SQUARE` (64), it doesn't match `NULL_SQUARE_VALUE` (63)
- Moves with `to == H8` (63) are printed as "xx" because 63 is treated as null
- This causes move generation inconsistencies

**Example from Test 6**:
```
FEN: 3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1
Move generated: h5xx  ← Rook on h5, "to" square showing as null!
```

### Fix

**Option A**: Change `NULL_SQUARE_VALUE` to 64 (RECOMMENDED)
```cpp
// In MoveGen.h
static constexpr Square NULL_SQUARE_VALUE = 64;  // Match NO_SQUARE from Types.h
```

**Option B**: Use 65 as universal null value
```cpp
// In both files
constexpr Square NO_SQUARE = 65;  // Beyond valid squares
```

**Recommendation**: Option A - minimal changes, aligns with existing Types.h

---

## Bug #2: En Passant Validation Edge Cases 🟡 MEDIUM

### Test 6: Illegal EP Move #1

```
FEN: 3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1
Expected: 186,770 nodes @ depth 5
Actual:   185,429 nodes @ depth 5
Delta:    -1,341 nodes (-0.72%)
```

**Analysis**:
- FEN shows `- -` (no en passant square)
- Test name: "Illegal EP Move #1"
- Purpose: Verify en passant is NOT allowed when it shouldn't be

**Hypothesis**: Engine is correctly REJECTING an invalid en passant, but test expects it to be generated and then rejected during legality check.

**Investigation Needed**:
1. Check if move generation should include pseudo-legal EP moves
2. Verify EP moves are properly filtered by `makeMove()` legality check
3. Compare with reference implementations (Stockfish, Ethereal)

### Test 7: Illegal EP Move #2

```
FEN: 8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1
Expected: 135,530 nodes @ depth 5
Actual:   135,655 nodes @ depth 5
Delta:    +125 nodes (+0.09%)
```

**Analysis**:
- POSITIVE delta (generating MORE moves)
- Suggests allowing an illegal move
- FEN shows `- -` (no en passant)

**Hypothesis**: False positive - generating a move that should be illegal

**Possible Causes**:
1. Double pawn push creating invalid EP square
2. En passant allowed without proper validation
3. Bishop/king interaction creating illegal position

---

## Bug #3: Endgame Move Generation 🟡 MEDIUM

### Test 3: Endgame Position

```
FEN: 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1
Expected: 681,673 nodes @ depth 5
Actual:   674,624 nodes @ depth 5
Delta:    -7,049 nodes (-1.03%)
```

**Perft Divide Results**:
```
Depth 1: 14 moves (correct)
Depth 2: 191 nodes
Depth 3: 2,812 nodes
```

**Analysis**:
- 1% discrepancy suggests systematic issue, not single bug
- Endgame-specific (kings, rooks, pawns only)
- No castling, no en passant in FEN

**Possible Causes**:
1. **Pawn promotion edge case**: Pawns on 2nd/7th rank
2. **King move generation**: Near board edges
3. **Rook move generation**: Blocking/pinning scenarios
4. **Move legality check**: Leaving king in check

**Investigation Steps**:
1. Run `perft_debug 0 4` for depth 4 breakdown
2. Compare move lists with Stockfish at each depth
3. Check promotion handling (e2e3, e2e4, g2g3, g2g4)
4. Verify rook captures (b4f4 shows only 41 nodes at depth 3)

---

## Debugging Methodology

### Tools Created

1. **perft_debug_simple** - Move-by-move analysis
   ```bash
   cd cpp/build
   ./perft_debug 0 3  # Test 3, depth 3
   ./perft_debug 1 3  # Test 6, depth 3
   ./perft_debug 2 3  # Test 7, depth 3
   ```

2. **Manual Verification**
   ```bash
   # Compare with Stockfish
   stockfish
   position fen 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1
   go perft 5
   ```

### Systematic Approach

For each failing test:

1. **Depth 1**: Verify move list is correct
   - Check against hand-calculated legal moves
   - Ensure all piece types move correctly

2. **Depth 2**: Check first-level responses
   - For each root move, verify response count
   - Identify which root moves have wrong counts

3. **Depth 3**: Narrow down specific sequences
   - Find exact move sequence causing discrepancy
   - Isolate position where bug occurs

4. **Root Cause**: Fix the underlying issue
   - Update move generation logic
   - Add test case for the specific scenario
   - Verify fix doesn't break other tests

---

## Implementation Plan

### Phase 1: Fix Square Value Mismatch (30 min)

1. Update `NULL_SQUARE_VALUE` in MoveGen.h to 64
2. Update bit packing to support 7-bit squares (0-64 range)
3. Test that "h5xx" bug disappears
4. Verify no regressions in existing tests

### Phase 2: En Passant Investigation (1-2 hours)

1. Create minimal test cases for EP edge cases
2. Compare with Chess Programming Wiki rules:
   - EP only valid immediately after double push
   - EP capture must not leave king in check
   - EP must be on correct rank
3. Review `generatePawnMoves()` EP logic (lines 80-88)
4. Add EP-specific unit tests

### Phase 3: Endgame Investigation (1-2 hours)

1. Run perft divide at increasing depths
2. Compare with Stockfish perft output
3. Identify specific move causing divergence
4. Fix root cause (likely promotion or king move)
5. Add regression test

### Phase 4: Verification (30 min)

1. Run full perft suite
2. Verify all 19 tests pass
3. Run full test suite (312 tests)
4. Update documentation

---

## Expected Outcomes

After fixes:
- ✅ All 19 perft tests passing
- ✅ No "xx" notation in move output
- ✅ Correct en passant validation
- ✅ Accurate endgame move generation

---

## Reference Materials

### Chess Programming Wiki
- [Perft Results](https://www.chessprogramming.org/Perft_Results)
- [En Passant](https://www.chessprogramming.org/En_passant)
- [Move Generation](https://www.chessprogramming.org/Move_Generation)

### Test Positions Source
- Martin Sedlak's PERFT test suite (Tests 6-7)
- Standard perft positions (Test 3)

### Stockfish Comparison
```bash
# Install Stockfish for verification
sudo apt-get install stockfish

# Run perft comparison
stockfish << EOF
position fen <fen_string>
go perft <depth>
EOF
```

---

**Next Action**: Implement Phase 1 - Fix square value mismatch

