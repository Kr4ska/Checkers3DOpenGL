#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "Object.h"
#include "Checker.h"
#include "shader.h"
#include "font.h"

class CheckersBoard {
public:
    enum GameState { PLAYING, WHITE_WIN, BLACK_WIN };
    GameState gameState = PLAYING;
    enum Player { WHITE, BLACK };
    Player currentPlayer = Player::WHITE;
    static constexpr int SIZE = 8;
    glm::vec3 origin;
    float cellSize;
    float height;
    Model whiteModel, blackModel;
    glm::mat4 textProjection;
    Font* font = nullptr;
    glm::mat4* projection;
    Shader* shaderFont;

    CheckersBoard(Model whiteModel_,
        Model blackModel_,
        Model highlightModel_,
        Font* _font,
        Shader* shaderFont_,
        glm::vec3 origin_,
        float cellSize_,
        float height_ = 0.0f);
    ~CheckersBoard();

    void resetGame(); // Перезапуск игры
    bool checkWinCondition();                          // Проверка победы

    // Process click on board cell
    void onCellClick(int row, int col);
    // Draw all checkers and highlights
    void render(Shader& shader);

private:
    bool canJumpAgain = false;  // Может ли шашка прыгать снова
    int jumpRow = -1;           // Текущая строка прыгающей шашки
    int jumpCol = -1;           // Текущий столбец прыгающей шашки

    Checker* board[SIZE][SIZE] = { {} };
    Model highlightModel;

    std::vector<Object*> highlights;
    Checker* selectedChecker = nullptr;
    int selectedRow = -1, selectedCol = -1;
    bool checkPath(int r1, int c1, int r2, int c2) const;
    void clearHighlights();
    std::vector<std::pair<int, int>> calculateMoves(int row, int col) const;
    bool hasCaptures(Player player) const;
    bool isInside(int r, int c) const { return r >= 0 && r < SIZE && c >= 0 && c < SIZE; }
    glm::vec3 cellPosition(int row, int col) const {
        return origin + glm::vec3(col * cellSize, height, row * cellSize);
    }
    void switchPlayer() {
        currentPlayer = (currentPlayer == Player::WHITE) ? Player::BLACK : Player::WHITE;
        std::cout << (currentPlayer == Player::WHITE ? "Ход белых\n" : "Ход черных\n");
    }
};

CheckersBoard::CheckersBoard (
    Model whiteModel_,
    Model blackModel_,
    Model highlightModel_,
    Font* _font,
    Shader* shaderFont_,
    glm::vec3 origin_,
    float cellSize_,
    float height_)
    : highlightModel(highlightModel_), origin(origin_), cellSize(cellSize_), 
    height(height_), blackModel(blackModel_), whiteModel(whiteModel_), font(_font), shaderFont(shaderFont_) {

    textProjection = glm::ortho(0.0f, 1600.0f, 0.0f, 900.0f);
    for (int r = 0; r < SIZE; ++r)
        for (int c = 0; c < SIZE; ++c)
            board[r][c] = nullptr;
    // Setup initial pieces
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < SIZE; ++c)
            if ((r + c) % 2 == 1) board[r][c] = new Checker("Black", blackModel, cellPosition(r, c));
    for (int r = 5; r < SIZE; ++r)
        for (int c = 0; c < SIZE; ++c)
            if ((r + c) % 2 == 1) board[r][c] = new Checker("White", whiteModel, cellPosition(r, c));
}

CheckersBoard::~CheckersBoard() {
    for (int r = 0; r < SIZE; ++r)
        for (int c = 0; c < SIZE; ++c)
            delete board[r][c];
    clearHighlights();
}

bool CheckersBoard::checkWinCondition() {
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            if(board[row][col] != nullptr) {
                if (board[row][col]->isWhite() && currentPlayer == Player::WHITE) {
                    if (!calculateMoves(row, col).empty()) {
                        return false;
                    }
                }
                else if (!board[row][col]->isWhite() && currentPlayer == Player::BLACK) {
                    if (!calculateMoves(row, col).empty()) {
                        return false;
                    }
                }
            }
        }
    }
    if (currentPlayer == Player::WHITE) {
        gameState = BLACK_WIN;
    }
    else {
        gameState = WHITE_WIN;
    }
    return true;
}

