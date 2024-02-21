#include "engine.h"
#include "utils.h"

#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
const int DIRECTION_OFFSETS[8] = {1, -1, 8, -8, 9, -9, 7, -7};

// Arrays with dimensions 2 are for colors
// 0 = white, 1 = black
// Variables below are used for generating moves for a specific piece
const int PAWN_PROMOTING_RANK[2] = {7, 0};
const int PAWN_INITIAL_RANK[2] = {1, 6};
const Direction PAWN_FORWARDS[2] = {UP, DOWN};
const Direction PAWN_DIAGNOALS[2][2] = {{TOPLEFT, TOPRIGHT},
                                        {BOTLEFT, BOTRIGHT}};
const int KNIGHT_RANK_OFFSETS[8] = {2, 2, -2, -2, 1, 1, -1, -1};
const int KNIGHT_FILE_OFFSETS[8] = {-1, 1, -1, 1, -2, 2, -2, 2};

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

// These are attack maps for king and knights
// which are only dependent on src square (will be same for a give square)
// precomputed once by populateAttackMaps()
uint64_t KING_ATTACK_MAPS[64] = { 0ull };
uint64_t KNIGHT_ATTACK_MAPS[64] = { 0ull };

// Represents number of squares in between a given square and board's edge
// precomputed once by populateSquaresTillEdges()
int SQUARES_TILL_EDGE[64][8];

bool LOG_SEARCH = false;


