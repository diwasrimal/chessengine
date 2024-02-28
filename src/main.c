#include "engine.h"
#include "utils.h"

#include <assert.h>
#include <ctype.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define CELL_SIZE 80
#define BOARD_PADDING 10
#define BOARD_SIZE (CELL_SIZE * 8)
#define WINDOW_SIZE (BOARD_SIZE + BOARD_PADDING * 2)

#define ICON_DIFF 10
#define ICON_SIZE (CELL_SIZE - ICON_DIFF)

#define COLOR_CHECK                                                            \
    (Color) { 0xd7, 0x6c, 0x6c, 0xff }
#define COLOR_BLACK                                                            \
    (Color) { 0x4E, 0x53, 0x56, 0xff }
#define COLOR_MOVE                                                             \
    (Color) { 0xcb, 0xdd, 0xaf, 0xff }
#define COLOR_CHECKER_DARK                                                     \
    (Color) { 0xc7, 0xce, 0xd1, 0xff }
#define COLOR_CHECKER_LIGHT WHITE

// Piece definitions confilct with colors from raylib
#define WHITE_PIECE 1 << 6
#define BLACK_PIECE 1 << 7

// For drawing promotion window
#define PROM_WIN_PADDING 5
#define PROM_WIN_X                                                             \
    (WINDOW_SIZE / 2 - (CELL_SIZE * 4) / 2 - PROM_WIN_PADDING / 2)
#define PROM_WIN_Y (WINDOW_SIZE / 2 - CELL_SIZE / 2 - PROM_WIN_PADDING / 2)

typedef struct {
    int x;
    int y;
} V2;

typedef struct {
    bool pending;
    char move_str[10];
} PromotionState;

typedef struct {
    Move last_move;
    bool king_checked;
    PromotionState prom_state;
    struct {
        int src_sq;
        V2 draw_pos;
    } dragged_piece;
} GuiState;

Piece promotables[4] = {
    BISHOP,
    ROOK,
    KNIGHT,
    QUEEN,
};

int drawYByRank(int rank);
int drawXByFile(int file);
int rankByPosY(int posy);
int fileByPosX(int posx);
V2 sqDrawPos(int sq);
Texture2D getPieceTexture(const char *path);
void loadPieceTextures(Texture2D *arr);
void unloadPieceTextures(Texture2D *arr);
void drawBoard(const Board *b, GuiState state, const Texture2D *textures);
void drawCheckmate();
void findTriedMove(char *str, const Board *b, int src_sq, int dst_sq,
                   PromotionState *prom_state);
Move getMatchingMove(char *tried, MoveList valids);
GuiState initGuiState(const Board *b);
void drawPromotionWindow(Piece promoting_color, Texture2D *textures);

