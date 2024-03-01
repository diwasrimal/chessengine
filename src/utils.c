#include "utils.h"
#include <stdlib.h>

const char *SQNAMES[64] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};

int squareNameToIdx(char *name)
{
    char file = name[0] - 'a';
    char rank = name[1] - '1';
    return rank * 8 + file;
}

bool isValidSquare(int sq)
{
    return 0 <= sq && sq < 64;
}

bool isValidRankAndFile(int rank, int file)
{
    return 0 <= rank && rank < 8 && 0 <= file && file < 8;
}

uint64_t decToBin(int n)
{
    int bin[32];
    int i = 0;
    while (n > 0) {
        bin[i] = n % 2;
        n = n / 2;
        i++;
    }

    uint64_t result = 0;
    for (int j = i - 1; j >= 0; j--)
        result = result * 10 + bin[j];

    return result;
}

// Generates random 64 bit integer (assuming srand() is called)
uint64_t rand64(void)
{
    uint64_t r1 = rand(); // assuming RAND_MAX == 2^32-1
    uint64_t r2 = rand();
    return (r1 << 32) | (r2);
}
