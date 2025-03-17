#include "board.h"
#include "engine.h"

#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

void testIsKingChecked();
void testPerformance();
void testMoveGeneration();
void testZobristHashes();
void testFenGeneration();

int main(void)
{
    precomputeValues();

    testIsKingChecked();
	testFenGeneration();
    testZobristHashes();
    testMoveGeneration();
    testPerformance();
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
    struct PositionData positions[] = {
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

    const int n = sizeof(positions) / sizeof(positions[0]);
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
    printf("\ntestPerformance()\nUsing fen: %s\n", fen);
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

    struct PositionData positions[] = {
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

    const int n = sizeof(positions) / sizeof(positions[0]);
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

bool checkZobristTillDepth(const Board *b, int depth)
{
    if (depth == 0) {
        uint64_t calculated_hash = getZobristHash(b);
        if (b->zobrist_hash == calculated_hash)
            return true;
        printf("calculated_hash: %llu, b.zobrist_hash: %llu\n", calculated_hash, b->zobrist_hash);
        return false;
    }

    MoveList mlist = generateMoves(b);

    for (size_t i = 0; i < mlist.count; i++) {
        Board updated = moveMake(mlist.moves[i], *b);
        bool result = checkZobristTillDepth(&updated, depth - 1);
        if (!result) {
            char move_str[15];
            printMoveToString(move_str, sizeof(move_str), mlist.moves[i], true);
            printf("On move: %s, depth: %d\n", move_str, depth);
            return false;
        }
    }

    return true;
}

void testZobristHashes(void)
{
    printf("\ntestZobristHashes()\n");
    int depth = 4;
    char *fens[6] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "5r2/2p1p1N1/4P3/2R1R3/NK3pp1/5Pk1/2P1pp2/8 w - - 0 1",
        "6R1/5P2/3p4/2Qr4/1p1pp2p/p4P1R/KP1P4/4k3 w - - 0 1",
        "4kbnr/1pp2ppp/r2qb3/p2Pn3/2BP1B2/2N2N2/PPP1QPPP/R3R1K1 w k - 0 1",
        "8/4pP2/1k1N2qP/1n1P4/PpK2b1P/8/1rP4R/8 w - - 0 1",
        "r1bqkbnr/ppp2ppp/8/3Pn3/2B5/5N2/PPPPQPPP/RNB2RK1 b kq - 0 1",
    };

    for (size_t i = 0; i < 6; i++) {
        Board b = initBoardFromFen(fens[i]);
        MoveList mlist = generateMoves(&b);
        bool passed = true;
        for (size_t i = 0; i < mlist.count; i++) {
            Board updated = moveMake(mlist.moves[i], b);
            passed = checkZobristTillDepth(&updated, depth - 1);
            if (!passed) {
                char move_str[15];
                printMoveToString(move_str, sizeof(move_str), mlist.moves[i], true);
                printf("On move: %s, depth: %d\n", move_str, depth);
                printf("On board..\n");
                printBoard(b);
                break;
            }
        }
        printf("[%s]: depth: %d, fen: %s\n", passed ? "pass" : "FAIL", depth, fens[i]);
    }
}


void testFenGeneration(void) 
{
	printf("\ntestFenGeneration()\n");

    char *fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "5r2/2p1p1N1/4P3/2R1R3/NK3pp1/5Pk1/2P1pp2/8 w - - 0 1",
        "6R1/5P2/3p4/2Qr4/1p1pp2p/p4P1R/KP1P4/4k3 w - - 0 1",
        "4kbnr/1pp2ppp/r2qb3/p2Pn3/2BP1B2/2N2N2/PPP1QPPP/R3R1K1 w k - 0 1",
        "8/4pP2/1k1N2qP/1n1P4/PpK2b1P/8/1rP4R/8 w - - 0 1",
        "r1bqkbnr/ppp2ppp/8/3Pn3/2B5/5N2/PPPPQPPP/RNB2RK1 b kq - 0 1",
		"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
		"rnbqkb1r/ppNpp1pp/5n2/5p2/8/8/PPPPPPPP/R1BQKBNR b KQkq - 0 1",
		"rnb1kb1r/ppppqppp/8/8/8/5n2/PPPP1PPP/RNBQKBNR w KQkq - 0 1",
		"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
		"1n2k1nr/1ppp1ppp/5N2/4p3/1b1P2Pq/N5b1/1PPQPP1P/r1B1KB1R b Kk - 0 1",
		"1n2k1nr/1ppp1ppp/8/3Np2B/1b1P2Pq/N5b1/1PPQPP1P/r1B1K2R b Kk - 0 1",

    };
    const int n = sizeof(fens) / sizeof(fens[0]);
	for (int i = 0; i < n; i++) {
		char generated[100];
		const Board b = initBoardFromFen(fens[i]);
		printBoardFenToString(generated, sizeof(generated), &b);
		bool passed = strcmp(fens[i], generated) == 0;
		printf("[%s]: actual_fen: \"%s\", generated_fen: \"%s\"\n", passed ? "pass" : "FAIL", fens[i], generated);
	}
}
