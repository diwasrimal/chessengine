#include <stdio.h>
#include <string.h>

#include "engine.h"

void startInteractiveGame(char *fen);

int main(int argc, char **argv)
{
    char *starting_fen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    char *fen = (argc >= 2) ? argv[1] : starting_fen;

    precomputeValues();

    // startInteractiveGame(fen);

    Board b = initBoardFromFen(fen);
    printBoard(b);
    MoveList moves = generateMoves(&b);
    printMoveList(moves);

    Move best_move = findBestMove(&b);
    char move_str[15];
    printMoveToString(best_move, move_str, true);
    printf("Best move is: %s\n", move_str);

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
        printMoveList(mlist);

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
