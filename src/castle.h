#ifndef CASTLE_H
#define CASTLE_H

#include <stdint.h>

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

// Below externs are defined in castle.c

// These variables are used for making moves,
// generating castling moves,
// changing castle rights,
// or checking castle rights
extern const int QSC_FLAGS[2];
extern const int KSC_FLAGS[2];

// Squares that should be empty for castle to happen
extern const int QSC_EMPTY_SQ[2][3];
extern const int KSC_EMPTY_SQ[2][2];

// Squares that should be safe for castle to happen
extern const int QSC_SAFE_SQ[2][2];
extern const int KSC_SAFE_SQ[2][2];

// Squares the king moves to during castle
extern const int QS_DST_SQ[2];
extern const int KS_DST_SQ[2];

// Rook's src and dst squares during castle
extern const int QSC_ROOK_SRC_SQ[2];
extern const int QSC_ROOK_DST_SQ[2];
extern const int KSC_ROOK_SRC_SQ[2];
extern const int KSC_ROOK_DST_SQ[2];

// This is used to revoke castling right
// castle_right & CRIGHT_REVOKING_MASK[0] revokes both castling rights for white
extern const CastleRight CRIGHT_REVOKING_MASK[2];

#endif // !CASTLE_H
