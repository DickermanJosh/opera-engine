#include <iostream>
#include "Types.h"

using namespace opera;

int main() {
    std::cout << "G8 square value: " << G8 << std::endl;
    std::cout << "Square 62 in chess notation: " << static_cast<char>('a' + (62 % 8)) << static_cast<char>('1' + (62 / 8)) << std::endl;
    return 0;
}