// Реализация перезапуска игры
void CheckersBoard::resetGame() {
    // Очистка доски
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            delete board[r][c];
            board[r][c] = nullptr;
        }
    }

    // Повторная инициализация
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            if ((r + c) % 2 == 1) {
                board[r][c] = new Checker("Black", blackModel, cellPosition(r, c));
            }
        }
    }
    for (int r = 5; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            if ((r + c) % 2 == 1) {
                board[r][c] = new Checker("White", whiteModel, cellPosition(r, c));
            }
        }
    }

    // Сброс состояния
    gameState = PLAYING;
    currentPlayer = Player::WHITE;
    clearHighlights();
    selectedChecker = nullptr;
}

bool CheckersBoard::hasCaptures(Player player) const {
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            Checker* checker = board[r][c];
            if (!checker || (player == Player::WHITE && !checker->isWhite())
                || (player == Player::BLACK && checker->isWhite())) continue;

            auto moves = calculateMoves(r, c);
            for (const auto& move : moves) {
                if (abs(move.first - r) > 1 || abs(move.second - c) > 1) {
                    // Проверяем, есть ли реальный враг на пути
                    int dr = (move.first - r) > 0 ? 1 : -1;
                    int dc = (move.second - c) > 0 ? 1 : -1;
                    int steps = std::max(abs(move.first - r), abs(move.second - c));

                    for (int i = 1; i < steps; ++i) {
                        int curR = r + dr * i;
                        int curC = c + dc * i;
                        if (board[curR][curC] && board[curR][curC]->isWhite() != checker->isWhite()) {
                            return true; // Найдено реальное взятие
                        }
                    }
                }
            }
        }
    }
    return false;
}

