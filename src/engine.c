#include "engine.h"
#include "direction.h"
#include "generator.h"
#include "movelist.h"
#include "utils.h"
#include "zobrist.h"

#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

bool LOG_SEARCH = false;

// Used for sorting moves
int compareMove(const void *m1, const void *m2);
void orderMoves(MoveList *mlist);

void precomputeValues(void)
{
    populateGeneratorValues();
    populateZobristValues();
}

// Finds if king with given color is in check
bool isKingChecked(const Board *b, Piece color)
{
    int col_idx = color == WHITE ? 0 : 1;
    int king_sq = b->king_squares[col_idx];
    if (king_sq == -1)
        assert(0 && "King not found!");

    // Find squares attacked by opposite color
    Piece opposing_color = (color == WHITE) ? BLACK : WHITE;
    uint64_t attacks = generateAttackMap(b, opposing_color);

    uint64_t king_mask = 1ull << king_sq;
    if (attacks & king_mask)
        return true;

    return false;
}

MoveList generateMoves(const Board *b)
{
    MoveList pseudolegals = generatePseudoLegalMoves(b);

    // Filter pseudolegal moves - moves that will leave our king in check
    MoveList legalmoves = {.count = 0};
    for (size_t i = 0; i < pseudolegals.count; i++) {
        Move m = pseudolegals.moves[i];
        Board updated = moveMake(m, *b);
        if (isKingChecked(&updated, b->color_to_move))
            continue;
        legalmoves.moves[legalmoves.count++] = m;
    }

    orderMoves(&legalmoves);
    return legalmoves;
}

