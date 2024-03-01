#include "zobrist.h"
#include "utils.h"
#include <stdlib.h>

// Populates the ZOBRIST struct with random values
// Called by engine during start, via precomputeValues()
void populateZobristValues(void)
{
    const unsigned seed = 433453234;
    srand(seed);

    for (int col_idx = 0; col_idx < 2; col_idx++) {
        for (int piece_idx = 0; piece_idx < 6; piece_idx++) {
            for (int sq = 0; sq < 64; sq++) {
                ZOBRIST.pieces[col_idx][piece_idx][sq] = rand64();
            }
        }
    }

    for (int castle = 0; castle < 16; castle++)
        ZOBRIST.castles[castle] = rand64();

    for (int ep_sq = 0; ep_sq < 64; ep_sq++)
        ZOBRIST.ep_square[ep_sq] = rand64();

    ZOBRIST.black = rand64();
}
