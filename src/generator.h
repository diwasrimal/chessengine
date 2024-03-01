#ifndef GENERATOR_H
#define GENERATOR_H

#include "board.h"
#include "movelist.h"
#include <stdint.h>

// Computes some global variables, required for the move generation
// Precomputing makes the move generation fast
void populateGeneratorValues(void);
void populateSquaresTillEdges(void);
void populateAttackMaps(void);

MoveList generatePseudoLegalMoves(const Board *b);
void fillSlidingMoves(const Board *b, int src_sq, MoveList *list);
void fillPawnMoves(const Board *b, int src_sq, MoveList *list);
void fillKnightMoves(const Board *b, int src_sq, MoveList *list);
void fillKingMoves(const Board *b, int src_sq, MoveList *list);
uint64_t generateSlidingAttackMap(const Board *b, int src_sq);
uint64_t generatePawnAttackMap(const Board *b, int src_sq);
uint64_t generateAttackMap(const Board *b, Piece attacking_color);

#endif // !GENERATOR_H
