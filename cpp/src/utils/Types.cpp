#include "Types.h"
#include <sstream>
#include <cctype>

namespace opera {

std::string Move::toString() const {
    if (data == 0) return "0000";
    
    std::ostringstream oss;
    
    // From square
    Square fromSq = from();
    oss << static_cast<char>('a' + fileOf(fromSq));
    oss << static_cast<char>('1' + rankOf(fromSq));
    
    // To square
    Square toSq = to();
    oss << static_cast<char>('a' + fileOf(toSq));
    oss << static_cast<char>('1' + rankOf(toSq));
    
    // Promotion piece
    if (isPromotion()) {
        switch (promotionType()) {
            case QUEEN: oss << 'q'; break;
            case ROOK: oss << 'r'; break;
            case BISHOP: oss << 'b'; break;
            case KNIGHT: oss << 'n'; break;
            default: break;
        }
    }
    
    return oss.str();
}

Move Move::fromString(const std::string& moveStr) {
    if (moveStr.length() < 4 || moveStr.length() > 5) {
        return Move(); // Invalid move
    }
    
    // Parse from square
    char fromFile = moveStr[0];
    char fromRank = moveStr[1];
    if (fromFile < 'a' || fromFile > 'h' || fromRank < '1' || fromRank > '8') {
        return Move(); // Invalid from square
    }
    Square from = makeSquare(fromFile - 'a', fromRank - '1');
    
    // Parse to square
    char toFile = moveStr[2];
    char toRank = moveStr[3];
    if (toFile < 'a' || toFile > 'h' || toRank < '1' || toRank > '8') {
        return Move(); // Invalid to square
    }
    Square to = makeSquare(toFile - 'a', toRank - '1');
    
    // Parse promotion (if present)
    PieceType promotion = NO_PIECE_TYPE;
    MoveType moveType = NORMAL;
    
    if (moveStr.length() == 5) {
        char promChar = std::tolower(moveStr[4]);
        switch (promChar) {
            case 'q': promotion = QUEEN; break;
            case 'r': promotion = ROOK; break;
            case 'b': promotion = BISHOP; break;
            case 'n': promotion = KNIGHT; break;
            default: return Move(); // Invalid promotion
        }
        moveType = PROMOTION;
    }
    
    return Move(from, to, moveType, promotion);
}

} // namespace opera