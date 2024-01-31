#include "move.h"
#include "utils.h"

#include <stdio.h>

Move moveEncode(MoveFlag flag, int src_sq, int dst_sq)
{
    return ((Move)flag << 12) | ((Move)src_sq << 6) | dst_sq;
}

MoveFlag getMoveFlag(Move m)
{
    return (m & MFLAG_MASK) >> 12;
}

int getMoveSrc(Move m)
{
    return (m & SRC_SQ_MASK) >> 6;
}

int getMoveDst(Move m)
{
    return m & DST_SQ_MASK;
}

void printMoveToString(Move m, char *str, bool print_flag)
{
    MoveFlag flag = getMoveFlag(m);
    int src_sq = getMoveSrc(m);
    int dst_sq = getMoveDst(m);
    const char *src_sqname = SQNAMES[src_sq];
    const char *dst_sqname = SQNAMES[dst_sq];

    char *promo_type = "";
    if (flag == QUEEN_PROMOTION || flag == QUEEN_PROMO_CAPTURE)
        promo_type = "q";
    else if (flag == KNIGHT_PROMOTION || flag == KNIGHT_PROMO_CAPTURE)
        promo_type = "n";
    else if (flag == BISHOP_PROMOTION || flag == BISHOP_PROMO_CAPTURE)
        promo_type = "b";
    else if (flag == ROOK_PROMOTION || flag == ROOK_PROMO_CAPTURE)
        promo_type = "r";

    if (print_flag)
        sprintf(str, "%s%s%s[%04llu]", src_sqname, dst_sqname, promo_type, decToBin(flag));
    else
        sprintf(str, "%s%s%s", src_sqname, dst_sqname, promo_type);
}