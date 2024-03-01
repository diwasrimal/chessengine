#ifndef ENGINE_H
#define ENGINE_H

#include "board.h"
#include "move.h"
#include "movelist.h"
#include <stdint.h>

// Handles computation of some constant variables, this should be called
// from the main program before doing anything else
void precomputeValues(void);

MoveList generateMoves(const Board *b);
Board moveMake(Move m, Board b);
bool isKingChecked(const Board *b, Piece color);

uint64_t generateTillDepth(Board b, int depth, bool show_move);
Move findBestMove(const Board *b);
int evaluateBoard(const Board *b);
int bestEvaluation(const Board *b, int depth, bool is_maximizing, int alpha, int beta);

#endif // ENGINE_H
