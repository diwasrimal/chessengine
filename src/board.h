#ifndef BOARD_H
#define BOARD_H

#include "piece.h"
#include "castle.h"

typedef struct {
    Piece pieces[64];
    Piece color_to_move;
    CastleRight castle_rights;
    int ep_square;
    int halfmove_clock;
    int fullmoves;
    int king_squares[2];
    uint64_t zobrist_hash;
} Board;

Board initBoardFromFen(char *starting_fen);
uint64_t getZobristHash(const Board *b);
void printBoard(const Board b);

#endif // !BOARD_H
