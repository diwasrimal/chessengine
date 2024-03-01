#include "generator.h"
#include "utils.h"
#include "direction.h"

// Arrays with dimensions 2 are for colors
// 0 = white, 1 = black
// Variables below are used for generating moves for a specific piece
const int PAWN_PROMOTING_RANK[2] = {7, 0};
const int PAWN_INITIAL_RANK[2] = {1, 6};

const int KNIGHT_RANK_OFFSETS[8] = {2, 2, -2, -2, 1, 1, -1, -1};
const int KNIGHT_FILE_OFFSETS[8] = {-1, 1, -1, 1, -2, 2, -2, 2};

// These are attack maps for king and knights
// which are only dependent on src square (will be same for a give square)
// precomputed once by populateAttackMaps()
uint64_t KING_ATTACK_MAPS[64] = { 0ull };
uint64_t KNIGHT_ATTACK_MAPS[64] = { 0ull };

// Represents number of squares in between a given square and board's edge
// precomputed once by populateSquaresTillEdges()
int SQUARES_TILL_EDGE[64][8];

void populateGeneratorValues(void)
{
    populateSquaresTillEdges();
    populateAttackMaps();
}

// Finds squares between a square and board's edge in all possible directions
// Called only once during board initialization
void populateSquaresTillEdges(void)
{
    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            SQUARES_TILL_EDGE[square][RIGHT] = 8 - file - 1;
            SQUARES_TILL_EDGE[square][LEFT] = file;
            SQUARES_TILL_EDGE[square][UP] = 8 - rank - 1;
            SQUARES_TILL_EDGE[square][DOWN] = rank;
            SQUARES_TILL_EDGE[square][TOPRIGHT] = 8 - MAX(rank, file) - 1;
            SQUARES_TILL_EDGE[square][BOTRIGHT] = MIN(rank, file);
            SQUARES_TILL_EDGE[square][TOPLEFT] = MIN(8 - rank - 1, file);
            SQUARES_TILL_EDGE[square][BOTLEFT] = MIN(8 - file - 1, rank);
        }
    }
}

// This function precomputes attack maps for king and knight
void populateAttackMaps(void)
{
    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int src_sq = rank * 8 + file;

            // King
            for (int direction = 0; direction < 8; direction++) {
                if (SQUARES_TILL_EDGE[src_sq][direction] != 0) {
                    int dst_sq = src_sq + DIR_OFFSETS[direction];
                    KING_ATTACK_MAPS[src_sq] |= 1ull << dst_sq;
                }
            }

            // Knight
            for (int i = 0; i < 8; i++) {
                int r = rank + KNIGHT_RANK_OFFSETS[i];
                int f = file + KNIGHT_FILE_OFFSETS[i];
                if (isValidRankAndFile(r, f)) {
                    int dst_sq = r * 8 + f;
                    KNIGHT_ATTACK_MAPS[src_sq] |= 1ull << dst_sq;
                }
            }
        }
    }
}

MoveList generatePseudoLegalMoves(const Board *b)
{
    MoveList pseudolegals = {.count = 0};

    for (int src_sq = 0; src_sq < 64; src_sq++) {
        if (b->pieces[src_sq] == EMPTY_PIECE ||
            !haveSameColor(b->color_to_move, b->pieces[src_sq]))
            continue;

        if (b->pieces[src_sq] & (ROOK | QUEEN | BISHOP)) {
            fillSlidingMoves(b, src_sq, &pseudolegals);
        }
        else if (b->pieces[src_sq] & PAWN) {
            fillPawnMoves(b, src_sq, &pseudolegals);
        }
        else if (b->pieces[src_sq] & KNIGHT) {
            fillKnightMoves(b, src_sq, &pseudolegals);
        }
        else if (b->pieces[src_sq] & KING) {
            fillKingMoves(b, src_sq, &pseudolegals);
        }
    }

    return pseudolegals;
}

void fillSlidingMoves(const Board *b, int src_sq, MoveList *list)
{
    // Rook only moves straight -> first 4 directions
    // Bishop only moves diagnoals -> 4 - 8 directins
    // Queen goes all directions
    int start = 0, end = 8;
    if (b->pieces[src_sq] & ROOK)
        end = 4;
    else if (b->pieces[src_sq] & BISHOP)
        start = 4;

    for (int direction = start; direction < end; direction++) {
        int offset = DIR_OFFSETS[direction];
        for (int n = 0; n < SQUARES_TILL_EDGE[src_sq][direction]; n++) {

            int dst_sq = src_sq + offset * (n + 1);

            // Path blocked by own piece
            if (haveSameColor(b->pieces[dst_sq], b->pieces[src_sq]))
                break;

            if (b->pieces[dst_sq] != EMPTY_PIECE) {
                list->moves[list->count++] =
                    moveEncode(CAPTURE, src_sq, dst_sq);
                break;
            }
            else {
                list->moves[list->count++] = moveEncode(QUIET, src_sq, dst_sq);
            }
        }
    }
}

