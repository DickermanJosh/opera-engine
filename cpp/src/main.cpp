#include <iostream>
#include <string>
#include <cstdlib>
#include "Board.h"
#include "Types.h"

using namespace opera;

int main(int argc, char* argv[]) {
    // Check for test flag
    if (argc > 1 && std::string(argv[1]) == "--test") {
        std::cout << "Running Opera Engine test suite..." << std::endl;
        
        // Run the test executable
        int result = std::system("./tests/opera_tests");
        
        if (result == 0) {
            std::cout << "\n✅ All tests passed successfully!" << std::endl;
        } else {
            std::cout << "\n❌ Some tests failed. Exit code: " << result << std::endl;
        }
        
        return result;
    }
    
    std::cout << "Opera Chess Engine v1.0.0" << std::endl;
    
    try {
        // Test basic board functionality
        Board board;
        
        std::cout << "Starting position:" << std::endl;
        board.print();
        
        // Test a simple move
        Move e2e4(E2, E4, NORMAL);
        std::cout << "Making move: " << e2e4.toString() << std::endl;
        board.makeMove(e2e4);
        board.print();
        
        std::cout << "Opera Engine initialized successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}