int main(int argc, char **argv)
{
    char *init_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    char *fen = (argc >= 2) ? argv[1] : init_fen;
    precomputeValues();

    bool computer_playing = false;

    Board board = initBoardFromFen(fen);
    GuiState state = initGuiState(&board);
    MoveList mlist = generateMoves(&board);

    InitWindow(WINDOW_SIZE, WINDOW_SIZE, "Chess");
    SetTargetFPS(60);
    Texture2D piece_textures[256];
    loadPieceTextures(piece_textures);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(COLOR_BLACK);

        drawBoard(&board, state, piece_textures);
        if (mlist.count == 0) {
            drawCheckmate();
            EndDrawing();
            continue;
        }

        if (state.prom_state.pending) {
            drawPromotionWindow(board.color_to_move, piece_textures);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                int posx = GetMouseX();
                int posy = GetMouseY();
                int y_start = PROM_WIN_Y + PROM_WIN_PADDING;
                int y_end = y_start + CELL_SIZE;
                int diff = posx - PROM_WIN_X + PROM_WIN_PADDING;
                if (diff > 0 && y_start <= posy && posy <= y_end) {
                    int i = diff / CELL_SIZE;
                    printf("gui: i is: %d\n", i);
                    if (0 <= i && i <= 3) {
                        char move_str[10];
                        char prom_notations[4] = {'b', 'r', 'n', 'q'};
                        sprintf(move_str, "%s%c", state.prom_state.move_str,
                                prom_notations[i]);
                        printf("gui: Promotion Move: %s\n", move_str);
                        Move m = getMatchingMove(move_str, mlist);
                        if (m != EMPTY_MOVE) {
                            board = moveMake(m, board);
                            state.last_move = m;
                            state.king_checked =
                                isKingChecked(&board, board.color_to_move);
                            state.prom_state.pending = false;
                            mlist = generateMoves(&board);
                        }
                    }
                }
            }
            EndDrawing();
            continue;
        }
        EndDrawing();

        if (computer_playing) {
            if (board.color_to_move & BLACK_PIECE) {
                Move m = findBestMove(&board);
                board = moveMake(m, board);
                state.last_move = m;
                mlist = generateMoves(&board);
                printf("\nPossible moves: \n");
                printMoveList(mlist);
                continue;
            }
        }

        // Look for dragging events
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int sq = rankByPosY(GetMouseY()) * 8 + fileByPosX(GetMouseX());
            if (isValidSquare(sq) && board.pieces[sq] != EMPTY_PIECE) {
                state.dragged_piece.src_sq = sq;
            }
        }

        // Update piece's drawing position during drag
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            if (state.dragged_piece.src_sq != -1) {
                state.dragged_piece.draw_pos = (V2) {
                    .x = GetMouseX() - CELL_SIZE / 2,
                    .y = GetMouseY() - CELL_SIZE / 2,
                };
            }
        }

        // Trying making move after release
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            int src_sq = state.dragged_piece.src_sq;
            int dst_sq = rankByPosY(GetMouseY()) * 8 + fileByPosX(GetMouseX());
            if (src_sq != -1 && isValidSquare(dst_sq) && dst_sq != src_sq) {
                char move_str[10] = {0};
                findTriedMove(move_str, &board, src_sq, dst_sq, &state.prom_state);
                if (!state.prom_state.pending) {
                    printf("gui: Your Move: %s\n", move_str);
                    Move m = getMatchingMove(move_str, mlist);
                    if (m != EMPTY_MOVE) {
                        board = moveMake(m, board);
                        state.last_move = m;
                        state.king_checked = isKingChecked(&board, board.color_to_move);
                        mlist = generateMoves(&board);
                        printf("\nengine: Possible moves: \n");
                        printMoveList(mlist);
                    }
                }
            }
            state.dragged_piece.src_sq = -1;
        }
    }

    printf("gui: Closing window\n");
    unloadPieceTextures(piece_textures);
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

Texture2D getPieceTexture(const char *path)
{
    Image img = LoadImage(path);
    ImageResize(&img, ICON_SIZE, ICON_SIZE);
    Texture2D texture = LoadTextureFromImage(img);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
    return texture;
}

void loadPieceTextures(Texture2D *arr)
{
    arr[WHITE_PIECE | PAWN] = getPieceTexture("./resources/white-pawn.png");
    arr[WHITE_PIECE | ROOK] = getPieceTexture("./resources/white-rook.png");
    arr[WHITE_PIECE | KNIGHT] = getPieceTexture("./resources/white-knight.png");
    arr[WHITE_PIECE | BISHOP] = getPieceTexture("./resources/white-bishop.png");
    arr[WHITE_PIECE | QUEEN] = getPieceTexture("./resources/white-queen.png");
    arr[WHITE_PIECE | KING] = getPieceTexture("./resources/white-king.png");
    arr[BLACK_PIECE | PAWN] = getPieceTexture("./resources/black-pawn.png");
    arr[BLACK_PIECE | ROOK] = getPieceTexture("./resources/black-rook.png");
    arr[BLACK_PIECE | KNIGHT] = getPieceTexture("./resources/black-knight.png");
    arr[BLACK_PIECE | BISHOP] = getPieceTexture("./resources/black-bishop.png");
    arr[BLACK_PIECE | QUEEN] = getPieceTexture("./resources/black-queen.png");
    arr[BLACK_PIECE | KING] = getPieceTexture("./resources/black-king.png");
}