void fillPawnMoves(const Board *b, int src_sq, MoveList *list)
{
    int color = (b->pieces[src_sq] & WHITE) ? 0 : 1;
    int rank = src_sq / 8;
    int promoting_rank = PAWN_PROMOTING_RANK[color];
    Direction forward = PAWN_FORWARD_DIRS[color];
    Direction diagonals[2];
    diagonals[0] = PAWN_DIAGNOAL_DIRS[color][0];
    diagonals[1] = PAWN_DIAGNOAL_DIRS[color][1];

    // Forward moves (quiet or promotions)
    int forward_moves = (rank == PAWN_INITIAL_RANK[color]) ? 2 : 1;
    forward_moves = MIN(forward_moves, SQUARES_TILL_EDGE[src_sq][forward]);
    for (int n = 1; n <= forward_moves; n++) {
        int dst_sq = src_sq + DIR_OFFSETS[forward] * n;
        if (b->pieces[dst_sq] != EMPTY_PIECE)
            break;

        if ((dst_sq / 8) == promoting_rank) {
            list->moves[list->count++] =
                moveEncode(ROOK_PROMOTION, src_sq, dst_sq);
            list->moves[list->count++] =
                moveEncode(KNIGHT_PROMOTION, src_sq, dst_sq);
            list->moves[list->count++] =
                moveEncode(BISHOP_PROMOTION, src_sq, dst_sq);
            list->moves[list->count++] =
                moveEncode(QUEEN_PROMOTION, src_sq, dst_sq);
        }
        else {
            MoveFlag flag = (n == 2) ? DOUBLE_PAWN_PUSH : QUIET;
            list->moves[list->count++] = moveEncode(flag, src_sq, dst_sq);
        }
    }

    // Diagnoal moves (en passant possible)
    for (int i = 0; i < 2; i++) {
        int direction = diagonals[i];
        if (SQUARES_TILL_EDGE[src_sq][direction] == 0)
            continue;

        int dst_sq = src_sq + DIR_OFFSETS[direction];
        if (dst_sq == b->ep_square) {
            list->moves[list->count++] = moveEncode(EP_CAPTURE, src_sq, dst_sq);
            continue;
        }

        // Only captures are possible on diagnonals (except en passant)
        if (b->pieces[dst_sq] == EMPTY_PIECE ||
            haveSameColor(b->pieces[src_sq], b->pieces[dst_sq]))
            continue;

        if ((dst_sq / 8) == promoting_rank) {
            list->moves[list->count++] =
                moveEncode(ROOK_PROMO_CAPTURE, src_sq, dst_sq);
            list->moves[list->count++] =
                moveEncode(KNIGHT_PROMO_CAPTURE, src_sq, dst_sq);
            list->moves[list->count++] =
                moveEncode(BISHOP_PROMO_CAPTURE, src_sq, dst_sq);
            list->moves[list->count++] =
                moveEncode(QUEEN_PROMO_CAPTURE, src_sq, dst_sq);
        }
        else {
            list->moves[list->count++] = moveEncode(CAPTURE, src_sq, dst_sq);
        }
    }
}

void fillKnightMoves(const Board *b, int src_sq, MoveList *list)
{
    int rank = src_sq / 8;
    int file = src_sq % 8;

    for (int i = 0; i < 8; i++) {
        int r = rank + KNIGHT_RANK_OFFSETS[i];
        int f = file + KNIGHT_FILE_OFFSETS[i];
        int dst_sq = r * 8 + f;

        // Invalid square or same colored piece
        if (!isValidRankAndFile(r, f) ||
            haveSameColor(b->pieces[src_sq], b->pieces[dst_sq]))
            continue;

        MoveFlag flag = (b->pieces[dst_sq] != EMPTY_PIECE) ? CAPTURE : QUIET;
        list->moves[list->count++] = moveEncode(flag, src_sq, dst_sq);
    }
}