Board moveMake(Move m, Board b)
{
    const MoveFlag flag = getMoveFlag(m);
    const int src_sq = getMoveSrc(m);
    const int dst_sq = getMoveDst(m);
    const int col_idx = (b.pieces[src_sq] & WHITE) ? 0 : 1;
    const int opp_col_idx = 1 - col_idx;

    //
    // En passant handling
    //

    // Reset en passantable square on every move
    bool had_ep_square = b.ep_square != -1;
    if (had_ep_square)
        b.zobrist_hash ^= ZOBRIST.ep_square[b.ep_square];
    b.ep_square = -1;

    // Record en passantable square if move is a double pawn push
    int pawn_forward_offset = DIR_OFFSETS[PAWN_FORWARD_DIRS[col_idx]];
    if (flag == DOUBLE_PAWN_PUSH) {
        b.ep_square = dst_sq - pawn_forward_offset;
        b.zobrist_hash ^= ZOBRIST.ep_square[b.ep_square];
    }

    // Capture pawn below dst square during en passant
    if (flag == EP_CAPTURE) {
        int captured_sq = dst_sq - pawn_forward_offset;
        b.pieces[captured_sq] = EMPTY_PIECE;
        b.zobrist_hash ^= ZOBRIST.pieces[opp_col_idx][PAWN_IDX][captured_sq];
    }

    //
    // Castles
    //

    // Remove previous castle right info from zobrist hash
    b.zobrist_hash ^= ZOBRIST.castles[b.castle_rights];

    // Revoke castle rights if king is moving
    // and update king square
    if (b.pieces[src_sq] & KING) {
        b.castle_rights &= CRIGHT_REVOKING_MASK[col_idx];
        b.king_squares[col_idx] = dst_sq;
    }

    // Move rook when castled
    if (flag == QUEEN_CASTLE) {
        int rook_src_sq = QSC_ROOK_SRC_SQ[col_idx];
        int rook_dst_sq = QSC_ROOK_DST_SQ[col_idx];
        b.pieces[rook_dst_sq] = b.pieces[rook_src_sq];
        b.pieces[rook_src_sq] = EMPTY_PIECE;
        b.zobrist_hash ^= ZOBRIST.pieces[col_idx][ROOK_IDX][rook_src_sq] ^
                          ZOBRIST.pieces[col_idx][ROOK_IDX][rook_dst_sq];
    }
    else if (flag == KING_CASTLE) {
        int rook_src_sq = KSC_ROOK_SRC_SQ[col_idx];
        int rook_dst_sq = KSC_ROOK_DST_SQ[col_idx];
        b.pieces[rook_dst_sq] = b.pieces[rook_src_sq];
        b.pieces[rook_src_sq] = EMPTY_PIECE;
        b.zobrist_hash ^= ZOBRIST.pieces[col_idx][ROOK_IDX][rook_src_sq] ^
                          ZOBRIST.pieces[col_idx][ROOK_IDX][rook_dst_sq];
    }

    // We lose castle right on a side if we move our rook
    if (b.pieces[src_sq] & ROOK) {
        if (b.castle_rights & QSC_FLAGS[col_idx] && src_sq == QSC_ROOK_SRC_SQ[col_idx])
            b.castle_rights ^= QSC_FLAGS[col_idx];
        if (b.castle_rights & KSC_FLAGS[col_idx] && src_sq == KSC_ROOK_SRC_SQ[col_idx])
            b.castle_rights ^= KSC_FLAGS[col_idx];
    }

    // Opponent loses castle right on a side if we capture their rook
    if (b.pieces[dst_sq] & ROOK) {
        if (b.castle_rights & QSC_FLAGS[opp_col_idx] && dst_sq == QSC_ROOK_SRC_SQ[opp_col_idx])
            b.castle_rights ^= QSC_FLAGS[opp_col_idx];
        if (b.castle_rights & KSC_FLAGS[opp_col_idx] && dst_sq == KSC_ROOK_SRC_SQ[opp_col_idx])
            b.castle_rights ^= KSC_FLAGS[opp_col_idx];
    }

    // Record updated castle right information in zobrist hash
    b.zobrist_hash ^= ZOBRIST.castles[b.castle_rights];

    //
    // Increment / reset halfmove clock
    //
    b.halfmove_clock++;
    if (flag & CAPTURE || b.pieces[src_sq] & PAWN)
        b.halfmove_clock = 0;

    //
    // src -> dst movement
    //

    // Remove piece from dst square if non empty
    if (b.pieces[dst_sq] != EMPTY_PIECE) {
        int dst_pi = getPieceIdx(b.pieces[dst_sq]);
        b.zobrist_hash ^= ZOBRIST.pieces[opp_col_idx][dst_pi][dst_sq];
    }

    // Put src piece at dst square
    int src_pi = getPieceIdx(b.pieces[src_sq]);
    b.pieces[dst_sq] = b.pieces[src_sq];
    b.zobrist_hash ^= ZOBRIST.pieces[col_idx][src_pi][dst_sq];

    // Empty src square
    b.pieces[src_sq] = EMPTY_PIECE;
    b.zobrist_hash ^= ZOBRIST.pieces[col_idx][src_pi][src_sq];

    //
    // Promotion (change piece at dst square)
    //

    if (flag & PROMOTION) {
        Piece promoted_piece = EMPTY_PIECE;
        if (flag == KNIGHT_PROMOTION || flag == KNIGHT_PROMO_CAPTURE)
            promoted_piece = KNIGHT;
        else if (flag == QUEEN_PROMOTION || flag == QUEEN_PROMO_CAPTURE)
            promoted_piece = QUEEN;
        else if (flag == BISHOP_PROMOTION || flag == BISHOP_PROMO_CAPTURE)
            promoted_piece = BISHOP;
        else if (flag == ROOK_PROMOTION || flag == ROOK_PROMO_CAPTURE)
            promoted_piece = ROOK;

        assert(promoted_piece != EMPTY_PIECE);

        // Remove piece currently sitting at dst square
        // this is the piece moved from src square to dst square previously
        b.zobrist_hash ^= ZOBRIST.pieces[col_idx][src_pi][dst_sq];

        // Put new promoted piece at dst square
        b.pieces[dst_sq] = b.color_to_move | promoted_piece;
        int dst_pi = getPieceIdx(b.pieces[dst_sq]);
        b.zobrist_hash ^= ZOBRIST.pieces[col_idx][dst_pi][dst_sq];
    }

    // Change turn and update fullmoves
    if (b.color_to_move == WHITE) {
        b.color_to_move = BLACK;
    }
    else {
        b.color_to_move = WHITE;
        b.fullmoves++;
    }
    b.zobrist_hash ^= ZOBRIST.black;

    return b;
}

uint64_t generateTillDepth(Board b, int depth, bool show_move)
{
    if (depth == 0)
        return 1;

    int total = 0;
    char move_str[20];
    MoveList list = generatePseudoLegalMoves(&b);

    for (size_t i = 0; i < list.count; i++) {
        Board new = moveMake(list.moves[i], b);
        int n_moves = generateTillDepth(new, depth - 1, false);
        if (show_move) {
            printMoveToString(list.moves[i], move_str, true);
            printf("%s: %d\n", move_str, n_moves);
        }
        total += n_moves;
    }

    return total;
}

