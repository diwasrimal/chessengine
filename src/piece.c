#include "piece.h"
#include <ctype.h>

int getPieceIdx(Piece p)
{
    return (p & KING)     ? KING_IDX
           : (p & QUEEN)  ? QUEEN_IDX
           : (p & BISHOP) ? BISHOP_IDX
           : (p & KNIGHT) ? KNIGHT_IDX
           : (p & ROOK)   ? ROOK_IDX
                          : PAWN_IDX;
}

char pieceToNotation(const Piece p)
{
    char notation;

    if (p & PAWN)
        notation = 'P';
    else if (p & ROOK)
        notation = 'R';
    else if (p & KNIGHT)
        notation = 'N';
    else if (p & BISHOP)
        notation = 'B';
    else if (p & QUEEN)
        notation = 'Q';
    else if (p & KING)
        notation = 'K';
    else
        notation = ' ';

    return (p & BLACK) ? tolower(notation) : notation;
}

bool haveSameColor(Piece p1, Piece p2)
{
    // First 6 bits denote piece type, remove them and compare
    return ((p1 >> 6) == (p2 >> 6));
}