void fillKingMoves(const Board *b, int src_sq, MoveList *list)
{
    // Find squares attacked by opponent
    Piece opposing_color = (b->color_to_move == WHITE) ? BLACK : WHITE;
    uint64_t attacks = generateAttackMap(b, opposing_color);

    // Normal moves
    for (int direction = 0; direction < 8; direction++) {
        if (SQUARES_TILL_EDGE[src_sq][direction] == 0)
            continue;

        int dst_sq = src_sq + DIR_OFFSETS[direction];
        if (haveSameColor(b->pieces[src_sq], b->pieces[dst_sq]))
            continue;

        // King can't move to attacked square
        uint64_t dst_mask = 1ull << dst_sq;
        if (attacks & dst_mask)
            continue;

        MoveFlag flag = (b->pieces[dst_sq] != EMPTY_PIECE) ? CAPTURE : QUIET;
        list->moves[list->count++] = moveEncode(flag, src_sq, dst_sq);
    }

    // Castle not possible if we don't have castle rights or if king is in check
    uint64_t src_mask = 1ull << src_sq;
    bool is_checked = (attacks & src_mask) > 0;
    if (b->castle_rights == NO_CASTLE || is_checked)
        return;

    int col_idx = (b->pieces[src_sq] & WHITE) ? 0 : 1;

    // Queen side castle
    if (b->castle_rights & QSC_FLAGS[col_idx]) {
        bool squares_are_empty =
            b->pieces[QSC_EMPTY_SQ[col_idx][0]] == EMPTY_PIECE &&
            b->pieces[QSC_EMPTY_SQ[col_idx][1]] == EMPTY_PIECE &&
            b->pieces[QSC_EMPTY_SQ[col_idx][2]] == EMPTY_PIECE;
        bool squares_are_safe =
            (attacks & (1ull << QSC_SAFE_SQ[col_idx][0])) == 0 &&
            (attacks & (1ull << QSC_SAFE_SQ[col_idx][1])) == 0;
        if (squares_are_empty && squares_are_safe) {
            list->moves[list->count++] =
                moveEncode(QUEEN_CASTLE, src_sq, QS_DST_SQ[col_idx]);
        }
    }

    // King side castle
    if (b->castle_rights & KSC_FLAGS[col_idx]) {
        bool squares_are_empty =
            b->pieces[KSC_EMPTY_SQ[col_idx][0]] == EMPTY_PIECE &&
            b->pieces[KSC_EMPTY_SQ[col_idx][1]] == EMPTY_PIECE;
        bool squares_are_safe =
            (attacks & (1ull << KSC_SAFE_SQ[col_idx][0])) == 0 &&
            (attacks & (1ull << KSC_SAFE_SQ[col_idx][1])) == 0;
        if (squares_are_empty && squares_are_safe) {
            list->moves[list->count++] =
                moveEncode(KING_CASTLE, src_sq, KS_DST_SQ[col_idx]);
        }
    }
}

uint64_t generateSlidingAttackMap(const Board *b, int src_sq)
{
    uint64_t attacks = 0;

    int start = 0, end = 8;
    if (b->pieces[src_sq] & ROOK)
        end = 4;
    else if (b->pieces[src_sq] & BISHOP)
        start = 4;

    for (int direction = start; direction < end; direction++) {
        int offset = DIR_OFFSETS[direction];
        for (int n = 0; n < SQUARES_TILL_EDGE[src_sq][direction]; n++) {

            int dst_sq = src_sq + offset * (n + 1);
            attacks |= (1ull << dst_sq);

            // Further path blocked
            if (b->pieces[dst_sq] != EMPTY_PIECE)
                break;
        }
    }
    return attacks;
}

uint64_t generatePawnAttackMap(const Board *b, int src_sq)
{
    uint64_t attacks = 0;

    // Pawn only attacks diagnoals
    int color = (b->pieces[src_sq] & WHITE) ? 0 : 1;
    Direction diagonals[2] = {
        PAWN_DIAGNOAL_DIRS[color][0],
        PAWN_DIAGNOAL_DIRS[color][1],
    };

    for (int i = 0; i < 2; i++) {
        if (SQUARES_TILL_EDGE[src_sq][diagonals[i]] != 0) {
            int dst_sq = src_sq + DIR_OFFSETS[diagonals[i]];
            attacks |= 1ull << dst_sq;
        }
    }
    return attacks;
}

uint64_t generateAttackMap(const Board *b, Piece attacking_color)
{
    uint64_t attacks = 0;

    for (int src_sq = 0; src_sq < 64; src_sq++) {
        if (b->pieces[src_sq] == EMPTY_PIECE || !haveSameColor(attacking_color, b->pieces[src_sq]))
            continue;
        if (b->pieces[src_sq] & (ROOK | BISHOP | QUEEN)) {
            attacks |= generateSlidingAttackMap(b, src_sq);
        }
        else if (b->pieces[src_sq] & PAWN) {
            attacks |= generatePawnAttackMap(b, src_sq);
        }
        else if (b->pieces[src_sq] & KNIGHT) {
            attacks |= KNIGHT_ATTACK_MAPS[src_sq];
        }
        else if (b->pieces[src_sq] & KING) {
            attacks |= KING_ATTACK_MAPS[src_sq];
        }
    }
    return attacks;
}
