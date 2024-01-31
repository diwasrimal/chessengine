#ifndef TESTS_H
#define TESTS_H

#include "engine.h"

void testMoveGeneration(void);
void testPerformance(void);
void testIsKingChecked(void);
bool checkZobristTillDepth(const Board *b, int depth);
void testZobristHashes(void);

#endif // TESTS_H
