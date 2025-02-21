#include "engine.h"
#include "move.h"
#include "piece.h"
#include "utils.h"

#include <assert.h>
#include <ctype.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define CELL_SIZE 80
#define BOARD_PADDING 10
#define BOARD_SIZE (CELL_SIZE * 8)
#define WINDOW_SIZE (BOARD_SIZE + BOARD_PADDING * 2)

#define PIECE_PADDING 4
#define PIECE_SIZE (CELL_SIZE - PIECE_PADDING*2)

#define COLOR_CHECK             (Color) { 0xd7, 0x6c, 0x6c, 0xff }
#define COLOR_BLACK             (Color) { 0x4E, 0x53, 0x56, 0xff }
#define COLOR_MOVE              (Color) { 0xcb, 0xdd, 0xaf, 0xff }
#define COLOR_CHECKER_DARK      (Color) { 0xc7, 0xce, 0xd1, 0xff }
#define COLOR_CHECKER_LIGHT     WHITE

// Piece definitions confilct with colors from raylib
#define WHITE_PIECE 1 << 6
#define BLACK_PIECE 1 << 7

// For drawing promotion window
#define PROM_WIN_PADDING 5
#define PROM_WIN_X       (WINDOW_SIZE / 2 - (CELL_SIZE * 4) / 2 - PROM_WIN_PADDING / 2)
#define PROM_WIN_Y       (WINDOW_SIZE / 2 - CELL_SIZE / 2 - PROM_WIN_PADDING / 2)

typedef struct {
    int x;
    int y;
} V2;

typedef struct {
    Board board;
    MoveList mlist;
    Move last_move;
    bool king_checked;
    bool prom_pending;
    bool computer_thinking;
    char prom_move[10];
    int dragged_piece_src_sq;
    V2 dragged_piece_draw_pos;
} GameState;

GameState initGameState(Board b);
Texture2D getPieceTexture(const char *path);
V2 sqDrawPos(int sq);
int drawXByFile(int file);
int drawYByRank(int rank);
int fileByPosX(int posx);
int rankByPosY(int posy);
void drawBoard(const GameState *state);
void drawPiece(Piece piece, int square_x, int square_y);
void drawCheckmate();
void drawPromotionWindow(Piece promoting_color);
void loadPieceTextures();
void loadTextureMapAndPieceRects();
void unloadPieceTextures();
void updateStateWithMove(GameState *state, Move m);
void *playComputerMove(void *st);

Piece promotables[4] = {
    BISHOP,
    ROOK,
    KNIGHT,
    QUEEN,
};

// Single texture that holds all pieces,
Texture piece_texture_map;

// Stores individual piece's position in the texture map
Rectangle piece_texture_rect[256];


