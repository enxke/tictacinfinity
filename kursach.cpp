#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define _CRT_SECURE_NO_WARNINGS
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>

// Настройки поля
#define MAX_CELLS 50
#define CELL_SIZE 24.0f
#define GRID_LINES 50

typedef enum { EMPTY, X, O } Cell;
typedef enum {
    MENU_MAIN,
    MENU_PVE_SELECT,
    GAME_RUNNING
} GameState;

typedef enum {
    PLAYER_VS_PLAYER,
    PLAYER_VS_AI
} GameMode;

Cell board[MAX_CELLS][MAX_CELLS];
int board_size = GRID_LINES;
float offset_x = 0, offset_y = 0;
int turn = 0; // 0 - X, 1 - O
GameMode mode = PLAYER_VS_AI;
GameState state = MENU_MAIN;
int player_symbol = EMPTY;
int game_over = 0;

unsigned char ttf_buffer[1 << 20]; // ~1MB
unsigned char temp_bitmap[512 * 512];
GLuint font_texture;
stbtt_bakedchar cdata[96];

// === Прототипы функций ===
void draw_text(const char* text, float x, float y);
void reset_game();
int check_win(Cell player);
int count_line(int x, int y, Cell player);
int evaluate_position(Cell player);
void ai_move();
void find_best_move(int* best_x, int* best_y);
bool is_cell_occupied(int x, int y);
bool is_cell(int x, int y, Cell player);
void get_candidate_moves(int* candidate_x, int* candidate_y, int* count);

void draw_line(float x1, float y1, float x2, float y2);
void draw_grid();
void draw_symbol(int x, int y, Cell cell);
void draw_board();
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

// === MAIN ===
int main() {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Tic-Tac-Toe Infinite", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);

    glOrtho(0, 1920, 1080, 0, -1, 1);

    FILE* fp = fopen("assets/Roboto-Regular.ttf", "rb");
    if (!fp) {
        printf("Не могу открыть шрифт!\n");
        printf("Проверьте, существует ли файл: assets/Roboto-Regular.ttf\n");
        return -1;
    }

    fread(ttf_buffer, 1, 1 << 20, fp);
    fclose(fp);

    glGenTextures(1, &font_texture);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 512, 512, 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    stbtt_BakeFontBitmap(ttf_buffer, 0, 32.0f, temp_bitmap, 512, 512, 32, 96, cdata);

    reset_game();

    while (state != GAME_RUNNING) {
        printf("\nSelect Mode:\n");
        printf("1. Player vs Player (PvP)\n");
        printf("2. Player vs Bot (PvE)\n");
        printf("Enter your choice: ");
        int choice;
        scanf("%d", &choice);

        if (choice == 1) {
            mode = PLAYER_VS_PLAYER;
            state = GAME_RUNNING;
            turn = 0;
            reset_game();
        }
        else if (choice == 2) {
            printf("\nPlay as:\n");
            printf("1. O (second move)\n");
            printf("2. X (first move)\n");
            printf("Enter your choice: ");
            scanf("%d", &choice);

            if (choice == 1) {
                mode = PLAYER_VS_AI;
                player_symbol = O;
                turn = 1;
                state = GAME_RUNNING;
                reset_game();

                ai_move(); // Бот делает первый ход как X
                turn = 0;
                if (check_win(X)) {
                    printf("Bot wins!\n");
                    game_over = 1;
                }
            }
            else if (choice == 2) {
                mode = PLAYER_VS_AI;
                player_symbol = X;
                turn = 0;
                state = GAME_RUNNING;
                reset_game();
            }
        }
    }

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        draw_grid();
        draw_board();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// Функция отрисовки текста
void draw_text(const char* text, float x, float y) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);

    float xPos = x;
    float yPos = y;

    while (*text) {
        if (*text >= 32 && *text < 128) {
            stbtt_bakedchar* b = &cdata[*text - 32];
            float x0 = b->x0 + xPos;
            float y0 = b->y0 + yPos;
            float x1 = b->x1 + xPos;
            float y1 = b->y1 + yPos;

            float s0 = b->x0 / 512.0f;
            float t0 = b->y0 / 512.0f;
            float s1 = b->x1 / 512.0f;
            float t1 = b->y1 / 512.0f;

            glTexCoord2f(s0, t0); glVertex2f(x0, y0);
            glTexCoord2f(s1, t0); glVertex2f(x1, y0);
            glTexCoord2f(s1, t1); glVertex2f(x1, y1);
            glTexCoord2f(s0, t1); glVertex2f(x0, y1);

            xPos += b->xadvance;
        }
        ++text;
    }

    glEnd();
    glDisable(GL_TEXTURE_2D);
}

// Сброс игры
void reset_game() {
    for (int i = 0; i < MAX_CELLS; i++)
        for (int j = 0; j < MAX_CELLS; j++)
            board[i][j] = EMPTY;

    game_over = 0;

    if (mode == PLAYER_VS_AI) {
        if (player_symbol == X) {
            turn = 0;
        }
        else {
            turn = 1;
        }
    }
    else {
        turn = 0;
    }
}

