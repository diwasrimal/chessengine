#ifndef MOVE_H
#define MOVE_H

#include <stdint.h>
#include <stdbool.h>

// 4 bits to represent type of move
//           . . . .
//           ^ ^ ^^^
//          /   \  other
//    promotion  capture
//
typedef uint8_t MoveFlag;
enum {
    QUIET = 0,
    DOUBLE_PAWN_PUSH = 0b0001,
    KING_CASTLE = 0b0010,
    QUEEN_CASTLE = 0b0011,
    CAPTURE = 0b0100,
    EP_CAPTURE = 0b0101,
    PROMOTION = 0b1000,
    KNIGHT_PROMOTION = 0b00 | PROMOTION,
    BISHOP_PROMOTION = 0b01 | PROMOTION,
    ROOK_PROMOTION = 0b10 | PROMOTION,
    QUEEN_PROMOTION = 0b11 | PROMOTION,
    KNIGHT_PROMO_CAPTURE = KNIGHT_PROMOTION | CAPTURE,
    BISHOP_PROMO_CAPTURE = BISHOP_PROMOTION | CAPTURE,
    ROOK_PROMO_CAPTURE = ROOK_PROMOTION | CAPTURE,
    QUEEN_PROMO_CAPTURE = QUEEN_PROMOTION | CAPTURE,
};

// 4 bits for flag, 6, 6 for src and dst squares
// FFFFSSSSSSDDDDDD
typedef uint16_t Move;
enum {
    EMPTY_MOVE = 0,
    MFLAG_MASK = 15 << 12,
    SRC_SQ_MASK = 63 << 6,
    DST_SQ_MASK = 63,
};

Move moveEncode(MoveFlag flag, int src_sq, int dst_sq);
MoveFlag getMoveFlag(Move m);
int getMoveSrc(Move m);
int getMoveDst(Move m);
void printMoveToString(char *str, int max_str_size, Move m, bool print_flag);

#endif // MOVE_H
