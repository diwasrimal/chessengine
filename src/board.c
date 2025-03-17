#include "board.h"
#include "piece.h"
#include "utils.h"
#include "zobrist.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    strncpy(fen, starting_fen, sizeof(fen));

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

void printBoardFenToString(char *str, int max_str_size, const Board *b)
{
    // Ex: starting postion fen: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    char arrangement[100];
    int i = -1;

    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            Piece p = b->pieces[sq];
            if (p == EMPTY_PIECE) {
                empty++;
            } else {
                if (empty > 0) {
                    arrangement[++i] = empty + '0';
                    empty = 0;
                }
                arrangement[++i] = pieceToNotation(p);
            }
        }
        if (empty > 0)
            arrangement[++i] = empty + '0';
        arrangement[++i] = '/';
    }
    arrangement[i] = '\0'; // remove last '/' and terminate the string

    char turn = (b->color_to_move & WHITE ? 'w' : 'b');

    char castles[5];
    i = -1;
	if (b->castle_rights == NO_CASTLE) {
		castles[++i] = '-';
	} else {
		if (b->castle_rights & WKSC) castles[++i] = 'K';
		if (b->castle_rights & WQSC) castles[++i] = 'Q';
		if (b->castle_rights & BKSC) castles[++i] = 'k';
		if (b->castle_rights & BQSC) castles[++i] = 'q';
	}
    castles[++i] = '\0';


    char ep_square[3];
    i = -1;
    if (b->ep_square == -1) {
        ep_square[++i] = '-';
    } else {
		ep_square[++i] = b->ep_square % 8 + 'a'; // file
		ep_square[++i] = b->ep_square / 8 + '1'; // rank
    }
    ep_square[++i] = '\0';

    // "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    snprintf(str, max_str_size,  "%s %c %s %s %d %d", arrangement, turn, castles, ep_square, b->halfmove_clock, b->fullmoves);
}