// Проверка победы при 5 в ряд
int check_win(Cell player) {
    // По горизонтали
    for (int y = 0; y < board_size; y++) {
        int count = 0;
        for (int x = 0; x < board_size; x++) {
            if (board[x][y] == player)
                count++;
            else
                count = 0;
            if (count >= 5) return 1;
        }
    }

    // По вертикали
    for (int x = 0; x < board_size; x++) {
        int count = 0;
        for (int y = 0; y < board_size; y++) {
            if (board[x][y] == player)
                count++;
            else
                count = 0;
            if (count >= 5) return 1;
        }
    }

    // Диагонали слева направо
    for (int start_x = 0; start_x < board_size; start_x++) {
        for (int start_y = 0; start_y < board_size; start_y++) {
            int count = 0;
            for (int i = 0; i < board_size; i++) {
                int x = start_x + i;
                int y = start_y + i;
                if (x >= board_size || y >= board_size) break;
                if (board[x][y] == player)
                    count++;
                else
                    count = 0;
                if (count >= 5) return 1;
            }
        }
    }

    // Диагонали справа налево
    for (int start_x = 0; start_x < board_size; start_x++) {
        for (int start_y = 0; start_y < board_size; start_y++) {
            int count = 0;
            for (int i = 0; i < board_size; i++) {
                int x = start_x + i;
                int y = start_y - i;
                if (x >= board_size || y < 0) break;
                if (board[x][y] == player)
                    count++;
                else
                    count = 0;
                if (count >= 5) return 1;
            }
        }
    }

    return 0;
}

// Подсчёт веса линии вокруг точки
int count_line(int x, int y, Cell player) {
    const int dirs[4][2] = { {1, 0}, {0, 1}, {1, 1}, {1, -1} };
    int score = 0;

    for (int d = 0; d < 4; d++) {
        int dx = dirs[d][0], dy = dirs[d][1];
        int length = 1;
        int open_ends = 0;

        // В одном направлении
        for (int step = 1; step <= 5; step++) {
            int nx = x + dx * step;
            int ny = y + dy * step;
            if (nx < 0 || nx >= board_size || ny < 0 || ny >= board_size) {
                open_ends++;
                break;
            }
            if (board[nx][ny] == player) length++;
            else break;
        }

        // В другом направлении
        for (int step = 1; step <= 5; step++) {
            int nx = x - dx * step;
            int ny = y - dy * step;
            if (nx < 0 || nx >= board_size || ny < 0 || ny >= board_size) {
                open_ends++;
                break;
            }
            if (board[nx][ny] == player) length++;
            else break;
        }

        if (length >= 5) score += 100000;
        else if (length == 4 && open_ends >= 1) score += 10000;
        else if (length == 3 && open_ends >= 1) score += 1000;
        else if (length == 2 && open_ends >= 1) score += 100;
    }

    return score;
}

// Оценка позиции для бота
int evaluate_position(Cell player) {
    int score = 0;

    for (int x = 0; x < board_size; x++) {
        for (int y = 0; y < board_size; y++) {
            if (board[x][y] == player) {
                score += count_line(x, y, player);
            }
            else if (board[x][y] != EMPTY) {
                score -= count_line(x, y, board[x][y]);
            }
        }
    }

    return score;
}

// Получить список возможных ходов (окрестности занятых клеток)
void get_candidate_moves(int* candidate_x, int* candidate_y, int* count) {
    *count = 0;
    bool occupied[100][100] = { 0 };

    for (int x = 0; x < board_size; x++) {
        for (int y = 0; y < board_size; y++) {
            if (board[x][y] != EMPTY) {
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < board_size && ny >= 0 && ny < board_size &&
                            board[nx][ny] == EMPTY && !occupied[nx][ny]) {
                            candidate_x[*count] = nx;
                            candidate_y[*count] = ny;
                            (*count)++;
                            occupied[nx][ny] = true;
                        }
                    }
                }
            }
        }
    }

    if (*count == 0) {
        for (int x = 0; x < board_size; x++) {
            for (int y = 0; y < board_size; y++) {
                if (board[x][y] == EMPTY) {
                    candidate_x[*count] = x;
                    candidate_y[*count] = y;
                    (*count)++;
                    return;
                }
            }
        }
    }
}

// Найти лучший ход
void find_best_move(int* best_x, int* best_y) {
    int max_score = -1000000;
    *best_x = -1;
    *best_y = -1;

    int candidate_x[1000], candidate_y[1000];
    int candidate_count = 0;
    get_candidate_moves(candidate_x, candidate_y, &candidate_count);

    for (int i = 0; i < candidate_count; i++) {
        int x = candidate_x[i];
        int y = candidate_y[i];

        if (board[x][y] == EMPTY) {
            board[x][y] = O;
            int score = evaluate_position(O);
            board[x][y] = EMPTY;

            if (score > max_score) {
                max_score = score;
                *best_x = x;
                *best_y = y;
            }
        }
    }

    if (*best_x == -1) {
        for (int x = 0; x < board_size; x++) {
            for (int y = 0; y < board_size; y++) {
                if (board[x][y] == EMPTY) {
                    *best_x = x;
                    *best_y = y;
                    return;
                }
            }
        }
    }
}

