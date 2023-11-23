#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX(x, y) ((x > y) ? x : y)
#define MIN(x, y) ((x < y) ? x : y)

// 8 bits to represent a colored piece
//      . .         . . . . . .
//      ^ ^         ^ ^ ^ ^ ^ ^
//     /   \        | | | | | |
//   black white    k q b n r p
typedef uint8_t Piece;
const Piece EMPTY_PIECE = 0;
const Piece PAWN = 1 << 0;
const Piece ROOK = 1 << 1;
const Piece KNIGHT = 1 << 2;
const Piece BISHOP = 1 << 3;
const Piece QUEEN = 1 << 4;
const Piece KING = 1 << 5;
const Piece WHITE = 1 << 6;
const Piece BLACK = 1 << 7;

// 4 bits in format W W B B represent castling rights
//                  ^ ^
//                 /   \
//           kingside   queenside
typedef uint8_t CastleRight;
const CastleRight NO_CASTLE = 0;
const CastleRight BQSC = 1 << 0; // Black Queen Side Castle
const CastleRight BKSC = 1 << 1;
const CastleRight WQSC = 1 << 2;
const CastleRight WKSC = 1 << 3;

// 4 bits to represent type of move
//           . . . .
//           ^ ^ ^^^
//          /   \  other
//    promotion  capture
//
typedef uint8_t MoveFlag;
const MoveFlag QUIET = 0;
const MoveFlag DOUBLE_PAWN_PUSH = 0b0001;
const MoveFlag KING_CASTLE = 0b0010;
const MoveFlag QUEEN_CASTLE = 0b0011;
const MoveFlag CAPTURE = 0b0100;
const MoveFlag EP_CAPTURE = 0b0101;
const MoveFlag PROMOTION = 0b1000;
const MoveFlag KNIGHT_PROMOTION = 0b00 | PROMOTION;
const MoveFlag BISHOP_PROMOTION = 0b01 | PROMOTION;
const MoveFlag ROOK_PROMOTION = 0b10 | PROMOTION;
const MoveFlag QUEEN_PROMOTION = 0b11 | PROMOTION;
const MoveFlag KNIGHT_PROMO_CAPTURE = KNIGHT_PROMOTION | CAPTURE;
const MoveFlag BISHOP_PROMO_CAPTURE = BISHOP_PROMOTION | CAPTURE;
const MoveFlag ROOK_PROMO_CAPTURE = ROOK_PROMOTION | CAPTURE;
const MoveFlag QUEEN_PROMO_CAPTURE = QUEEN_PROMOTION | CAPTURE;

// 4 bits for flag, 6, 6 for src and dst squares
// FFFFSSSSSSDDDDDD
typedef uint16_t Move;
const Move EMPTY_MOVE = 0;
const Move MFLAG_MASK = 15 << 12;
const Move SRC_SQ_MASK = 63 << 6;
const Move DST_SQ_MASK = 63;

// Suppose 256 as maximum number of moves
typedef struct {
    Move moves[256];
    size_t count;
} MoveList;

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

typedef struct {
    Piece pieces[64];
    Piece color_to_move;
    CastleRight castle_rights;
    int ep_square;
    int halfmove_clock;
    int fullmoves;
    int king_squares[2];
} Board;

// Mapping of a square's index to its name
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

void startInteractiveGame(char *fen);
int squareNameToIdx(char *name);
char pieceToNotation(const Piece p);
bool isValidSquare(int rank, int file);
bool haveSameColor(Piece p1, Piece p2);
uint64_t decToBin(int n);
Board initBoardFromFen(char *starting_fen);
void printBoard(const Board b);
bool isKingChecked(const Board *b, Piece color);
MoveList generateMoves(const Board *b);
void fillSlidingMoves(const Board *b, int src_sq, MoveList *list);
void fillPawnMoves(const Board *b, int src_sq, MoveList *list);
void fillKnightMoves(const Board *b, int src_sq, MoveList *list);
void fillKingMoves(const Board *b, int src_sq, MoveList *list);
uint64_t generateSlidingAttackMap(const Board *b, int src_sq);
uint64_t generatePawnAttackMap(const Board *b, int src_sq);
uint64_t generateAttackMap(const Board *b, Piece attacking_color);
Move moveEncode(MoveFlag flag, int src_sq, int dst_sq);
Board moveMake(Move m, Board b);
void printMoveToString(Move m, char *str, bool print_flag);
int compareMove(const void *m1, const void *m2);
void orderMoves(MoveList *mlist);
void printMoves(const MoveList move_list);
uint64_t generateTillDepth(Board b, int depth, bool show_move);
void testMoveGeneration(void);
void testPerformance(void);
void testIsKingChecked(void);
void populateSquaresTillEdges(void);
void populateAttackMaps(void);
void precompute(void);
int evaluateBoard(const Board *b);
Move findBestMove(const Board *b);
int bestEvaluation(const Board *b, int depth, bool is_maximizing, int alpha, int beta);
int bestEvaluationRaw(const Board *b, int depth, bool is_maximizing);