void CheckersBoard::onCellClick(int row, int col) {
    if (!isInside(row, col)) return;
    if (gameState != PLAYING) std::cout << "Перезапустите игру (нажмите кнопку R)\n";
    Checker* clickedChecker = board[row][col];
    bool moveProcessed = false;
    bool mustCapture = hasCaptures(currentPlayer);

    // ─── Блок выбора шашки ────────────────────────────────────────────────
    if (clickedChecker && !selectedChecker) {
        if ((currentPlayer == Player::WHITE && clickedChecker->isWhite()) ||
            (currentPlayer == Player::BLACK && !clickedChecker->isWhite())) {

            auto moves = calculateMoves(row, col);
            bool hasRealJump = std::any_of(moves.begin(), moves.end(), [row, col](const auto& m) {
                return abs(m.first - row) > 1 || abs(m.second - col) > 1; });

            // Проверяем только прыжки с реальным врагом на пути
            for (const auto& move : moves) {
                if (abs(move.first - row) > 1 || abs(move.second - col) > 1) {
                    int dr = (move.first - row) > 0 ? 1 : -1;
                    int dc = (move.second - col) > 0 ? 1 : -1;
                    int steps = std::max(abs(move.first - row), abs(move.second - col));

                    for (int i = 1; i < steps; ++i) {
                        int curR = row + dr * i;
                        int curC = col + dc * i;
                        if (board[curR][curC] && board[curR][curC]->isWhite() != clickedChecker->isWhite()) {
                            hasRealJump = true;
                            break;
                        }
                    }
                    if (hasRealJump) break;
                }
            }

            // Если есть обязательные взятия, но у шашки их нет - блокируем выбор
            if (mustCapture && !hasRealJump) {
                std::cout << "Вы должны выбрать шашку с возможностью взятия!\n";
                return;
            }

            clearHighlights();
            selectedChecker = clickedChecker;
            selectedRow = row;
            selectedCol = col;

            // Подсветка только реальных ходов
            for (const auto& m : moves) {
                highlights.push_back(new Object("Highlight", highlightModel, cellPosition(m.first, m.second)));
            }
        }
        return;
    }
    // ─── Блок обработки хода ──────────────────────────────────────────────
    if (selectedChecker) {
        auto moves = calculateMoves(selectedRow, selectedCol);
        bool validMove = false;
        bool isJumpMove = false;
        std::vector<std::pair<int, int>> capturedCheckers;

        for (const auto& move : moves) {
            if (move.first == row && move.second == col) {
                validMove = true;
                isJumpMove = abs(row - selectedRow) > 1 || abs(col - selectedCol) > 1;
                break;
            }
        }

        if (validMove) {
            // Проверка на обязательное взятие
            if (mustCapture && !isJumpMove) {
                std::cout << "Вы должны совершить взятие!\n";
                return;
            }

            // Обработка прыжка
            if (isJumpMove) {
                int dr = (row - selectedRow) > 0 ? 1 : -1;
                int dc = (col - selectedCol) > 0 ? 1 : -1;
                int steps = std::max(abs(row - selectedRow), abs(col - selectedCol));

                // Удаление съеденных шашек
                for (int i = 1; i < steps; ++i) {
                    int curR = selectedRow + dr * i;
                    int curC = selectedCol + dc * i;
                    if (board[curR][curC] && board[curR][curC]->isWhite() != selectedChecker->isWhite()) {
                        delete board[curR][curC];
                        board[curR][curC] = nullptr;
                        capturedCheckers.emplace_back(curR, curC);
                        std::cout << "Шашка (" << curR << "," << curC << ") съедена\n";
                    }
                }
            }

            // Перемещение шашки
            board[row][col] = selectedChecker;
            board[selectedRow][selectedCol] = nullptr;
            selectedChecker->newPos(cellPosition(row, col));

            // Проверка превращения в дамку (только при движении вперед)
            if (!selectedChecker->getKing()) {
                bool isWhite = selectedChecker->isWhite();
                if ((isWhite && row == 0) || (!isWhite && row == SIZE - 1)) {
                    selectedChecker->setKing();
                    std::cout << "Шашка стала дамкой!\n";
                }
            }

            // Проверка продолжения прыжков
            if (!capturedCheckers.empty()) {
                auto newMoves = calculateMoves(row, col);
                bool hasMoreCaptures = false;

                // Проверяем, есть ли новые враги для взятия
                for (const auto& move : newMoves) {
                    if (abs(move.first - row) > 1 || abs(move.second - col) > 1) {
                        int dr = (move.first - row) > 0 ? 1 : -1;
                        int dc = (move.second - col) > 0 ? 1 : -1;
                        int steps = std::max(abs(move.first - row), abs(move.second - col));

                        for (int i = 1; i < steps; ++i) {
                            int curR = row + dr * i;
                            int curC = col + dc * i;
                            if (board[curR][curC] && board[curR][curC]->isWhite() != selectedChecker->isWhite()) {
                                hasMoreCaptures = true;
                                break;
                            }
                        }
                        if (hasMoreCaptures) break;
                    }
                }

                if (hasMoreCaptures) {
                    canJumpAgain = true;
                    jumpRow = row;
                    jumpCol = col;
                    selectedRow = row;
                    selectedCol = col;

                    clearHighlights();
                    for (const auto& m : newMoves) {
                        if (abs(m.first - row) > 1 || abs(m.second - col) > 1) {
                            highlights.push_back(new Object(
                                "Highlight",
                                highlightModel,
                                cellPosition(m.first, m.second)
                            ));
                        }
                    }
                    std::cout << "Продолжайте прыжки!\n";
                    return;
                }
            }

            // Завершение хода
            clearHighlights();
            selectedChecker = nullptr;
            canJumpAgain = false;

            if (capturedCheckers.empty() || !canJumpAgain) {
                switchPlayer();
                if (checkWinCondition())
                    std::cout << "Победа " << ((gameState == WHITE_WIN) ? "белых" : "черных") << std::endl;
            }

            moveProcessed = true;
        }
        else {
            std::cout << "Недопустимый ход!\n";
        }
    }

    // ─── Смена выбора шашки ─────────────────────────────────────────
    if (!moveProcessed && clickedChecker && clickedChecker != selectedChecker) {
        clearHighlights();
        selectedChecker = nullptr;
        onCellClick(row, col);
    }
}

