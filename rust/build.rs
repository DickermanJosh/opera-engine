//! Build script for cxx integration with C++ Opera Engine core

#[cfg(feature = "ffi")]
use std::path::PathBuf;

fn main() {
    // Only build C++ integration when FFI feature is enabled
    #[cfg(feature = "ffi")]
    {
        println!("cargo:rerun-if-changed=src/ffi.rs");
        println!("cargo:rerun-if-changed=../cpp/include/");
        println!("cargo:rerun-if-changed=../cpp/src/");

        // Get the path to the C++ engine
        let cpp_include_path = PathBuf::from("../cpp/include");

        // Configure cxx build with compatible toolchain settings
        // Build directly from source to avoid static library compatibility issues
        let mut bridge = cxx_build::bridge("src/ffi.rs");
        bridge
            // UCI Bridge FFI
            .file("../cpp/src/UCIBridge.cpp")
            // Board representation
            .file("../cpp/src/board/Board.cpp")
            .file("../cpp/src/board/MoveGenerator.cpp")
            // Search engine
            .file("../cpp/src/search/search_engine.cpp")
            .file("../cpp/src/search/alphabeta.cpp")
            .file("../cpp/src/search/transposition_table.cpp")
            .file("../cpp/src/search/move_ordering.cpp")
            .file("../cpp/src/search/see.cpp")
            // Evaluation
            .file("../cpp/src/eval/handcrafted_eval.cpp")
            .file("../cpp/src/eval/morphy_eval.cpp")
            // Utilities
            .file("../cpp/src/utils/Types.cpp")
            .include(&cpp_include_path)
            .cpp(true)
            .std("c++17")
            .opt_level(3)
            .define("NDEBUG", None);

        let target_os = std::env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
        if target_os == "macos" {
            bridge.flag_if_supported("-fno-addrsig");
        }

        bridge.compile("opera-uci-bridge");

        // Platform-specific linking
        // cc/cxx select the target's C++ runtime and MSVC CRT automatically.
        if target_os == "linux" {
            println!("cargo:rustc-link-lib=pthread");
        }
    }

    #[cfg(not(feature = "ffi"))]
    {
        println!("cargo:warning=Skipping C++ build (FFI feature disabled)");
    }
}