int main(int argc, char **argv)
{
    char *starting_fen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    char *fen = (argc >= 2) ? argv[1] : starting_fen;

    precompute();

    // startInteractiveGame(fen);

    Board b = initBoardFromFen(fen);
    printBoard(b);
    MoveList moves = generateMoves(&b);
    printMoves(moves);

    Move best_move = findBestMove(&b);
    char move_str[15];
    printMoveToString(best_move, move_str, true);
    printf("Best move is: %s\n", move_str);

    // testIsKingChecked();
    // testPerformance();
    // testMoveGeneration();
}

void startInteractiveGame(char *fen)
{
    printf("Starting ineractive game, FEN: %s\n", fen);
    Board b = initBoardFromFen(fen);
    char move_str[16], move_inp[16];

    while (true) {
        printBoard(b);
        MoveList mlist = generateMoves(&b);
        if (mlist.count == 0) {
            if (isKingChecked(&b, b.color_to_move))
                printf("Checkmate!!\n");
            else
                printf("No valid moves!\n");
            break;
        }
        printMoves(mlist);

        // Get a valid move from user and make move
        Move move_to_make = EMPTY_MOVE;
        do {
            printf("Move: ");
            scanf("%s", move_inp);
            for (size_t i = 0; i < mlist.count; i++) {
                printMoveToString(mlist.moves[i], move_str, false);
                if (strcmp(move_str, move_inp) == 0) {
                    move_to_make = mlist.moves[i];
                    break;
                }
            }
        } while (move_to_make == EMPTY_MOVE);
        b = moveMake(move_to_make, b);
    }
}

int squareNameToIdx(char *name)
{
    char file = name[0] - 'a';
    char rank = name[1] - '1';
    return rank * 8 + file;
}

char pieceToNotation(const Piece p)
{
    char notation;
    if (p == EMPTY_PIECE)
        return ' ';

    if (p & PAWN)
        notation = 'P';
    else if (p & ROOK)
        notation = 'R';
    else if (p & KNIGHT)
        notation = 'N';
    else if (p & BISHOP)
        notation = 'B';
    else if (p & QUEEN)
        notation = 'Q';
    else if (p & KING)
        notation = 'K';

    return (p & BLACK) ? tolower(notation) : notation;
}

bool isValidSquare(int rank, int file)
{
    return 0 <= rank && rank < 8 && 0 <= file && file < 8;
}

