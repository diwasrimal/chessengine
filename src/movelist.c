#include "movelist.h"
#include <stdio.h>

void printMoveList(const MoveList move_list)
{
    char str[20];
    printf("Total moves: %lu\n", move_list.count);
    for (size_t i = 0; i < move_list.count; i++) {
        Move m = move_list.moves[i];
        printMoveToString(str, sizeof(str), m, true);
        printf("%s ", str);
    }
    printf("\n");
}