void precomputeValues(void)
{
    populateSquaresTillEdges();
    populateAttackMaps();
    populateZobristValues();
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
                    int dst_sq = src_sq + DIRECTION_OFFSETS[direction];
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

Board initBoardFromFen(char *starting_fen)
{
    // Initialize with default values
    Board b = {
        .castle_rights = NO_CASTLE,
        .ep_square = -1,
        .halfmove_clock = 0,
        .fullmoves = 1,
        .king_squares = {-1},
        .zobrist_hash = 0,
    };

    Piece types[256];
    types['K'] = WHITE | KING;
    types['Q'] = WHITE | QUEEN;
    types['B'] = WHITE | BISHOP;
    types['N'] = WHITE | KNIGHT;
    types['R'] = WHITE | ROOK;
    types['P'] = WHITE | PAWN;
    types['k'] = BLACK | KING;
    types['q'] = BLACK | QUEEN;
    types['b'] = BLACK | BISHOP;
    types['n'] = BLACK | KNIGHT;
    types['r'] = BLACK | ROOK;
    types['p'] = BLACK | PAWN;

    char fen[100];
    strcpy(fen, starting_fen);

    // Piece arrangement
    char *arrangement = strtok(fen, " ");
    if (arrangement == NULL)
        assert(0 && "No piece arrangement information in FEN");

    size_t n = strlen(arrangement);
    int rank = 7, file = 0;
    for (size_t i = 0; i < n; i++) {
        char c = arrangement[i];
        if (c == '/') {
            rank--;
            file = 0;
        }
        else if (isalpha(c)) {
            int sq = rank * 8 + file;
            b.pieces[sq] = types[(uint8_t)c];
            if (c == 'K')
                b.king_squares[0] = sq;
            else if (c == 'k')
                b.king_squares[1] = sq;
            file++;
        }
        else if (isdigit(c)) {
            int skips = c - '0';
            for (int j = 0; j < skips; j++) {
                b.pieces[rank * 8 + file] = EMPTY_PIECE;
                file++;
            }
        }
    }

    // Turn
    char *turn = strtok(NULL, " ");
    if (turn == NULL)
        assert(0 && "Insufficient FEN string information");
    b.color_to_move = (turn[0] == 'b' ? BLACK : WHITE);

    // Castle rights
    char *rights = strtok(NULL, " ");
    if (rights == NULL)
        assert(0 && "No castling rights in FEN");

    if (rights[0] != '-') {
        n = strlen(rights);
        for (size_t i = 0; i < n; i++) {
            switch (rights[i]) {
            case 'K':
                b.castle_rights |= WKSC;
                break;
            case 'Q':
                b.castle_rights |= WQSC;
                break;
            case 'k':
                b.castle_rights |= BKSC;
                break;
            case 'q':
                b.castle_rights |= BQSC;
                break;
            }
        }
    }

    // En passant target square
    char *ep_square = strtok(NULL, " ");
    if (ep_square == NULL)
        assert(0 && "No ep square given in FEN");
    if (ep_square[0] != '-') {
        b.ep_square = squareNameToIdx(ep_square);
    }

    // Haflmove clock and fullmoves
    char *halfmove_clock = strtok(NULL, " ");
    if (halfmove_clock == NULL)
        goto defer;
    b.halfmove_clock = atoi(halfmove_clock);

    char *fullmoves = strtok(NULL, " ");
    if (fullmoves == NULL)
        goto defer;
    b.fullmoves = atoi(fullmoves);

defer:
    b.zobrist_hash = getZobristHash(&b);
    return b;
}

void printBoard(const Board b)
{
    for (int rank = 7; rank >= 0; rank--) {
        printf("   +---+---+---+---+---+---+---+---+\n");
        printf(" %d |", rank + 1);
        for (int file = 0; file < 8; file++) {
            printf(" %c |", pieceToNotation(b.pieces[rank * 8 + file]));
        }
        printf("\n");
    }
    printf("   +---+---+---+---+---+---+---+---+\n");
    printf("  ");
    for (char file = 'a'; file <= 'h'; file++) {
        printf("   %c", file);
    }
    printf("\n");

    const char *epsquare_name = (b.ep_square == -1) ? "-" : SQNAMES[b.ep_square];
    printf(
        "turn: %c, castle rights: %04llu, ep square: %s, halfmove_clock: "
        "%d, fullmoves: %d, king_squares: [%d %d], zobrist hash: %llu\n",
        (b.color_to_move & WHITE) ? 'w' : 'b',
        decToBin(b.castle_rights),
        epsquare_name,
        b.halfmove_clock,
        b.fullmoves,
        b.king_squares[0],
        b.king_squares[1],
        b.zobrist_hash
    );
}


// Hashes a chess position
uint64_t getZobristHash(const Board *b)
{
    uint64_t hash = 0;

    // Hash piece positions
    for (int sq = 0; sq < 64; sq++) {
        if (b->pieces[sq] == EMPTY_PIECE)
            continue;
        int col_idx = (b->pieces[sq] & WHITE) ? 0 : 1;
        int piece_idx = getPieceIdx(b->pieces[sq]);
        hash ^= ZOBRIST.pieces[col_idx][piece_idx][sq];
    }

    // Hash black's turn to move
    if (b->color_to_move == BLACK)
        hash ^= ZOBRIST.black;

    // Hash castle rights
    hash ^= ZOBRIST.castles[b->castle_rights];

    // Hash ep square file if any
    if (b->ep_square != -1)
        hash ^= ZOBRIST.ep_square[b->ep_square];

    return hash;
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
        int offset = DIRECTION_OFFSETS[direction];
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
    Direction forward = PAWN_FORWARDS[color];
    Direction diagonals[2];
    diagonals[0] = PAWN_DIAGNOALS[color][0];
    diagonals[1] = PAWN_DIAGNOALS[color][1];

    // Forward moves (quiet or promotions)
    int forward_moves = (rank == PAWN_INITIAL_RANK[color]) ? 2 : 1;
    forward_moves = MIN(forward_moves, SQUARES_TILL_EDGE[src_sq][forward]);
    for (int n = 1; n <= forward_moves; n++) {
        int dst_sq = src_sq + DIRECTION_OFFSETS[forward] * n;
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

        int dst_sq = src_sq + DIRECTION_OFFSETS[direction];
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

        int dst_sq = src_sq + DIRECTION_OFFSETS[direction];
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
        int offset = DIRECTION_OFFSETS[direction];
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
        PAWN_DIAGNOALS[color][0],
        PAWN_DIAGNOALS[color][1],
    };

    for (int i = 0; i < 2; i++) {
        if (SQUARES_TILL_EDGE[src_sq][diagonals[i]] != 0) {
            int dst_sq = src_sq + DIRECTION_OFFSETS[diagonals[i]];
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
    int pawn_forward_offset = DIRECTION_OFFSETS[PAWN_FORWARDS[col_idx]];
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

// Compares two moves based on their flags
// used for sorting moves
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

void orderMoves(MoveList *mlist)
{
    qsort(mlist->moves, mlist->count, sizeof(Move), compareMove);
}

void printMoveList(const MoveList move_list)
{
    char str[20];
    printf("Total moves: %lu\n", move_list.count);
    for (size_t i = 0; i < move_list.count; i++) {
        Move m = move_list.moves[i];
        printMoveToString(m, str, true);
        printf("%s ", str);
    }
    printf("\n");
}

uint64_t generateTillDepth(Board b, int depth, bool show_move)
{
    if (depth == 0)
        return 1;

    int total = 0;
    char move_str[20];
    MoveList list = generateMoves(&b);

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

    MoveList mlist = generateMoves(b);
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
    MoveList mlist = generateMoves(b);
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

// Generates random 64 bit integer (assuming srand() is called)
uint64_t rand64(void)
{
    uint64_t r1 = rand(); // assuming RAND_MAX == 2^32-1
    uint64_t r2 = rand();
    return (r1 << 32) | (r2);
}

