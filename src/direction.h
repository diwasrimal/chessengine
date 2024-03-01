#ifndef DIRECTION_H
#define DIRECTION_H

typedef enum {
    RIGHT,
    LEFT,
    UP,
    DOWN,
    TOPRIGHT,
    BOTRIGHT,
    TOPLEFT,
    BOTLEFT,
} Direction;

// Number of cells required to go in that `Direction`
extern const int DIR_OFFSETS[8];

// Index 0 for white, 1 for black
extern const Direction PAWN_FORWARD_DIRS[2];
extern const Direction PAWN_DIAGNOAL_DIRS[2][2];

#endif /* ifndef DIRECTION_H */