std::vector<std::pair<int, int>> CheckersBoard::calculateMoves(int row, int col) const {
    std::vector<std::pair<int, int>> moves;
    Checker* chk = board[row][col];
    if (!chk) return moves;

    bool isKing = chk->getKing();
    bool isWhite = chk->isWhite();
    int forward = isWhite ? -1 : 1;

    // Обычные шашки
    if (!isKing) {
        std::vector<std::pair<int, int>> jumpMoves;

        // Прыжки в любом направлении
        for (int dr : {-1, 1}) {
            for (int dc : {-1, 1}) {
                int jr = row + 2 * dr;
                int jc = col + 2 * dc;
                if (isInside(jr, jc) && !board[jr][jc]) {
                    Checker* mid = board[row + dr][col + dc];
                    if (mid && mid->isWhite() != isWhite) {
                        jumpMoves.emplace_back(jr, jc);
                    }
                }
            }
        }

        if (!jumpMoves.empty()) return jumpMoves;

        // Обычные ходы только вперед
        for (int dc : {-1, 1}) {
            int nr = row + forward;
            int nc = col + dc;
            if (isInside(nr, nc) && !board[nr][nc]) {
                moves.emplace_back(nr, nc);
            }
        }
    }
    // Дамки
    else {
        // Для дамок: сначала проверяем все возможные прыжки
        std::vector<std::pair<int, int>> jumpMoves;

        // Поиск прыжков с захватом вражеских шашек
        for (int dr : {-1, 1}) {
            for (int dc : {-1, 1}) {
                bool hasEnemy = false;
                int enemyR = -1, enemyC = -1;

                for (int step = 1; step < SIZE; ++step) {
                    int nr = row + dr * step;
                    int nc = col + dc * step;

                    if (!isInside(nr, nc)) break;

                    if (board[nr][nc]) {
                        if (board[nr][nc]->isWhite() != isWhite && !hasEnemy) {
                            hasEnemy = true;
                            enemyR = nr;
                            enemyC = nc;
                        }
                        else {
                            break; // Своя шашка или второй враг
                        }
                    }
                    else if (hasEnemy) {
                        // Добавляем прыжок, если после врага есть свободное место
                        jumpMoves.emplace_back(nr, nc);
                    }
                }
            }
        }

        // Если есть прыжки - возвращаем только их
        if (!jumpMoves.empty()) {
            return jumpMoves;
        }

        // Если прыжков нет - возвращаем все обычные ходы
        for (int dr : {-1, 1}) {
            for (int dc : {-1, 1}) {
                for (int step = 1; step < SIZE; ++step) {
                    int nr = row + dr * step;
                    int nc = col + dc * step;
                    if (!isInside(nr, nc) || board[nr][nc]) break;
                    moves.emplace_back(nr, nc);
                }
            }
        }
    }

    return moves;
}

void CheckersBoard::clearHighlights() {
    for (auto* o : highlights) delete o;
    highlights.clear();
}

void CheckersBoard::render(Shader& shader) {
    // Сначала рисуем все элементы доски
    for (int r = 0; r < SIZE; ++r)
        for (int c = 0; c < SIZE; ++c)
            if (board[r][c])
                board[r][c]->model.Draw(shader);

    for (auto* h : highlights)
        h->model.Draw(shader);

    // Затем рисуем текст поверх всего
    if (gameState != PLAYING) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        string winText;

        if (gameState == WHITE_WIN) winText = "White win";
        else winText = "Black win";

        font->RenderText(
            winText,
            100.0f, 100.0f, 1.0f,
            { 1.0f, 1.0f, 0.0f },
            textProjection,
            *shaderFont);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
}

bool CheckersBoard::checkPath(int r1, int c1, int r2, int c2) const {
    int dr = (r2 > r1) ? 1 : -1;
    int dc = (c2 > c1) ? 1 : -1;
    int steps = abs(r2 - r1);
    int enemyCount = 0;

    for (int i = 1; i < steps; ++i) {
        int curR = r1 + dr * i;
        int curC = c1 + dc * i;
        
        if (board[curR][curC]) {
            if (board[curR][curC]->isWhite() == board[r1][c1]->isWhite()) 
                return false;
            
            enemyCount++;
            if (enemyCount > 1) return false;
        }
    }
    return true;
}
