# Play Opera in the Unity chess app

The existing Unity project is `../chess`, using Unity 6000.0.54f1 on `opera-integration`. Current engine development is on `engine-core`; `uci` is already merged and pushed to `main` as `bde03c3`.

Open the chess app and choose **Play Opera**. Click a piece, then a highlighted destination. The game starts with you as White; **New game · Black** flips the board and lets Opera open. Thinking-time buttons select 0.25, 1 or 3 seconds for subsequent engine moves. Promotion offers queen, rook, bishop and knight. The panel reports check, checkmate, draws, resignation and engine errors. New game, Main menu and closing the app stop the owned engine process.

The app selects the improved handcrafted **Morphy style**, one thread and 16 MB hash, with no pondering or match clock. The general UCI default remains the standard handcrafted evaluator. See [core changes and measured results](core-engine-review.md); neural development and tuning come later.

The Mac build was played through the visible UI as both colors. Legal-move highlighting, replies, board flipping, time selection, restarting during search, resignation and leaving/re-entering the game passed. Rule/client checks also pass independently of the rendered board. Promotion choices and terminal rules are covered by rule fixtures; a complete manual GUI game and a GUI promotion sequence have not yet been played in this session.

## Install or refresh the engine

From this repository, build a native release executable, then copy it into the Unity project:

```sh
cargo build --manifest-path rust/Cargo.toml --release --locked --bin opera-uci
python3 scripts/install_unity_engine.py --unity-project ../chess
```

On Windows, use your Python launcher if `python3` is unavailable. The installer chooses the host platform/architecture by default. `--binary`, `--platform` and `--arch` support a separately built native binary; pass its actual target, not the packaging machine's target. The installer copies it, sets Unix executable permissions and writes a SHA-256/checkout manifest. It does not compile or convert architectures. Generated native packages are ignored in the Unity repository and need reinstalling after a fresh checkout.

The app resolves `Assets/StreamingAssets/Opera/<platform>-<architecture>/opera-uci`, with `.exe` on Windows. Platform names are `macOS`, `Windows`, `Linux`; architectures are `arm64` and `x86_64`. An explicit `OPERA_ENGINE_PATH` environment variable overrides the packaged executable for development. Every target needs its own matching native engine. Windows/Linux app builds and Intel Mac builds have not been validated in this integration session.

## Validate and build locally

In the Unity editor, **Opera → Validate rules and engine** runs independent move/FEN fixtures, perft, draw checks and the real UCI client's move/cancellation/restart checks. **Opera → Build macOS ARM64 playtest** creates `../chess/build/Opera Desktop/Opera Chess.app` as a windowed desktop player. **Opera → Prepare AI scene** can regenerate the AI scene from the existing OnlineGame board/sprite setup; it overwrites the AI scene, so use it only when intentionally regenerating that scene.

Equivalent command-line checks on this Mac:

```sh
UNITY='/Applications/Unity/Hub/Editor/6000.0.54f1/Unity.app/Contents/MacOS/Unity'
"$UNITY" -batchmode -nographics -quit -projectPath "$PWD/../chess" \
  -executeMethod OperaIntegrationTools.Validate -logFile /tmp/opera-unity-validation.log
"$UNITY" -batchmode -nographics -quit -projectPath "$PWD/../chess" \
  -executeMethod OperaIntegrationTools.BuildMac -logFile /tmp/opera-unity-build.log
```

The fixture file is in the Unity project at `Assets/Editor/Opera/RulesPositions.tsv`: 494 reference rows generated with python-chess/chess 1.11.2, including the two saved Stockfish games and deterministic random seed 20260918. Board queries must be read-only. Castling rights, en passant removal and pins, pawn attacks, promotions, halfmove/fullmove FEN counters and repetition keys are checked independently of Opera's move selection.

This is a local development app, not a signed/notarized distribution release. Future work includes cross-platform app validation, match clocks, move-list/PGN export and feedback from manual play before improving core search/evaluation.
