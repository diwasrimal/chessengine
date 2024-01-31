#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include "piece.h"
#include "move.h"

// 4 bits in format W W B B represent castling rights
//                  ^ ^
//                 /  |
//           kingside queenside
typedef uint8_t CastleRight;
enum {
    NO_CASTLE = 0,
    BQSC = 1 << 0, // Black Queen Side Castle
    BKSC = 1 << 1,
    WQSC = 1 << 2,
    WKSC = 1 << 3,
};

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

// Suppose 256 as maximum number of moves
typedef struct {
    Move moves[256];
    size_t count;
} MoveList;

// Handles computation of some constant variables, this should be called
// from the main program before doing anything else
void precomputeValues(void);
void populateSquaresTillEdges(void);
void populateAttackMaps(void);
void populateZobristValues(void);

Board initBoardFromFen(char *starting_fen);
uint64_t getZobristHash(const Board *b);
void printBoard(const Board b);

bool isKingChecked(const Board *b, Piece color);
MoveList generateMoves(const Board *b);
void fillSlidingMoves(const Board *b, int src_sq, MoveList *list);
void fillPawnMoves(const Board *b, int src_sq, MoveList *list);
void fillKnightMoves(const Board *b, int src_sq, MoveList *list);
void fillKingMoves(const Board *b, int src_sq, MoveList *list);
uint64_t generateSlidingAttackMap(const Board *b, int src_sq);
uint64_t generatePawnAttackMap(const Board *b, int src_sq);
uint64_t generateAttackMap(const Board *b, Piece attacking_color);
Board moveMake(Move m, Board b);

int compareMove(const void *m1, const void *m2);
void orderMoves(MoveList *mlist);
void printMoveList(const MoveList move_list);

uint64_t rand64(void);

uint64_t generateTillDepth(Board b, int depth, bool show_move);
int evaluateBoard(const Board *b);
Move findBestMove(const Board *b);
int bestEvaluation(const Board *b, int depth, bool is_maximizing, int alpha, int beta);

#endif // ENGINE_H
