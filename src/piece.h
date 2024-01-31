#ifndef PIECE_H
#define PIECE_H

#include <stdint.h>
#include <stdbool.h>

// 8 bits to represent a colored piece
//      . .         . . . . . .
//      ^ ^         ^ ^ ^ ^ ^ ^
//     /   \        | | | | | |
//   black white    k q b n r p
typedef uint8_t Piece;

enum {
    EMPTY_PIECE = 0,
    PAWN = 1 << 0,
    ROOK = 1 << 1,
    KNIGHT = 1 << 2,
    BISHOP = 1 << 3,
    QUEEN = 1 << 4,
    KING = 1 << 5,
    WHITE = 1 << 6,
    BLACK = 1 << 7,
};

typedef enum {
    KING_IDX,
    QUEEN_IDX,
    BISHOP_IDX,
    KNIGHT_IDX,
    ROOK_IDX,
    PAWN_IDX,
} PieceIdx;


int getPieceIdx(Piece p);
char pieceToNotation(const Piece p);
bool haveSameColor(Piece p1, Piece p2);

#endif // PIECE_H