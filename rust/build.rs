// Build script for cxx integration with C++ Opera Engine core
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=src/ffi.rs");
    println!("cargo:rerun-if-changed=../cpp/include/");
    println!("cargo:rerun-if-changed=../cpp/src/");
    
    // Get the path to the C++ engine
    let cpp_include_path = PathBuf::from("../cpp/include");
    let cpp_lib_path = PathBuf::from("../cpp/build");
    
    // Configure cxx build
    cxx_build::bridge("src/ffi.rs")
        .file("../cpp/src/UCIBridge.cpp")  // Will be created in task 1.2
        .include(&cpp_include_path)
        .flag("-std=c++17")
        .flag("-O3")
        .flag("-DNDEBUG")
        .compile("opera-uci-bridge");
    
    // Link against the Opera Engine core library
    println!("cargo:rustc-link-search=native={}", cpp_lib_path.display());
    println!("cargo:rustc-link-lib=static=opera_core");
    
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