// Ход бота (минимакс)
void ai_move() {
    if (game_over) return;

    int best_x = -1, best_y = -1;

    // Если это первый ход — занимаем центр
    int total_moves = 0;
    for (int x = 0; x < board_size; x++) {
        for (int y = 0; y < board_size; y++) {
            if (board[x][y] != EMPTY) total_moves++;
        }
    }

    if (total_moves == 0) {
        board[board_size / 2][board_size / 2] = O;
        return;
    }

    // Если есть выигрышные ходы — делаем их
    for (int x = 0; x < board_size; x++) {
        for (int y = 0; y < board_size; y++) {
            if (board[x][y] == EMPTY) {
                board[x][y] = O;
                if (check_win(O)) return;
                board[x][y] = EMPTY;
            }
        }
    }

    // Если нужно блокировать игрока — делаем это
    for (int x = 0; x < board_size; x++) {
        for (int y = 0; y < board_size; y++) {
            if (board[x][y] == EMPTY) {
                board[x][y] = X;
                if (check_win(X)) {
                    board[x][y] = O;
                    return;
                }
                board[x][y] = EMPTY;
            }
        }
    }

    // Ищем лучший ход по оценке
    find_best_move(&best_x, &best_y);
    if (best_x != -1 && best_y != -1) {
        board[best_x][best_y] = O;
        return;
    }

    // Если не нашли — случайный ход
    for (int x = 0; x < board_size; x++) {
        for (int y = 0; y < board_size; y++) {
            if (board[x][y] == EMPTY) {
                board[x][y] = O;
                return;
            }
        }
    }
}

// Рисование линии
void draw_line(float x1, float y1, float x2, float y2) {
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

// Рисование сетки
void draw_grid() {
    glColor3f(0.5f, 0.5f, 0.5f);
    for (int i = 0; i <= board_size; i++) {
        draw_line(i * CELL_SIZE + offset_x, 0 + offset_y,
            i * CELL_SIZE + offset_x, board_size * CELL_SIZE + offset_y);
        draw_line(0 + offset_x, i * CELL_SIZE + offset_y,
            board_size * CELL_SIZE + offset_x, i * CELL_SIZE + offset_y);
    }
}

// Рисование крестика или нолика
void draw_symbol(int x, int y, Cell cell) {
    float cx = x * CELL_SIZE + offset_x + CELL_SIZE / 2;
    float cy = y * CELL_SIZE + offset_y + CELL_SIZE / 2;
    float r = CELL_SIZE / 3;

    if (cell == X) {
        glColor3f(1.0f, 0.0f, 0.0f);
        draw_line(cx - r, cy - r, cx + r, cy + r);
        draw_line(cx - r, cy + r, cx + r, cy - r);
    }
    else if (cell == O) {
        glColor3f(0.0f, 1.0f, 0.0f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 360; i++) {
            float rad = i * M_PI / 180;
            float x = cx + cos(rad) * r;
            float y = cy + sin(rad) * r;
            glVertex2f(x, y);
        }
        glEnd();
    }
}

// Отрисовка всего игрового поля
void draw_board() {
    for (int i = 0; i < board_size; i++) {
        for (int j = 0; j < board_size; j++) {
            if (board[i][j] != EMPTY)
                draw_symbol(i, j, board[i][j]);
        }
    }
}

// Обработка кликов мыши
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (game_over || button != GLFW_MOUSE_BUTTON_1 || action != GLFW_PRESS)
        return;

    double x, y;
    glfwGetCursorPos(window, &x, &y);

    int cell_x = (x - offset_x) / CELL_SIZE;
    int cell_y = (y - offset_y) / CELL_SIZE;

    if (cell_x >= 0 && cell_x < board_size && cell_y >= 0 && cell_y < board_size &&
        board[cell_x][cell_y] == EMPTY) {

        board[cell_x][cell_y] = (turn == 0) ? X : O;

        if (check_win(board[cell_x][cell_y])) {
            printf("Player %c wins!\n", board[cell_x][cell_y] == X ? 'X' : 'O');
            game_over = 1;
        }

        turn = 1 - turn;

        if (mode == PLAYER_VS_AI && turn == 1 && !game_over) {
            ai_move();
            turn = 0;
            if (check_win(O)) {
                printf("Bot wins!\n");
                game_over = 1;
            }
        }
    }
}

// Обработка клавиш
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_R) {
        reset_game();
        game_over = 0;
        state = MENU_MAIN;
    }
}