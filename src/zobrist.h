#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <stdint.h>
// This struct of pseudorandom numbers is used to hash a chess position
// hashing a position allows caching the search results in a transposition table
// pieces[2][6][64]: 2 colors, 6 piece types, for each of 64 squares
// castles[16]: total 16 combination of castling right is possible
// black: denotes that it's black's turn to move
struct ZobristValues {
    uint64_t pieces[2][6][64];
    uint64_t castles[16];
    uint64_t ep_square[64];
    uint64_t black;
} ZOBRIST;

void populateZobristValues(void);

#endif // !ZOBRIST_H
