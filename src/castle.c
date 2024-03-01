#include "castle.h"

// These variables are used for making moves,
// generating castling moves,
// changing castle rights,
// or checking castle rights
const int QSC_FLAGS[2] = {WQSC, BQSC};
const int KSC_FLAGS[2] = {WKSC, BKSC};

// Squares that should be empty for castle to happen
const int QSC_EMPTY_SQ[2][3] = {{1, 2, 3}, {57, 58, 59}};
const int KSC_EMPTY_SQ[2][2] = {{5, 6}, {61, 62}};

// Squares that should be safe for castle to happen
const int QSC_SAFE_SQ[2][2] = {{2, 3}, {58, 59}};
const int KSC_SAFE_SQ[2][2] = {{5, 6}, {61, 62}};

// Squares the king moves to during castle
const int QS_DST_SQ[2] = {2, 58};
const int KS_DST_SQ[2] = {6, 62};

// Rook's src and dst squares during castle
const int QSC_ROOK_SRC_SQ[2] = {0, 56};
const int QSC_ROOK_DST_SQ[2] = {3, 59};
const int KSC_ROOK_SRC_SQ[2] = {7, 63};
const int KSC_ROOK_DST_SQ[2] = {5, 61};

// This is used to revoke castling right
// castle_right & CRIGHT_REVOKING_MASK[0] revokes both castling rights for white
const CastleRight CRIGHT_REVOKING_MASK[2] = {0b0011, 0b1100};