int main(int argc, char **argv)
{
    char *init_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    char *fen = (argc >= 2) ? argv[1] : init_fen;
    precomputeValues();

    GameState state = initGameState(initBoardFromFen(fen));
    bool computer_playing = false;

    // Board board = initBoardFromFen(fen);
    // GuiState state = initGuiState(&board);
    // MoveList mlist = generateMoves(&board);

    InitWindow(WINDOW_SIZE, WINDOW_SIZE, "Chess");
    SetTargetFPS(60);
    loadTextureMapAndPieceRects();

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(COLOR_BLACK);

        drawBoard(&state);
        if (state.mlist.count == 0) {
            drawCheckmate();
            EndDrawing();
            continue;
        }

        if (state.prom_pending) {
            drawPromotionWindow(state.board.color_to_move);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                int x = GetMouseX();
                int y = GetMouseY();
                int y_start = PROM_WIN_Y + PROM_WIN_PADDING;
                int y_end = y_start + CELL_SIZE;
                int diff = x - PROM_WIN_X + PROM_WIN_PADDING;
                if (diff > 0 && y_start <= y && y <= y_end) {
                    int i = diff / CELL_SIZE;
                    if (0 <= i && i <= 3) {
                        char move_str[10];
                        char prom_notations[4] = {'b', 'r', 'n', 'q'};  // similar to display order
                        sprintf(move_str, "%s%c", state.prom_move, prom_notations[i]);
                        for (size_t i = 0; i < state.mlist.count; i++) {
                            Move m = state.mlist.moves[i];
                            char str[10];
                            printMoveToString(m, str, false);
                            if (strcmp(move_str, str) == 0) {
                                updateStateWithMove(&state, m);
                                state.prom_pending = false;
                            }
                        }
                    }
                }
            }
            EndDrawing();
            continue;
        }

        if (computer_playing && !state.computer_thinking) {
            if (state.board.color_to_move & BLACK_PIECE) {
                pthread_t tid;
                printf("Computer thinking...\n");
                state.computer_thinking = true;
                pthread_create(&tid, NULL, playComputerMove, (void *)&state);
                continue;
            }
        }

        EndDrawing();

        // Look for dragging events
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int sq = rankByPosY(GetMouseY()) * 8 + fileByPosX(GetMouseX());
            if (isValidSquare(sq) && state.board.pieces[sq] != EMPTY_PIECE) {
                state.dragged_piece_src_sq = sq;
            }
        }

        // Update piece's drawing position during drag
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            if (state.dragged_piece_src_sq != -1) {
                state.dragged_piece_draw_pos = (V2) {
                    .x = GetMouseX() - CELL_SIZE / 2,
                    .y = GetMouseY() - CELL_SIZE / 2,
                };
            }
        }

        // Try making move after release
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            int src_sq = state.dragged_piece_src_sq;
            int dst_sq = rankByPosY(GetMouseY()) * 8 + fileByPosX(GetMouseX());
            if (src_sq != -1 && isValidSquare(dst_sq) && dst_sq != src_sq) {

                char try[10];   // String representation of tried move
                snprintf(try, sizeof(try), "%s%s", SQNAMES[src_sq], SQNAMES[dst_sq]);
                printf("gui: tried move: %s\n", try);

                for (size_t i = 0; i < state.mlist.count; i++) {
                    Move m = state.mlist.moves[i];
                    char str[10];
                    printMoveToString(m, str, false);
                    if (strncmp(str, try, 4) == 0) {
                        MoveFlag flag = getMoveFlag(m);
                        if (flag & PROMOTION) {
                            state.prom_pending = true;
                            strcpy(state.prom_move, try);
                        } else {
                            updateStateWithMove(&state, m);
                        }
                        break;
                    }
                }
            }
            state.dragged_piece_src_sq = -1;
        }
    }

    printf("gui: Closing window\n");
    UnloadTexture(piece_texture_map);
    CloseWindow();
}

// Returns the drawing y position for a given cell's rank
// Note that higher ranks are at top side of board
int drawYByRank(int rank) { return BOARD_PADDING + ((7 - rank) * CELL_SIZE); }

int drawXByFile(int file) { return BOARD_PADDING + (file * CELL_SIZE); }

// Returns rank of touched cell
int rankByPosY(int posy) { return 7 - (posy - BOARD_PADDING) / CELL_SIZE; }

int fileByPosX(int posx) { return (posx - BOARD_PADDING) / CELL_SIZE; }

V2 sqDrawPos(int sq)
{
    if (!isValidSquare(sq))
        assert(0 && "Invalid square!");
    V2 pos = {.x = drawXByFile(sq % 8), .y = drawYByRank(sq / 8)};
    return pos;
}

void loadTextureMapAndPieceRects()
{
    piece_texture_map = LoadTexture("./resources/chess-pieces.png");
    GenTextureMipmaps(&piece_texture_map);
    SetTextureFilter(piece_texture_map, TEXTURE_FILTER_TRILINEAR);

    const int crop_size = 468;
    const Piece texture_map_pieces_in_order[6] = {KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN};

    for (int i = 0; i < 6; i++) {
        Piece p = texture_map_pieces_in_order[i];
        piece_texture_rect[p | WHITE_PIECE] = (Rectangle){
            .x = i * crop_size,
            .y = 0 * crop_size,
            .height = crop_size,
            .width = crop_size,
        };
        piece_texture_rect[p | BLACK_PIECE] = (Rectangle){
            .x = i * crop_size,
            .y = 1 * crop_size,
            .height = crop_size,
            .width = crop_size,
        };
    }
}

Texture2D getPieceTexture(const char *path)
{
    Image img = LoadImage(path);
    ImageResize(&img, PIECE_SIZE, PIECE_SIZE);
    Texture2D texture = LoadTextureFromImage(img);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
    UnloadImage(img);
    return texture;
}

