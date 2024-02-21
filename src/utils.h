#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdint.h>

#define MAX(x, y) ((x > y) ? x : y)
#define MIN(x, y) ((x < y) ? x : y)

// Mapping of a square's index to its name
extern const char *SQNAMES[64];

bool isValidSquare(int sq);
bool isValidRankAndFile(int rank, int file);
int squareNameToIdx(char *name);
uint64_t decToBin(int n);

#endif // UTILS_H
