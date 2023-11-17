#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(x, y) ((x > y) ? x : y)
#define MIN(x, y) ((x < y) ? x : y)

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

typedef uint8_t CastleRight;
const CastleRight NO_CASTLE = 0;
const CastleRight BQSC = 1 << 0; // Black Queen Side Castle
const CastleRight BKSC = 1 << 1;
const CastleRight WQSC = 1 << 2;
const CastleRight WKSC = 1 << 3;

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

// Max 64 squares could be attacked
typedef struct {
    int attacks[64];
} AttackMap;

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
} Board;

typedef unsigned long long Ull;

int squareNameToIdx(char *name);
void fillSquareName(int square, char *name);
char pieceToNotation(const Piece p);
bool isValidSquare(int rank, int file);
bool haveSameColor(Piece p1, Piece p2);
Ull decToBin(int n);
void populateSquaresTillEdges(void);
Board initBoardFromFen(char *starting_fen);
void printBoard(const Board b);
bool isKingChecked(Board b);
MoveList generateMoves(const Board b);
MoveList generateSingleMoves(const Board b, int src_sq);
void fillSlidingMoves(const Board b, int src_sq, MoveList *list);
void fillPawnMoves(const Board b, int src_sq, MoveList *list);
void fillKnightMoves(const Board b, int src_sq, MoveList *list);
void fillKingMoves(const Board b, int src_sq, MoveList *list);
void fillAttacks(const Board b, int *attacks);
void fillSingleAttacks(const Board b, int src_sq, int *attacks);
void fillSlidingAttacks(const Board b, int src_sq, int *attacks);
void fillPawnAttacks(const Board b, int src_sq, int *attacks);
void fillKnightAttacks(int src_sq, int *attacks);
void fillKingAttacks(int src_sq, int *attacks);
MoveFlag getMoveFlag(Move m);
int getMoveSrc(Move m);
int getMoveDst(Move m);
Move moveEncode(MoveFlag flag, int src_sq, int dst_sq);
Board moveMake(Move m, Board b);
void printMoves(const MoveList move_list);
int testGenerationTillDepth(Board b, int depth);

int DEPTH = 5;

int main(int argc, char **argv)
{
    char *starting_fen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    char *fen = (argc >= 2) ? argv[1] : starting_fen;
    printf("Using FEN: %s\n", fen);

    Board b = initBoardFromFen(fen);
    printBoard(b);

    // MoveList moves = generateMoves(b);
    // printMoves(moves);

    int total = testGenerationTillDepth(b, DEPTH);
    printf("Depth %d, moves: %d\n", DEPTH, total);
}

int squareNameToIdx(char *name)
{
    char file = name[0] - 'a';
    char rank = name[1] - '1';
    return rank * 8 + file;
}