void drawBoard(const GameState *state)
{
    if (!IsWindowReady())
        assert(0 && "!IsWindowReady(), Cannot drawBoard()\n");

    Board b = state->board;

    int last_move_src_sq = -1, last_move_dst_sq = -1;
    if (state->last_move != EMPTY_MOVE) {
        last_move_src_sq = getMoveSrc(state->last_move);
        last_move_dst_sq = getMoveDst(state->last_move);
    }

    int checked_king_sq = -1;
    if (state->king_checked) {
        checked_king_sq =
            b.king_squares[b.color_to_move & WHITE_PIECE ? 0 : 1];
    }

    int drag_src_sq = state->dragged_piece_src_sq;

    // First draw backgrounds and idle pieces
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            int x = drawXByFile(file);
            int y = drawYByRank(rank);
            Color bg = (rank + file) % 2 == 0 ? COLOR_CHECKER_DARK : COLOR_CHECKER_LIGHT;
            if (sq == last_move_dst_sq || sq == last_move_src_sq) {
                bg = COLOR_MOVE;
                bg.a = (rank + file) % 2 == 0 ? 0xf0 : 0xd0;
            }
            if (sq == checked_king_sq) {
                bg = COLOR_CHECK;
            }
            DrawRectangle(x, y, CELL_SIZE, CELL_SIZE, bg);

            if (sq != drag_src_sq && b.pieces[sq] != EMPTY_PIECE) {
                drawPiece(b.pieces[sq], x, y);
            }
        }
    }

    // Then draw piece that's being dragged (if any)
    if (drag_src_sq != -1) {
        V2 pos = state->dragged_piece_draw_pos;
        drawPiece(b.pieces[drag_src_sq], pos.x, pos.y);
    }
}

void drawPiece(Piece piece, int square_x, int square_y)
{
    Rectangle src = piece_texture_rect[piece];
    Rectangle dst = {
        .x = square_x + PIECE_PADDING,
        .y = square_y + PIECE_PADDING,
        .height = PIECE_SIZE,
        .width = PIECE_SIZE
    };
    DrawTexturePro(piece_texture_map, src, dst, (Vector2){0, 0}, 0, WHITE);
}

void drawCheckmate()
{
    if (!IsWindowReady())
        assert(0 && "!IsWindowReady(), Cannot drawCheckmate()\n");
    const char *msg = "Checkmate!";
    const int size = 45;
    int msg_width = MeasureText(msg, size);
    DrawText(msg, WINDOW_SIZE / 2 - msg_width / 2, WINDOW_SIZE / 2 - size / 2,
             size, RED);
}

GameState initGameState(Board b)
{
    GameState state = {
        .board = b,
        .mlist = generateMoves(&b),
        .last_move = EMPTY_MOVE,
        .king_checked = isKingChecked(&b, b.color_to_move),
        .prom_pending = false,
        .computer_thinking = false,
        .dragged_piece_src_sq = -1,
    };
    return state;
}

void updateStateWithMove(GameState *state, Move m)
{
    state->board = moveMake(m, state->board);
    state->last_move = m;
    state->king_checked = isKingChecked(&state->board, state->board.color_to_move);
    state->mlist = generateMoves(&state->board);

    printf("\ngui: States\n");
    printf("gui: king_checked: %d\n", state->king_checked);
    printf("gui: prom_pending: %d\n", state->prom_pending);
    printf("gui: computer_thinking: %d\n", state->computer_thinking);
    printMoveList(state->mlist);
}

void drawPromotionWindow(Piece promoting_color)
{
    int width = CELL_SIZE * 4 + PROM_WIN_PADDING * 2;
    int height = CELL_SIZE + PROM_WIN_PADDING * 2;
    DrawRectangle(PROM_WIN_X, PROM_WIN_Y, width, height, COLOR_BLACK);

    int pieces_x_start = PROM_WIN_PADDING + PROM_WIN_X;
    int pieces_y = PROM_WIN_Y + PROM_WIN_PADDING;
    for (int x = pieces_x_start, i = 0; i < 4; i++, x += CELL_SIZE) {
        DrawRectangle(x, pieces_y, CELL_SIZE, CELL_SIZE, WHITE);
        drawPiece(promotables[i] | promoting_color, x, pieces_y);
    }
}

void *playComputerMove(void *st)
{
    GameState *state = (GameState *) st;
    Move m = findBestMove(&state->board);
    updateStateWithMove(state, m);
    state->computer_thinking = false;
    return NULL;
}