int evaluateBoard(const Board *b)
{
    enum PieceType {
        king,
        queen,
        bishop,
        knight,
        rook,
        pawn,
    };

    int count[2][6] = {0};

    for (int sq = 0; sq < 64; sq++) {
        int col_idx = b->pieces[sq] & WHITE ? 0 : 1;
        if (b->pieces[sq] & KING)
            count[col_idx][king]++;
        else if (b->pieces[sq] & QUEEN)
            count[col_idx][queen]++;
        else if (b->pieces[sq] & BISHOP)
            count[col_idx][bishop]++;
        else if (b->pieces[sq] & KNIGHT)
            count[col_idx][knight]++;
        else if (b->pieces[sq] & ROOK)
            count[col_idx][rook]++;
        else if (b->pieces[sq] & PAWN)
            count[col_idx][pawn]++;
    }

    // White is supposed to be maximizing
    int material_score = (count[0][queen] - count[1][queen]) * 90 +
                         (count[0][bishop] - count[1][bishop]) * 30 +
                         (count[0][knight] - count[1][knight]) * 30 +
                         (count[0][rook] - count[1][rook]) * 50 +
                         (count[0][pawn] - count[1][pawn]) * 10;

    return material_score;
}


Move findBestMove(const Board *b)
{
    bool is_maximizing = (b->color_to_move & WHITE) ? true : false;
    int minimax_depth = 6;
    int best_score = is_maximizing ? INT_MIN : INT_MAX;
    int best_move = EMPTY_MOVE;
    int alpha = INT_MIN;
    int beta = INT_MAX;

    MoveList mlist = generatePseudoLegalMoves(b);
    clock_t start = clock();
    char move_str[20];

    // No need to search if only one valid move remaining
    if (mlist.count == 1)
        return mlist.moves[0];

    for (size_t i = 0; i < mlist.count; i++) {
        // printMoveToString(mlist.moves[i], move_str, true);
        Board updated = moveMake(mlist.moves[i], *b);
        if (is_maximizing) {
            int score = bestEvaluation(&updated, minimax_depth - 1, false, alpha, beta);
            if (LOG_SEARCH)
                printf("Move: %s, is_maximizing: %d, score: %d, best_score: %d\n", move_str, is_maximizing, score, best_score);
            if (score > best_score) {
                best_score = score;
                best_move = mlist.moves[i];
            }
        } else {
            int score = bestEvaluation(&updated, minimax_depth - 1, true, alpha, beta);
            if (LOG_SEARCH)
                printf("Move: %s, is_maximizing: %d, score: %d, best_score: %d\n", move_str, is_maximizing, score, best_score);
            if (score < best_score) {
                best_score = score;
                best_move = mlist.moves[i];
            }
        }
    }

    clock_t diff = clock() - start;
    double ms = (double)diff * 1000 / (double)CLOCKS_PER_SEC;
    printf("Searched depth: %d, Time elpased: %lf ms\n", minimax_depth, ms);
    return best_move;
}

int bestEvaluation(const Board *b, int depth, bool is_maximizing, int alpha, int beta)
{
    if (depth == 0)
        return evaluateBoard(b);

    int best_score = is_maximizing ? INT_MIN : INT_MAX;
    MoveList mlist = generatePseudoLegalMoves(b);
    char move_str[20];

    for (size_t i = 0; i < mlist.count; i++) {
        // printMoveToString(mlist.moves[i], move_str, true);
        Board updated = moveMake(mlist.moves[i], *b);
        if (is_maximizing) {
            int score = bestEvaluation(&updated, depth - 1, false, alpha, beta);
            if (LOG_SEARCH) {
                for (int i = 0; i < 3 - depth; i++) printf("    ");
                printf("Move: %s, is_maximizing: %d, score: %d, best_score: %d\n", move_str, is_maximizing, score, best_score);
            }
            if (score > best_score) {
                best_score = score;
                alpha = score;
            }
            if (score >= beta) {
                if (LOG_SEARCH) printf("score >= beta, pruning...\n");
                return beta;
            }
        } else {
            int score = bestEvaluation(&updated, depth - 1, true, alpha, beta);
            if (LOG_SEARCH) {
                for (int i = 0; i < 3 - depth; i++) printf("    ");
                printf("Move: %s, is_maximizing: %d, score: %d, best_score: %d\n", move_str, is_maximizing, score, best_score);
            }
            if (score < best_score) {
                best_score = score;
                beta = score;
            }
            if (score <= alpha) {
                if (LOG_SEARCH) printf("score <= alpha, pruning...");
                return alpha;
            }
        }
    }

    return best_score;
}

void orderMoves(MoveList *mlist)
{
    qsort(mlist->moves, mlist->count, sizeof(Move), compareMove);
}

int compareMove(const void *m1, const void *m2)
{
    MoveFlag flag1 = getMoveFlag(*(Move *)m1);
    MoveFlag flag2 = getMoveFlag(*(Move *)m2);

    // Greatest value goes first (decending sort)
    if (flag1 > flag2)
        return -1;
    else if (flag1 < flag2)
        return 1;
    else
        return 0;
}