void fillSquareName(int square, char *name)
{
    if (!isValidSquare(square / 8, square % 8)) {
        sprintf(name, "-");
    }
    else {
        char rank = '1' + (square / 8);
        char file = 'a' + (square % 8);
        sprintf(name, "%c%c", file, rank);
    }
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

Ull decToBin(int n)
{
    int bin[32];
    int i = 0;
    while (n > 0) {
        bin[i] = n % 2;
        n = n / 2;
        i++;
    }

    Ull result = 0;
    for (int j = i - 1; j >= 0; j--)
        result = result * 10 + bin[j];

    return result;
}

// Finds squares between a square and board's edge in all possible directions
// Called only once during board initialization
int SQUARES_TILL_EDGE[64][8];
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

Board initBoardFromFen(char *starting_fen)
{
    populateSquaresTillEdges();
    Board b;

    Piece types[256];
    types['K'] = KING;
    types['Q'] = QUEEN;
    types['N'] = KNIGHT;
    types['B'] = BISHOP;
    types['R'] = ROOK;
    types['P'] = PAWN;

    char fen[100];
    strcpy(fen, starting_fen);

    // Piece arrangement
    char *arrangement = strtok(fen, " ");
    if (arrangement == NULL)
        assert(0 && "Insufficient FEN string information");

    size_t n = strlen(arrangement);
    int rank = 7, file = 0;
    for (size_t i = 0; i < n; i++) {
        char c = arrangement[i];
        if (c == '/') {
            rank--;
            file = 0;
        }
        else if (isalpha(c)) {
            char uppercased = toupper(c);
            b.pieces[rank * 8 + file] =
                types[(int)uppercased] | (islower(c) ? BLACK : WHITE);
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

    char *turn = strtok(NULL, " ");
    if (turn == NULL)
        assert(0 && "Insufficient FEN string information");
    b.color_to_move = (turn[0] == 'b' ? BLACK : WHITE);

    char *rights = strtok(NULL, " ");
    if (rights == NULL)
        assert(0 && "Insufficient FEN string information");

    b.castle_rights = 0;
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

    char *ep_square = strtok(NULL, " ");
    if (ep_square == NULL)
        assert(0 && "Insufficient FEN string information");
    b.ep_square = -1;
    if (ep_square[0] != '-') {
        b.ep_square = squareNameToIdx(ep_square);
    }

    b.halfmove_clock = 0;
    char *halfmove_clock = strtok(NULL, " ");
    if (halfmove_clock == NULL)
        assert(0 && "Insufficient FEN string information");
    b.halfmove_clock = atoi(halfmove_clock);

    b.fullmoves = 1;
    char *fullmoves = strtok(NULL, " ");
    if (fullmoves == NULL)
        assert(0 && "Insufficient FEN string information");
    b.fullmoves = atoi(fullmoves);

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

    char epsquare_name[3];
    fillSquareName(b.ep_square, epsquare_name);
    printf("turn: %c, castle rights: %04llu, ep square: %s, halfmove_clock: "
           "%d, fullmoves: %d\n",
           ((b.color_to_move & WHITE) ? 'w' : 'b'), decToBin(b.castle_rights),
           epsquare_name, b.halfmove_clock, b.fullmoves);
}

// Given a board, finds if opposing king is in check
bool isKingChecked(Board b)
{
    int king_sq = -1;
    for (int src_sq = 0; src_sq < 64; src_sq++) {
        if (b.pieces[src_sq] & KING &&
            !haveSameColor(b.color_to_move, b.pieces[src_sq])) {
            king_sq = src_sq;
            break;
        }
    }

    if (king_sq == -1)
        assert(0 && "King not found!");

    // Set turn to opposite color to find attacks to our color
    int attacks[64] = {0};
    fillAttacks(b, attacks);

    // printf("\nAttacks on isKingChecked\n");
    // for (int r = 7; r >= 0; r--) {
    //     for (int f = 0; f < 8; f++) {
    //         printf("%d ", attacks[r * 8 + f]);
    //     }
    //     printf("\n");
    // }

    if (attacks[king_sq] > 0)
        return true;

    return false;
}

MoveList generateMoves(const Board b)
{
    MoveList pseudolegals = {.count = 0};

    for (int src_sq = 0; src_sq < 64; src_sq++) {
        if (b.pieces[src_sq] == EMPTY_PIECE ||
            !haveSameColor(b.color_to_move, b.pieces[src_sq]))
            continue;

        MoveList list = generateSingleMoves(b, src_sq);
        for (size_t i = 0; i < list.count; i++) {
            pseudolegals.moves[pseudolegals.count++] = list.moves[i];
        }
    }

    // Filter moves that will leave our king in check
    MoveList legalmoves = {.count = 0};
    for (size_t i = 0; i < pseudolegals.count; i++) {
        Move m = pseudolegals.moves[i];
        Board updated = moveMake(m, b);
        if (isKingChecked(updated))
            continue;

        legalmoves.moves[legalmoves.count++] = m;
    }

    return legalmoves;
}

MoveList generateSingleMoves(const Board b, int src_sq)
{
    MoveList list = {.count = 0};

    if (b.pieces[src_sq] & (ROOK | QUEEN | BISHOP)) {
        fillSlidingMoves(b, src_sq, &list);
    }
    else if (b.pieces[src_sq] & PAWN) {
        fillPawnMoves(b, src_sq, &list);
    }
    else if (b.pieces[src_sq] & KNIGHT) {
        fillKnightMoves(b, src_sq, &list);
    }
    else if (b.pieces[src_sq] & KING) {
        fillKingMoves(b, src_sq, &list);
    }

    return list;
}

void fillSlidingMoves(const Board b, int src_sq, MoveList *list)
{
    // Rook only moves straight -> first 4 directions
    // Bishop only moves diagnoals -> 4 - 8 directins
    // Queen goes all directions
    int start = 0, end = 8;
    if (b.pieces[src_sq] & ROOK)
        end = 4;
    else if (b.pieces[src_sq] & BISHOP)
        start = 4;

    for (int direction = start; direction < end; direction++) {
        int offset = DIRECTION_OFFSETS[direction];
        for (int n = 0; n < SQUARES_TILL_EDGE[src_sq][direction]; n++) {

            int dst_sq = src_sq + offset * (n + 1);

            // Path blocked by own piece
            if (haveSameColor(b.pieces[dst_sq], b.pieces[src_sq]))
                break;

            if (b.pieces[dst_sq] != EMPTY_PIECE) {
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

// Idx 0 for white , 1 for black
const int PAWN_PROMOTING_RANK[2] = {7, 0};
const int PAWN_INITIAL_RANK[2] = {1, 6};
const Direction PAWN_FORWARDS[2] = {UP, DOWN};
const Direction PAWN_DIAGNOALS[2][2] = {{TOPLEFT, TOPRIGHT},
                                        {BOTLEFT, BOTRIGHT}};

void fillPawnMoves(const Board b, int src_sq, MoveList *list)
{
    int color = (b.pieces[src_sq] & WHITE) ? 0 : 1;
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
        if (b.pieces[dst_sq] != EMPTY_PIECE)
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
        if (dst_sq == b.ep_square) {
            list->moves[list->count++] = moveEncode(EP_CAPTURE, src_sq, dst_sq);
            continue;
        }

        // Only captures are possible on diagnonals (except en passant)
        if (b.pieces[dst_sq] == EMPTY_PIECE ||
            haveSameColor(b.pieces[src_sq], b.pieces[dst_sq]))
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

const int KNIGHT_RANK_OFFSETS[8] = {2, 2, -2, -2, 1, 1, -1, -1};
const int KNIGHT_FILE_OFFSETS[8] = {-1, 1, -1, 1, -2, 2, -2, 2};

void fillKnightMoves(const Board b, int src_sq, MoveList *list)
{
    int rank = src_sq / 8;
    int file = src_sq % 8;

    for (int i = 0; i < 8; i++) {
        int r = rank + KNIGHT_RANK_OFFSETS[i];
        int f = file + KNIGHT_FILE_OFFSETS[i];
        int dst_sq = r * 8 + f;

        // Invalid square or same colored piece
        if (!isValidSquare(r, f) ||
            haveSameColor(b.pieces[src_sq], b.pieces[dst_sq]))
            continue;

        MoveFlag flag = (b.pieces[dst_sq] != EMPTY_PIECE) ? CAPTURE : QUIET;
        list->moves[list->count++] = moveEncode(flag, src_sq, dst_sq);
    }
}

// Squares checked during castle
const int QSC_SQ[2][3] = {{1, 2, 3}, {57, 58, 59}};
const int KSC_SQ[2][2] = {{5, 6}, {61, 62}};

const int QSC_FLAGS[2] = {WQSC, BQSC};
const int KSC_FLAGS[2] = {WKSC, BKSC};

// Squares the king moves to during castle
const int QS_DST_SQ[2] = {2, 58};
const int KS_DST_SQ[2] = {6, 62};

void fillKingMoves(const Board b, int src_sq, MoveList *list)
{
    // Normal moves
    for (int direction = 0; direction < 8; direction++) {
        if (SQUARES_TILL_EDGE[src_sq][direction] == 0)
            continue;

        int dst_sq = src_sq + DIRECTION_OFFSETS[direction];
        if (haveSameColor(b.pieces[src_sq], b.pieces[dst_sq]))
            continue;

        MoveFlag flag = (b.pieces[dst_sq] != EMPTY_PIECE) ? CAPTURE : QUIET;
        list->moves[list->count++] = moveEncode(flag, src_sq, dst_sq);
    }

    // Castle not available
    if (b.castle_rights == NO_CASTLE)
        return;

    int color = (b.pieces[src_sq] & WHITE) ? 0 : 1;

    // Queen side castle
    if (b.castle_rights & QSC_FLAGS[color]) {
        bool possible = true;
        for (int i = 0; i < 3; i++) {
            int sq = QSC_SQ[color][i];
            if (b.pieces[sq] != EMPTY_PIECE) {
                possible = false;
                break;
            }
            // TODO: Check for attacked square too
        }
        if (possible)
            list->moves[list->count++] =
                moveEncode(QUEEN_CASTLE, src_sq, QS_DST_SQ[color]);
    }

    // King side castle
    if (b.castle_rights & KSC_FLAGS[color]) {
        bool possible = true;
        for (int i = 0; i < 2; i++) {
            int sq = KSC_SQ[color][i];
            if (b.pieces[sq] != EMPTY_PIECE) {
                possible = false;
                break;
            }
            // TODO: Check for attacked square too
        }
        if (possible)
            list->moves[list->count++] =
                moveEncode(KING_CASTLE, src_sq, KS_DST_SQ[color]);
    }
}

void fillAttacks(const Board b, int *attacks)
{
    for (int src_sq = 0; src_sq < 64; src_sq++) {
        if (b.pieces[src_sq] == EMPTY_PIECE ||
            !haveSameColor(b.color_to_move, b.pieces[src_sq]))
            continue;
        fillSingleAttacks(b, src_sq, attacks);
    }
}

void fillSingleAttacks(const Board b, int src_sq, int *attacks)
{
    if (b.pieces[src_sq] & (ROOK | QUEEN | BISHOP)) {
        fillSlidingAttacks(b, src_sq, attacks);
    }
    else if (b.pieces[src_sq] & PAWN) {
        fillPawnAttacks(b, src_sq, attacks);
    }
    else if (b.pieces[src_sq] & KNIGHT) {
        fillKnightAttacks(src_sq, attacks);
    }
    else if (b.pieces[src_sq] & KING) {
        fillKingAttacks(src_sq, attacks);
    }
}

void fillSlidingAttacks(const Board b, int src_sq, int *attacks)
{
    int start = 0, end = 8;
    if (b.pieces[src_sq] & ROOK)
        end = 4;
    else if (b.pieces[src_sq] & BISHOP)
        start = 4;

    for (int direction = start; direction < end; direction++) {
        int offset = DIRECTION_OFFSETS[direction];
        for (int n = 0; n < SQUARES_TILL_EDGE[src_sq][direction]; n++) {

            int dst_sq = src_sq + offset * (n + 1);
            attacks[dst_sq]++;

            // Further path blocked
            if (b.pieces[dst_sq] != EMPTY_PIECE)
                break;
        }
    }
}

void fillPawnAttacks(const Board b, int src_sq, int *attacks)
{
    // Pawn only attacks diagnoals
    int color = (b.pieces[src_sq] & WHITE) ? 0 : 1;
    Direction diagonals[2] = {
        PAWN_DIAGNOALS[color][0],
        PAWN_DIAGNOALS[color][1],
    };

    for (int i = 0; i < 2; i++) {
        if (SQUARES_TILL_EDGE[src_sq][diagonals[i]] != 0) {
            int dst_sq = src_sq + DIRECTION_OFFSETS[diagonals[i]];
            attacks[dst_sq]++;
        }
    }
}

void fillKnightAttacks(int src_sq, int *attacks)
{
    int rank = src_sq / 8;
    int file = src_sq % 8;

    for (int i = 0; i < 8; i++) {
        int r = rank + KNIGHT_RANK_OFFSETS[i];
        int f = file + KNIGHT_FILE_OFFSETS[i];
        if (isValidSquare(r, f)) {
            int dst_sq = r * 8 + f;
            attacks[dst_sq]++;
        }
    }
}

void fillKingAttacks(int src_sq, int *attacks)
{
    // King attacks everywhere
    for (int direction = 0; direction < 8; direction++) {
        if (SQUARES_TILL_EDGE[src_sq][direction] != 0) {
            int dst_sq = src_sq + DIRECTION_OFFSETS[direction];
            attacks[dst_sq]++;
        }
    }
}

Move moveEncode(MoveFlag flag, int src_sq, int dst_sq)
{
    return ((Move)flag << 12) | ((Move)src_sq << 6) | dst_sq;
}

MoveFlag getMoveFlag(Move m) { return (m & MFLAG_MASK) >> 12; }

int getMoveSrc(Move m) { return (m & SRC_SQ_MASK) >> 6; }

int getMoveDst(Move m) { return m & DST_SQ_MASK; }

// Rook's src and dst squares during castle
// idx 0 for white, 1 for black
const int QSC_ROOK_SRC_SQ[2] = {0, 56};
const int QSC_ROOK_DST_SQ[2] = {3, 59};
const int KSC_ROOK_SRC_SQ[2] = {7, 63};
const int KSC_ROOK_DST_SQ[2] = {5, 61};

Board moveMake(Move m, Board b)
{
    MoveFlag flag = getMoveFlag(m);
    int src_sq = getMoveSrc(m);
    int dst_sq = getMoveDst(m);
    int color = (b.pieces[src_sq] & WHITE) ? 0 : 1;

    // Record / reset en passantable square
    int pawn_forward_offset = DIRECTION_OFFSETS[PAWN_FORWARDS[color]];
    if (flag == DOUBLE_PAWN_PUSH) {
        b.ep_square = dst_sq - pawn_forward_offset;
    }
    else
        b.ep_square = -1;

    // Capture pawn below dst square during en passant
    if (flag == EP_CAPTURE)
        b.pieces[dst_sq - pawn_forward_offset] = EMPTY_PIECE;

    // Revoke castle rights if king is moving
    if (b.pieces[src_sq] & KING)
        b.castle_rights ^= (KSC_FLAGS[color] | QSC_FLAGS[color]);

    // Move rook when castled
    if (flag == QUEEN_CASTLE) {
        b.pieces[QSC_ROOK_DST_SQ[color]] = b.pieces[QSC_ROOK_SRC_SQ[color]];
        b.pieces[QSC_ROOK_SRC_SQ[color]] = EMPTY_PIECE;
    }
    else if (flag == KING_CASTLE) {
        b.pieces[KSC_ROOK_DST_SQ[color]] = b.pieces[KSC_ROOK_SRC_SQ[color]];
        b.pieces[KSC_ROOK_SRC_SQ[color]] = EMPTY_PIECE;
    }

    // Castle was available on a side, but that side's rook moves,
    // which revokes castling right on that side
    if (b.castle_rights & QSC_FLAGS[color] && b.pieces[src_sq] & ROOK &&
        src_sq == QSC_ROOK_SRC_SQ[color]) {
        b.castle_rights ^= QSC_FLAGS[color];
    }
    if (b.castle_rights & KSC_FLAGS[color] && b.pieces[src_sq] & ROOK &&
        src_sq == KSC_ROOK_SRC_SQ[color]) {
        b.castle_rights ^= KSC_FLAGS[color];
    }

    // Update or rest halfmove clock
    b.halfmove_clock++;
    if (flag & CAPTURE || b.pieces[src_sq] & PAWN)
        b.halfmove_clock = 0;

    // Move src piece to dst
    Piece dst_piece = b.pieces[src_sq];

    // Dst piece changes during promotion
    if (flag & PROMOTION) {
        Piece promoted_piece = (flag == KNIGHT_PROMOTION)   ? KNIGHT
                               : (flag == BISHOP_PROMOTION) ? BISHOP
                               : (flag == ROOK_PROMOTION)   ? ROOK
                                                            : QUEEN;
        dst_piece = b.color_to_move | promoted_piece;
    }

    b.pieces[dst_sq] = dst_piece;
    b.pieces[src_sq] = EMPTY_PIECE;

    // Change turn and update fullmoves
    b.color_to_move = (b.color_to_move & WHITE) ? BLACK : WHITE;
    if (b.color_to_move & WHITE)
        b.fullmoves++;

    return b;
}

void printMoves(const MoveList move_list)
{
    char src[3];
    char dst[3];
    printf("Total moves: %lu\n", move_list.count);
    for (size_t i = 0; i < move_list.count; i++) {
        Move m = move_list.moves[i];
        MoveFlag flag = getMoveFlag(m);
        int src_sq = getMoveSrc(m);
        int dst_sq = getMoveDst(m);
        fillSquareName(src_sq, src);
        fillSquareName(dst_sq, dst);
        printf("%s%s[%04llu] ", src, dst, decToBin(flag));
    }
    printf("\n");
}

int testGenerationTillDepth(Board b, int depth)
{
    if (depth == 0)
        return 1;

    int total = 0;
    char src[3], dst[3];
    MoveList list = generateMoves(b);

    for (size_t i = 0; i < list.count; i++) {
        Board new = moveMake(list.moves[i], b);
        int src_sq = getMoveSrc(list.moves[i]);
        int dst_sq = getMoveDst(list.moves[i]);
        fillSquareName(src_sq, src);
        fillSquareName(dst_sq, dst);
        int n_moves = testGenerationTillDepth(new, depth - 1);
        if (depth == DEPTH)
            printf("%s%s: %d\n", src, dst, n_moves);
        total += n_moves;
    }

    return total;
}