bool haveSameColor(Piece p1, Piece p2)
{
    // First 6 bits denote piece type, remove them and compare
    return ((p1 >> 6) == (p2 >> 6));
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

Board initBoardFromFen(char *starting_fen)
{
    populateSquaresTillEdges();

    // Initialize with default values
    Board b = {
        .castle_rights = NO_CASTLE,
        .ep_square = -1,
        .halfmove_clock = 0,
        .fullmoves = 1,
        .king_squares = {-1},
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
            b.pieces[sq] = types[(int)c];
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
    printf("turn: %c, castle rights: %04llu, ep square: %s, halfmove_clock: "
           "%d, fullmoves: %d, king_squares: [%d %d]\n",
           ((b.color_to_move & WHITE) ? 'w' : 'b'), decToBin(b.castle_rights),
           epsquare_name, b.halfmove_clock, b.fullmoves, b.king_squares[0],
           b.king_squares[1]);
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
        if (!isValidSquare(r, f) ||
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

Move moveEncode(MoveFlag flag, int src_sq, int dst_sq)
{
    return ((Move)flag << 12) | ((Move)src_sq << 6) | dst_sq;
}

MoveFlag getMoveFlag(Move m) { return (m & MFLAG_MASK) >> 12; }

int getMoveSrc(Move m) { return (m & SRC_SQ_MASK) >> 6; }

int getMoveDst(Move m) { return m & DST_SQ_MASK; }

Board moveMake(Move m, Board b)
{
    MoveFlag flag = getMoveFlag(m);
    int src_sq = getMoveSrc(m);
    int dst_sq = getMoveDst(m);
    int col_idx = (b.pieces[src_sq] & WHITE) ? 0 : 1;
    int opp_col_idx = 1 - col_idx;

    // Record / reset en passantable square
    int pawn_forward_offset = DIRECTION_OFFSETS[PAWN_FORWARDS[col_idx]];
    if (flag == DOUBLE_PAWN_PUSH) {
        b.ep_square = dst_sq - pawn_forward_offset;
    }
    else
        b.ep_square = -1;

    // Capture pawn below dst square during en passant
    if (flag == EP_CAPTURE)
        b.pieces[dst_sq - pawn_forward_offset] = EMPTY_PIECE;

    // Revoke castle rights if king is moving
    // and update king square
    if (b.pieces[src_sq] & KING) {
        b.castle_rights &= CRIGHT_REVOKING_MASK[col_idx];
        b.king_squares[col_idx] = dst_sq;
    }

    // Move rook when castled
    if (flag == QUEEN_CASTLE) {
        b.pieces[QSC_ROOK_DST_SQ[col_idx]] = b.pieces[QSC_ROOK_SRC_SQ[col_idx]];
        b.pieces[QSC_ROOK_SRC_SQ[col_idx]] = EMPTY_PIECE;
    }
    else if (flag == KING_CASTLE) {
        b.pieces[KSC_ROOK_DST_SQ[col_idx]] = b.pieces[KSC_ROOK_SRC_SQ[col_idx]];
        b.pieces[KSC_ROOK_SRC_SQ[col_idx]] = EMPTY_PIECE;
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

    // Update or reset halfmove clock
    b.halfmove_clock++;
    if (flag & CAPTURE || b.pieces[src_sq] & PAWN)
        b.halfmove_clock = 0;

    // Move src piece to dst
    Piece dst_piece = b.pieces[src_sq];

    // Dst piece changes during promotion
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
        dst_piece = b.color_to_move | promoted_piece;
    }

    b.pieces[dst_sq] = dst_piece;
    b.pieces[src_sq] = EMPTY_PIECE;

    // Change turn and update fullmoves
    if (b.color_to_move == WHITE) {
        b.color_to_move = BLACK;
    }
    else {
        b.color_to_move = WHITE;
        b.fullmoves++;
    }

    return b;
}

void printMoveToString(Move m, char *str, bool print_flag)
{
    MoveFlag flag = getMoveFlag(m);
    int src_sq = getMoveSrc(m);
    int dst_sq = getMoveDst(m);
    const char *src_sqname = SQNAMES[src_sq];
    const char *dst_sqname = SQNAMES[dst_sq];

    char *promo_type = "";
    if (flag == QUEEN_PROMOTION || flag == QUEEN_PROMO_CAPTURE)
        promo_type = "q";
    else if (flag == KNIGHT_PROMOTION || flag == KNIGHT_PROMO_CAPTURE)
        promo_type = "n";
    else if (flag == BISHOP_PROMOTION || flag == BISHOP_PROMO_CAPTURE)
        promo_type = "b";
    else if (flag == ROOK_PROMOTION || flag == ROOK_PROMO_CAPTURE)
        promo_type = "r";

    if (print_flag)
        sprintf(str, "%s%s%s[%04llu]", src_sqname, dst_sqname, promo_type, decToBin(flag));
    else
        sprintf(str, "%s%s%s", src_sqname, dst_sqname, promo_type);
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

void printMoves(const MoveList move_list)
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

void testMoveGeneration(void)
{
    printf("\ntestMoveGeneration()\n");
    struct PositionData {
        char *fen;
        uint64_t nodes[30];
        int depth;
    };

    // https://www.chessprogramming.org/Perft_Results
    const int n = 5;
    struct PositionData positions[n] = {
        {
            .fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            .depth = 15,
            .nodes =
            {
                20,
                400,
                8902,
                197281,
                4865609,
                119060324,
                3195901860,
                84998978956,
                2439530234167,
                69352859712417,
                2097651003696806,
            },
        },
        {
            .fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w "
                "KQkq - ",
            .depth = 6,
            .nodes =
            {
                48,
                2039,
                97862,
                4085603,
                193690690,
                8031647685,
            },
        },
        {
            .fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
            .depth = 8,
            .nodes =
            {
                14,
                191,
                2812,
                43238,
                674624,
                11030083,
                178633661,
                3009794393,
            },
        },
        {
            .fen = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq "
                "- 0 1",
            .depth = 6,
            .nodes =
            {
                6,
                264,
                9467,
                422333,
                15833292,
                706045033,
            },
        },
        {
            .fen = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8 ",
            .depth = 5,
            .nodes =
            {
                44,
                1486,
                62379,
                2103487,
                89941194,
            },
        }};

    const int max_depth = 5;
    for (int i = 0; i < n; i++) {
        struct PositionData pos = positions[i];
        printf("pos: %d, fen: %s\n", i + 1, pos.fen);
        Board b = initBoardFromFen(pos.fen);
        for (int d = 1; d <= max_depth && d <= pos.depth; d++) {
            uint64_t nodes = generateTillDepth(b, d, false);
            printf("\t[%s]: depth: %d, nodes: %10llu, calculated: %10llu\n",
                    nodes == pos.nodes[d - 1] ? "pass" : "FAIL", d,
                    pos.nodes[d - 1], nodes);
        }
    }
}

void testPerformance(void)
{
    char *fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    printf("\nTesting perfomance on fen: %s\n", fen);
    Board b = initBoardFromFen(fen);
    int max_depth = 6;
    for (int d = 1; d <= max_depth; d++) {
        clock_t start = clock();
        uint64_t total = generateTillDepth(b, d, false);
        clock_t diff = clock() - start;
        double ms = (double)diff * 1000 / (double)CLOCKS_PER_SEC;
        printf("Depth %d, moves: %10llu, time: %lf ms\n", d, total, ms);
    }
}

void testIsKingChecked(void)
{
    printf("\ntestIsKingChecked()\n");
    struct PositionData {
        char *fen;
        bool checked[2]; // idx 0 means white king is checked, 1 means black
    };

    const int n = 6;
    struct PositionData positions[n] = {
        {.fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            .checked = {false, false}},
        {
            .fen =
                "rnbqkb1r/ppNpp1pp/5n2/5p2/8/8/PPPPPPPP/R1BQKBNR b KQkq - 0 1",
            .checked = {false, true} // meaning black is checked
        },
        {
            .fen = "rnb1kb1r/ppppqppp/8/8/8/5n2/PPPP1PPP/RNBQKBNR w KQkq - 0 1",
            .checked = {true, false},
        },
        {
            .fen =
                "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 1",
            .checked = {true, false},
        },
        {
            .fen = "1n2k1nr/1ppp1ppp/5N2/4p3/1b1P2Pq/N5b1/1PPQPP1P/r1B1KB1R b "
                "Kk - 0 1",
            .checked = {false, true},
        },
        {.fen = "1n2k1nr/1ppp1ppp/8/3Np2B/1b1P2Pq/N5b1/1PPQPP1P/r1B1K2R b Kk - "
            "0 1",
            .checked = {false, false}},
    };

    for (int i = 0; i < n; i++) {
        struct PositionData pos = positions[i];
        printf("pos: %d, fen: %s\n", i, pos.fen);
        Board b = initBoardFromFen(pos.fen);
        bool result[2] = {
            isKingChecked(&b, WHITE),
            isKingChecked(&b, BLACK),
        };
        printf("\t[%s]: white was checked: %d, result: %d\n",
                pos.checked[0] == result[0] ? "pass" : "FAIL", pos.checked[0],
                result[0]);
        printf("\t[%s]: black was checked: %d, result: %d\n",
                pos.checked[1] == result[1] ? "pass" : "FAIL", pos.checked[1],
                result[1]);
    }
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
                if (isValidSquare(r, f)) {
                    int dst_sq = r * 8 + f;
                    KNIGHT_ATTACK_MAPS[src_sq] |= 1ull << dst_sq;
                }
            }
        }
    }
}

void precompute(void)
{
    populateSquaresTillEdges();
    populateAttackMaps();
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

    for (size_t i = 0; i < mlist.count; i++) {
        // printMoveToString(mlist.moves[i], move_str, true);
        Board updated = moveMake(mlist.moves[i], *b);
        if (is_maximizing) {
            // int score = bestEvaluationRaw(&updated, minimax_depth - 1, false);
            int score = bestEvaluation(&updated, minimax_depth - 1, false, alpha, beta);
            if (LOG_SEARCH)
                printf("Move: %s, is_maximizing: %d, score: %d, best_score: %d\n", move_str, is_maximizing, score, best_score);
            if (score > best_score) {
                best_score = score;
                best_move = mlist.moves[i];
            }
        } else {
            // int score = bestEvaluationRaw(&updated, minimax_depth - 1, false);
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
            if (score > beta) {
                if (LOG_SEARCH) printf("score >= beta, pruning...");
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

int bestEvaluationRaw(const Board *b, int depth, bool is_maximizing)
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
            int score = bestEvaluationRaw(&updated, depth - 1, false);
            if (LOG_SEARCH) {
                for (int i = 0; i < 3 - depth; i++) printf("    ");
                printf("Move: %s, is_maximizing: %d, score: %d, best_score: %d\n", move_str, is_maximizing, score, best_score);
            }
            if (score > best_score) {
                best_score = score;
            }
        } else {
            int score = bestEvaluationRaw(&updated, depth - 1, true);
            if (LOG_SEARCH) {
                for (int i = 0; i < 3 - depth; i++) printf("    ");
                printf("Move: %s, is_maximizing: %d, score: %d, best_score: %d\n", move_str, is_maximizing, score, best_score);
            }
            if (score < best_score) {
                best_score = score;
            }
        }
    }

    return best_score;
}

