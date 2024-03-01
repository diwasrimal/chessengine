#include "direction.h"

const int DIR_OFFSETS[8] = {1, -1, 8, -8, 9, -9, 7, -7};

const Direction PAWN_FORWARD_DIRS[2] = {UP, DOWN};
const Direction PAWN_DIAGNOAL_DIRS[2][2] = {{TOPLEFT, TOPRIGHT},
                                            {BOTLEFT, BOTRIGHT}};
