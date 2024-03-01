#ifndef MOVELIST_H
#define MOVELIST_H

#include "move.h"
#include <stddef.h>

// Suppose 256 as maximum number of moves
typedef struct {
    Move moves[256];
    size_t count;
} MoveList;

void printMoveList(const MoveList move_list);


#endif // !MOVELIST_H