void unloadPieceTextures(Texture2D *arr)
{
    UnloadTexture(arr[WHITE_PIECE | PAWN]);
    UnloadTexture(arr[WHITE_PIECE | ROOK]);
    UnloadTexture(arr[WHITE_PIECE | KNIGHT]);
    UnloadTexture(arr[WHITE_PIECE | BISHOP]);
    UnloadTexture(arr[WHITE_PIECE | QUEEN]);
    UnloadTexture(arr[WHITE_PIECE | KING]);
    UnloadTexture(arr[BLACK_PIECE | PAWN]);
    UnloadTexture(arr[BLACK_PIECE | ROOK]);
    UnloadTexture(arr[BLACK_PIECE | KNIGHT]);
    UnloadTexture(arr[BLACK_PIECE | BISHOP]);
    UnloadTexture(arr[BLACK_PIECE | QUEEN]);
    UnloadTexture(arr[BLACK_PIECE | KING]);
}

void drawBoard(const Board *b, GuiState state, const Texture2D *textures)
{
    if (!IsWindowReady())
        assert(0 && "!IsWindowReady(), Cannot drawBoard()\n");

    int last_move_src_sq = -1, last_move_dst_sq = -1;
    if (state.last_move != EMPTY_MOVE) {
        last_move_src_sq = getMoveSrc(state.last_move);
        last_move_dst_sq = getMoveDst(state.last_move);
    }

    int checked_king_sq = -1;
    if (state.king_checked) {
        checked_king_sq =
            b->king_squares[b->color_to_move & WHITE_PIECE ? 0 : 1];
    }

    int drag_src_sq = state.dragged_piece.src_sq;

    // First draw backgrounds and idle pieces
    int padding = ICON_DIFF / 2;
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

            if (sq != drag_src_sq && b->pieces[sq] != EMPTY_PIECE) {
                DrawTexture(textures[b->pieces[sq]], x + padding, y + padding, WHITE);
            }
        }
    }

    // Then draw piece that's being dragged (if any)
    if (drag_src_sq != -1) {
        V2 pos = state.dragged_piece.draw_pos;
        DrawTexture(textures[b->pieces[drag_src_sq]], pos.x + padding, pos.y + padding, WHITE);
    }
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

void findTriedMove(char *str, const Board *b, int src_sq, int dst_sq,
                   PromotionState *prom_state)
{
    if (src_sq == dst_sq || !isValidSquare(src_sq) || !isValidSquare(dst_sq))
        return;

    // Get type of piece if promotion happening
    // TODO: use a graphical interface
    int drank = dst_sq / 8;
    char promoted = '\0';

    if ((b->pieces[src_sq] & (PAWN | WHITE_PIECE) && drank == 7) ||
        (b->pieces[src_sq] & (PAWN | BLACK_PIECE) && drank == 0)) {
        prom_state->pending = true;
        sprintf(prom_state->move_str, "%s%s", SQNAMES[src_sq], SQNAMES[dst_sq]);
    }

    sprintf(str, "%s%s%c", SQNAMES[src_sq], SQNAMES[dst_sq], promoted);
}

Move getMatchingMove(char *tried, MoveList valids)
{
    char valid_str[10];
    for (size_t i = 0; i < valids.count; i++) {
        printMoveToString(valids.moves[i], valid_str, false);
        if (strcmp(tried, valid_str) == 0) {
            return valids.moves[i];
        }
    }
    return EMPTY_MOVE;
}

GuiState initGuiState(const Board *b)
{
    GuiState state = {
        .king_checked = false,
        .last_move = EMPTY_MOVE,
        .prom_state.pending = false,
        .dragged_piece.src_sq = -1,
    };

    if (isKingChecked(b, b->color_to_move))
        state.king_checked = true;
    return state;
}

void drawPromotionWindow(Piece promoting_color, Texture2D *textures)
{
    int width = CELL_SIZE * 4 + PROM_WIN_PADDING * 2;
    int height = CELL_SIZE + PROM_WIN_PADDING * 2;
    DrawRectangle(PROM_WIN_X, PROM_WIN_Y, width, height, COLOR_BLACK);

    int pieces_x_start = PROM_WIN_PADDING + PROM_WIN_X;
    int pieces_y = PROM_WIN_Y + PROM_WIN_PADDING;
    for (int x = pieces_x_start, i = 0; i < 4; i++, x += CELL_SIZE) {
        DrawRectangle(x, pieces_y, CELL_SIZE, CELL_SIZE, WHITE);
        DrawTexture(textures[promotables[i] | promoting_color],
                    x + ICON_DIFF / 2, pieces_y + ICON_DIFF / 2, WHITE);
    }
}
