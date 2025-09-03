//! Build script for cxx integration with C++ Opera Engine core

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
            .file("../cpp/src/UCIBridge.cpp")
            .file("../cpp/src/board/Board.cpp")
            .file("../cpp/src/board/MoveGenerator.cpp")
            .file("../cpp/src/utils/Types.cpp")
            .include(&cpp_include_path)
            .flag("-std=c++17")
            .flag("-O3")
            .flag("-DNDEBUG");

        // Force use of system clang to avoid toolchain mismatch
        #[cfg(target_os = "macos")]
        {
            bridge.cpp(true).flag("-fno-addrsig"); // Disable address significance tables that cause compatibility issues
        }

        bridge.compile("opera-uci-bridge");

        // Platform-specific linking
        #[cfg(target_os = "linux")]
        println!("cargo:rustc-link-lib=pthread");

        #[cfg(target_os = "macos")]
        println!("cargo:rustc-link-lib=c++");

        #[cfg(target_os = "windows")]
        {
            println!("cargo:rustc-link-lib=msvcrt");
            println!("cargo:rustc-link-lib=kernel32");
        }
    }

    #[cfg(not(feature = "ffi"))]
    {
        println!("cargo:warning=Skipping C++ build (FFI feature disabled)");
    }
}
